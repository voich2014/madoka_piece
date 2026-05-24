//---------------------------------------------------------------------------
// P/ECE ヴォイスチェンジャー
// 2003/09/17 by まどか
//
// H    P:http://www2.plala.or.jp/madoka/
// E-mail:madoka@olive.plala.or.jp
//
// エコーボイス＆サウンド出力部分はP/ECE HandBook Vol.2の記事を参考にさせて
// 頂きました。貴重な情報ありがとうございますm(-_-)m
//
//タブは4で見てください
//---------------------------------------------------------------------------
//■■ ファイルインクルード ■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
#include <s1c33cpu.h>
#include <string.h>
#include <piece.h>

//---------------------------------------------------------------------------
//■■ 定数定義 ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
#define ADI_VECTER		64				//A/D変換終了割り込みのベクタ番号
#define P16T3_VECTER	42				//16bitタイマー3コンペアB割り込みのベクタ番号

#define FC88KHZCT		68				//88.2KHzのカウンタ数
										//分周比が4なので、1カウンタは24/4 = 6MHz
										//の1クロック = 0.1667μsec.
										//88.2KHzは1秒間に88200回の周期なので、
										//1 / 88200 = 0.00001133sec. = 11.33μsec.
										//22.68 / 0.1667 = 67.6 = 68

#define OSIRO_SAMPLE	100				//簡易オシロのサンプリング回数

#define VOLUME_MAX		5				//最大ボリューム
#define VOLUME_MIN		0				//最小ボリューム

#define AVE_MAX			4				//音声サンプリングの平均化個数(平均化はノイズ対策)

#define ECHOBUFF_MAX	(0x3FFF)		//エコーバッファのサイズ
#define PITCHBUFF_MAX	(0x3FFF)		//ピッチ変換バッファのサイズ
#define PITCHOUTSTART	(0x0000)		//ピッチ変換の出力スタートインデックス
#define PITCHOUTCOUNT	(650)			//ピッチ変換期間(この数値を変えると声質が変わる)

enum eMode
{
	eNormalVoice = 0,	//ノーマルボイスモード
	eOsiro,				//簡易オシロモード
	eEchoVoice,			//エコーボイスモード
	ePitchVoice,		//ボイスピッチ変換モード
	eModeNum
};

//---------------------------------------------------------------------------
//■■ レジスタアクセス定義 ■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
//各レジスタへのアドレス

//A/D変換関連
//CPUデータシートP.401～P.402 参照
#define CFK6	(*(volatile unsigned char*)0x402C3)		//K6機能選択レジスタ
#define PSAD	(*(volatile unsigned char*)0x4014F)		//A/Dクロックコントロールレジスタ(PSONAD含む)
#define ADCS	(*(volatile unsigned char*)0x40243)		//A/Dチャネルレジスタ
#define ADTR	(*(volatile unsigned char*)0x40242)		//A/Dトリガレジスタ
#define FADE	(*(volatile unsigned char*)0x40287)		//割り込み要因フラグレジスタ
#define EADE	(*(volatile unsigned char*)0x40277)		//割り込みイネーブルレジスタ
#define ADER	(*(volatile unsigned char*)0x40244)		//A/Dイネーブルレジスタ
#define ADDL	(*(volatile unsigned char*)0x40240)		//A/D変換結果(下位)レジスタ
#define ADDH	(*(volatile unsigned char*)0x40241)		//A/D変換結果(上位)レジスタ

//16bitプログラマブルタイマー1(サウンド出力用)関連
//CPUデータシート P.272 P.280～P.293 P371 参照
#define P16T1C	(*(volatile unsigned char*)0x4818E)		//16bitタイマ1制御レジスタ
#define PST1CC	(*(volatile unsigned char*)0x40148)		//16bitタイマ1クロックコントロールレジスタ
#define CR1A	(*(volatile unsigned short*)0x48188)	//16bitタイマ1コンペアデータA設定レジスタ
#define CR1B	(*(volatile unsigned short*)0x4818A)	//16bitタイマ1コンペアデータB設定レジスタ
#define CFP2	(*(volatile unsigned char*)0x402D8)		//P2機能選択レジスタ
#define P3DR	(*(volatile unsigned char*)0x402DD)		//P3入出力兼用ポートデータレジスタ

//16bitプログラマブルタイマー3関連
//CPUデータシート P.272 P.280～P.293 参照
#define P16T3C	(*(volatile unsigned char*)0x4819E)		//16bitタイマ3制御レジスタ
#define PST3CC	(*(volatile unsigned char*)0x4014A)		//16bitタイマ3クロックコントロールレジスタ
#define CR3B	(*(volatile unsigned short*)0x4819A)	//16bitタイマ3コンペアデータB設定レジスタ
#define E16T3	(*(volatile unsigned char*)0x40273)		//16bitタイマ2/3割り込みイネーブルレジスタ
#define F16T3	(*(volatile unsigned char*)0x40283)		//16bitタイマ2/3割り込み要因フラグレジスタ

//---------------------------------------------------------------------------
//■■ グローバル変数定義 ■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
unsigned char g_aVbuff[128*88];							//仮想VRAM

//volatile unsigned short g_wVoltage = 0;					//A/D変換で計測した電圧
volatile unsigned short g_nSamplingCount = 0;			//サンプリング回数
volatile unsigned short g_aSampleData[OSIRO_SAMPLE];	//サンプリングしたデータ

PCETPENT pOldP16T3B_interrupt = NULL;					//元の16bitタイマー3
														//コンペアB割り込み関数

int g_nState = eNormalVoice;							//現在のモード

int g_nVolume = 1;										//出力ボリューム

unsigned short g_wOldADdata[AVE_MAX];					//平均化用バッファ

int g_nSndbuff[ECHOBUFF_MAX+1];							//エコー用サウンドバッファ
int g_nSndAdr0,g_nSndAdr1,g_nSndAdr2,g_nSndAdr3;		//エコー参照インデックス

int g_nPitchMode = 0;									//ピッチ変換モード(0:High 1:Normal 2:Low)
int g_nPitchBuff[PITCHBUFF_MAX+1];						//ピッチ変換用バッファ
int g_nPitchOutIndex = PITCHOUTSTART;					//ピッチ変換出力インデックス
int g_nPitchInIndex = 0;								//ピッチ変換用サンプリングインデックス
int g_nPitchOutStartIndex = (PITCHOUTSTART - PITCHOUTCOUNT) & PITCHBUFF_MAX;	//ピッチ変換補正用データインデックス
int g_nPitchOutCount = 0;								//補正タイミングカウンタ
unsigned short g_wOldPitchOut = 0;						//ノイズ対策用バッファ

//---------------------------------------------------------------------------
//■■ 関数のプロトタイプ宣言 ■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
//16bitタイマー3コンペアB割り込み関数
static void P16T3B_interrupt(void);					

//簡易オシロサンプリング処理
void OsiroInterrupt(unsigned short wADdata);		

//ノーマルボイス出力処理
void NomalVoiceInterrupt(unsigned short wADdata);	

//エコーボイス出力処理
void EchoVoiceInterrupt(unsigned short wADdata);	

//ピッチ変換ボイス出力処理
void PitchVoiceInterrupt(unsigned short wADdata);	

//---------------------------------------------------------------------------
//■■ A/D変換関連 ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void StartAD(void)
{

	//A/D変換の開始

	/* 今回ADI割り込みは使用しません
	//割り込み許可
	EADE |= 0x01;

	//割り込み要因フラグクリア
	FADE &= ~(0x01);
	*/

	//A/D変換許可・変換開始
	ADER = 0x06;

}
//---------------------------------------------------------------------------
void StopAD(void)
{

	//A/D変換の停止
	
	//A/D変換禁止・変換停止
	ADER = 0;

	//割り込み禁止
	EADE &= ~(0x01);
	
	//割り込み要因フラグクリア
	FADE &= ~(0x01);
	
}
//---------------------------------------------------------------------------
/* 今回ADI割り込みは使用しません ============================================
static void MyADInterrupt(void)
{
	
	//A/D変換終了割り込み

	INT_BEGIN2;
	{

		//A/D変換の結果を取得
		unsigned short wADdata;

		//上位から取得
		wADdata = ADDH << 8;
		
		//下位も取得
		wADdata |= ADDL;

		//変換値から電圧に換算
		//(変換値は1が3.3/1023[V]になります)
		g_wVoltage = (unsigned short)((330 * wADdata + 512) >> 10);

		//割り込み要因フラグクリア
		FADE &= ~(0x01);

		//A/D変換再スタート
		StartAD();
		
	}
	INT_END2;

}
//---------------------------------------------------------------------------
===========================================================================*/
void InitAD(void)
{

	//A/D変換関連の初期化処理

	int i;

DISABLE;	//割り込み禁止
	
	//現在のA/D変換処理を無効にする

	//A/D変換停止
	StopAD();

	//電源電圧測定禁止
	pceTimerSetCallback(5,PCE_TT_NONE,0,NULL);

	//電源電圧表示OFF
	pcePowerSetReport(PWR_RPTOFF);

	/* 今回ADI割り込みは使用しません
	//自分のA/D変換割り込み関数をセット
	pceVectorSetTrap(ADI_VECTER,MyADInterrupt);
	*/


	//新規にA/Dコンバータの設定をする
	
	/*=== １．アナログ入力端子の設定 ===*/

	//K67をAD7として機能選択する
	CFK6 |= (1 << 7);

	/*=== ２．入力クロックの設定 ===*/

	//A/D変換器の入力クロックの分周比を設定
	PSAD &= 0xF0;		//下位4bitをゼロクリア
						//(クロック制御部分もクリア)

	PSAD |= 0x03;		//24MHz / 16 = 1.5MHzを設定

	//A/D変換のクロック制御をON
	PSAD |= 0x08;		//↑でPSAD |= 0x0Bとしても良い

	/*=== ３．アナログ変換開始チャネル/終了チャネルの選択 ===*/

	//A/D変換するチャネルをAD7に設定
	ADCS = 0x3F;		//開始チャネル(0～2bit)と
						//終了チャネル(3～4bit)を設定
						//0x3F = &B00111111

	/*=== ４．A/D変換モードの設定 ＆ ５．トリガの選択 ===*/
	
	//ソフトウェアでA/D変換の
	//スタートトリガを指定する
	ADTR &= 0xC7;		//ゼロクリア &B11000111
						//通常モード/ソフトウェアトリガを選択

	//ノイズ対策　平均化用バッファの初期化
	for(i = 0;i < AVE_MAX;++i)
		g_wOldADdata[i] = 0;

ENABLE;		//割り込み許可
	
}
//---------------------------------------------------------------------------
void EndAD(void)
{
	
	//A/D変換の終了処理

	//A/D変換の停止
	StopAD();

	//リセットしないと電源電圧測定関連の設定が
	//元に戻らないので、リセットさせます。
DISABLE;	//割り込み禁止

		asm("xld.w	%r4,0xC00000");
		asm("jp		%r4");

ENABLE;		//割り込み許可

}
//---------------------------------------------------------------------------
//■■ 16bitタイマー3関連 ■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void StartTimer(void)
{

	//16bitタイマ3の開始

	//割り込み許可
	E16T3 |= (1 << 6);		//タイマ3コンペアB割り込みを許可

	//割り込み要因フラグクリア
	F16T3 &= ~(1 << 6);		//タイマ3コンペアB割り込み要因をクリア

	//タイマのカウントデータをリセット
	P16T3C |= (1 << 1);		//0x02でも良いんですけど(^^;

	//タイマー開始
	P16T3C |= 0x01;			//D0bitを1にする
	
}
//---------------------------------------------------------------------------
void StopTimer(void)
{

	//16bitタイマ３の停止

	//タイマー停止
	P16T3C &= 0xFE;		//D0bitを0にする

	//割り込み禁止
	E16T3 &= ~(1 << 6);	//タイマ3コンペアB割り込みを禁止

}
//---------------------------------------------------------------------------
unsigned short GetADdata(void)
{
	
	//A/D変換結果の取得

	unsigned short wADdata;	//A/D変換結果
	unsigned long ulADdata;
	int i;

	//念のためA/D変換停止
	StopAD();

	//A/D変換開始
	StartAD();

	//A/D変換が終わるのを待つ
	//ADERのD3bitが1になったら終了
	while(!(ADER & 0x08));

	//A/D変換の結果を取得

	//上位から取得
	wADdata = ADDH << 8;
	
	//下位も取得
	wADdata |= ADDL;

	//平均化用バッファに追加
	g_wOldADdata[0] = wADdata;

	//平均化処理
	for(i = 0,ulADdata = 0;i < AVE_MAX;++i)
		ulADdata += g_wOldADdata[i];
	wADdata = (ulADdata + (AVE_MAX / 2)) / AVE_MAX;

	//平均化用バッファをずらして次に備える
	for(i = AVE_MAX - 1;i > 0;--i)
		g_wOldADdata[i] = g_wOldADdata[i - 1];

	return wADdata;

}
//---------------------------------------------------------------------------
static void P16T3B_interrupt(void)
{

	//16bitタイマー3コンペアB割り込み関数
	
	INT_BEGIN2;
	{
				
		//モードで処理を分ける
		switch(g_nState)
		{
		
			case eNormalVoice:	//ノーマルボイスモード
				NomalVoiceInterrupt(GetADdata());
				break;

			case eOsiro:		//簡易オシロモード
				OsiroInterrupt(GetADdata());
				break;

			case eEchoVoice:	//エコーボイスモード
				EchoVoiceInterrupt(GetADdata());
				break;

			case ePitchVoice:	//ボイスピッチ変換モード
				PitchVoiceInterrupt(GetADdata());
				break;

		}
		
		//割り込み要因フラグクリア
		F16T3 &= ~(1 << 6);	//タイマ3コンペアB割り込み要因をクリア
		
	}
	INT_END2;

}
//---------------------------------------------------------------------------
void InitP16Timer3(void)
{

	//16bitプログラマブルタイマ３関連の初期化

DISABLE;	//割り込み禁止
	
	//タイマー停止
	StopTimer();

	//自分の16bitタイマー3コンペアB割り込み関数をセット
	pOldP16T3B_interrupt = pceVectorSetTrap(P16T3_VECTER,P16T3B_interrupt);

	//入力クロックの設定

	//内部クロック選択・通常モード・クロック出力OFF
	P16T3C &= 0xCD;	//0xCD = &B11001101

	//分周比を4に設定 (24MHz / 4)
	//ついでにクロック制御もONにします
	PST3CC = 0x0A;	//0x0A = &B00001010

	//コンペアレジスタバッファを使用
	P16T3C |= (1 << 5);

	//比較値をコンペアデータBに設定
	//今回は88.2KHz16bitモノラルで音声を出力するので
	//タイマーの周期を88.2KHzに設定します
	//(これ以上は上手くいかないようです)
	CR3B = FC88KHZCT;		//0～65535

ENABLE;		//割り込み許可

}
//---------------------------------------------------------------------------
void EndTimer(void)
{
	
	//16bitタイマ3の終了処理

	//タイマ停止
	StopTimer();

DISABLE;	//割り込み禁止

	//元の16bitタイマー3コンペアB割り込み関数をセット
	pceVectorSetTrap(P16T3_VECTER,pOldP16T3B_interrupt);

ENABLE;		//割り込み許可

}
//---------------------------------------------------------------------------
//■■ サウンド出力関連 ■■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void InitSoundOutTimer(void)
{

DISABLE;	//割り込み禁止

	//サウンド出力タイマー停止
	P16T1C = 0;

	//P2機能選択レジスタでP23をタイマのクロック出力に設定
	CFP2 |= (1 << 3);

	//サウンド出力タイマの分周比を1に設定
	//ついでにクロック制御もONにします
	PST1CC = (1 << 3);

	//コンペアデータをセット
	CR1A = 0;
	CR1B = 0x200;

	//以下P/ECE HandBook Vol.2より抜粋
	/* 16bitタイマ1を起動します
	 bit0: 起動/停止		-> 1(起動する)
	 bit1: リセット			-> 1(リセットする。0の書き込みは無効)
	 bit2: クロック出力		-> 1(出力する)
	 bit3: 入力クロック 	-> 0(内部クロック)
	 bit4: 出力反転			-> 0(反転しない ※1)
	 bit5: コンペアバッファ	-> 1(使用する)
	 bit6: ファインモード	-> 0(通常モード ※2)
	 bit7: なし
	
	 ※1 P/ECEのサウンドドライバは出力反転に設定していますが、
　	     ここではまず出力反転なしで実験してみることにします。
	 ※2 P/ECEのサウンドドライバはファインモードを使用していますが、
	     ここではまず基本的な通常モードで実験してみることにします。
	*/
	P16T1C = 0x27;

	//P31オーディオパワーをONに(0:ON 1:OFF)
	P3DR &= ~(1 << 1);

ENABLE;		//割り込み許可

}
//---------------------------------------------------------------------------
//■■ ノーマルボイスモード関連 ■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void InitNormalVoiceMode(void)
{
	
	//ノーマルボイスモードの初期化処理

	//タイマー停止
	StopTimer();

	//仮想画面クリア
	memset(g_aVbuff,0,128 * 88);

	//タイトル文字を表示
	pceFontSetPos(15,10);
	pceFontPrintf("ノーマルボイスモード");
	pceFontSetPos(15,20);
	pceFontPrintf("A:Vol.UP B:Vol.DOWN");
	
	pceFontSetPos(5,60);
	pceFontPrintf("このボリュームはオシロの");
	pceFontSetPos(5,70);
	pceFontPrintf("表示波形にも影響します");
	
	//タイマー開始
	StartTimer();

}
//---------------------------------------------------------------------------
void NomalVoiceInterrupt(unsigned short wADdata)
{
	
	//ノーマルボイス出力処理

	int nData;
	
	//波形データを取得
	nData = (int)wADdata - 512;

	//音量調整
	nData *= g_nVolume;
	if(nData > 511) nData = 511;
	if(nData < -512) nData = -512;
	
	//出力
	CR1A = (unsigned short)nData + 512;

}
//---------------------------------------------------------------------------
void VolumeControl(unsigned long pad)
{
	
	//ボリュームコントロール
	
	//AボタンでVol.UP
	if(pad & TRG_A)
	{
		g_nVolume++;

		if(g_nVolume > VOLUME_MAX)
			g_nVolume = VOLUME_MAX;
	}
	
	//BボタンでVol.DOWN
	if(pad & TRG_B)
	{
		g_nVolume--;

		if(g_nVolume < VOLUME_MIN)
			g_nVolume = VOLUME_MIN;
	}

}
//---------------------------------------------------------------------------
void NormalVoiceMode(unsigned long pad)
{

	//ノーマルボイスモードの処理

	//ボリューム調整
	VolumeControl(pad);

	//現在のボリュームを表示
	pceFontSetPos(20,40);
	pceFontPrintf("Vol.:%2d",g_nVolume);
	
}
//---------------------------------------------------------------------------
//■■ 簡易オシロモード関連 ■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void InitOsiroMode(void)
{

	//簡易オシロモードの初期化処理

	//タイマー停止
	StopTimer();

	//仮想画面クリア
	memset(g_aVbuff,0,128 * 88);

	//タイトル文字を表示
	pceFontSetPos(0,0);
	pceFontPrintf("簡易オシロスコープテスト");
	pceFontSetPos(0,10);
	pceFontPrintf("Aボタンでサンプリング開始");

}
//---------------------------------------------------------------------------
void OsiroInterrupt(unsigned short wADdata)
{
	
	//簡易オシロサンプリング処理

	//データを保存
	g_aSampleData[g_nSamplingCount] = wADdata;

	//サンプリングカウンタ更新
	g_nSamplingCount++;

	//指定の回数サンプリングしたらタイマー終了
	if(g_nSamplingCount == OSIRO_SAMPLE)
		StopTimer();

}
//---------------------------------------------------------------------------
void DrawGraphLine(void)
{

	//グラフの基準線を描画

	//基準値を描画
	pceFontSetPos(0,22);
	pceFontPrintf("3.3V");
	
	pceFontSetPos(0,52);
	pceFontPrintf("1.65V");
	
	pceFontSetPos(0,77);
	pceFontPrintf("0V");
	
	//基準線を描画
	pceLCDLine(3,5,27,127,27);
	pceLCDLine(3,5,57,127,57);
	pceLCDLine(3,5,87,127,87);

}
//---------------------------------------------------------------------------
void DrawGraph(void)
{
	
	//サンプリングデータの表示
	//(サンプリングデータを17.0667分の1にして表示します。

	int i;
	int Left = (128 - OSIRO_SAMPLE) / 2;
	int nData;

	//表示領域をクリア
	pceLCDPaint(0,Left,27,Left + OSIRO_SAMPLE,60);

	//基準線を描画
	DrawGraphLine();

	//グラフ表示
	for(i = 0;i < OSIRO_SAMPLE;++i)
	{

		nData = (int)g_aSampleData[i];
				
		//音量調整
		nData -= 512;
		nData *= g_nVolume;
		if(nData > 511) nData = 511;
		if(nData < -512) nData = -512;
	
		if(nData >= 0)
		{	
			//プラスの波形
		
			//1サンプリングデータの表示
			pceLCDLine(3,Left + i,57,Left + i,
					   57 - (int)((double)nData / 17.0667));
		}
		else
		{
			//マイナスの波形

			//1サンプリングデータの表示
			pceLCDLine(3,Left + i,57,Left + i,
					   57 - (int)((double)nData / 17.0667));
		}

	}
	
}
//---------------------------------------------------------------------------
void OsiroMode(unsigned long pad)
{

	//簡易オシロモードの処理

	//Aボタンでサンプリング
	if(pad & TRG_A)
	{

		//サンプリングカウンタクリア
		g_nSamplingCount = 0;

		//サンプリングタイマスタート
		StartTimer();

		//指定の回数サンプリングするのを待つ
		while(g_nSamplingCount < OSIRO_SAMPLE);

		//タイマ停止
		StopTimer();

		//サンプリングデータを表示
		DrawGraph();

	}

}
//---------------------------------------------------------------------------
//■■ エコーボイスモード関連 ■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void InitEchoVoiceMode(void)
{
	
	//エコーボイスモードの初期化処理

	//タイマー停止
	StopTimer();

	//仮想画面クリア
	memset(g_aVbuff,0,128 * 88);

	//タイトル文字を表示
	pceFontSetPos(15,10);
	pceFontPrintf("エコーボイスモード");
	pceFontSetPos(15,20);
	pceFontPrintf("A:Vol.UP B:Vol.DOWN");
	
	pceFontSetPos(5,60);
	pceFontPrintf("このボリュームはオシロの");
	pceFontSetPos(5,70);
	pceFontPrintf("表示波形にも影響します");

	//エコー参照インデックスを初期化
	g_nSndAdr0 = 0x0000;
	g_nSndAdr1 = ECHOBUFF_MAX - 0x1500;
	g_nSndAdr2 = ECHOBUFF_MAX - 0x0B00;
	g_nSndAdr3 = ECHOBUFF_MAX - 0x0100;

	//タイマー開始
	StartTimer();

}
//---------------------------------------------------------------------------
void EchoVoiceInterrupt(unsigned short wADdata)
{
	
	//エコーボイス出力処理

	int nData;
	
	//波形データを取得
	nData = (int)wADdata - 512;

	//音量調整
	nData *= g_nVolume;
	if(nData > 511) nData = 511;
	if(nData < -512) nData = -512;
	
	//エコー用サウンドバッファに保存
	g_nSndbuff[g_nSndAdr0] = nData;

	//エコー参照インデックスを更新
	g_nSndAdr0 = (g_nSndAdr0 + 1) & ECHOBUFF_MAX;
	g_nSndAdr1 = (g_nSndAdr1 + 1) & ECHOBUFF_MAX;
	g_nSndAdr2 = (g_nSndAdr2 + 1) & ECHOBUFF_MAX;
	g_nSndAdr3 = (g_nSndAdr3 + 1) & ECHOBUFF_MAX;

	//出力
	CR1A = (unsigned short)(((g_nSndbuff[g_nSndAdr1] / 7) + 
							 (g_nSndbuff[g_nSndAdr2] / 3) +
						 	  g_nSndbuff[g_nSndAdr3]) / 2 + 512); 

}
//---------------------------------------------------------------------------
void EchoVoiceMode(unsigned long pad)
{

	//エコーボイスモードの処理

	//ボリューム調整
	VolumeControl(pad);

	//現在のボリュームを表示
	pceFontSetPos(20,40);
	pceFontPrintf("Vol.:%2d",g_nVolume);
	
}
//---------------------------------------------------------------------------
//■■ ピッチ変換ボイスモード関連 ■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void InitPitchVoiceMode(void)
{
	
	//ピッチ変換ボイスモードの初期化処理

	//タイマー停止
	StopTimer();

	//仮想画面クリア
	memset(g_aVbuff,0,128 * 88);

	//タイトル文字を表示
	pceFontSetPos(10,10);
	pceFontPrintf("ピッチ変換ボイスモード");
	pceFontSetPos(15,20);
	pceFontPrintf("A:Vol.UP B:Vol.DOWN");
	pceFontSetPos(12,30);
	pceFontPrintf("Pitch ↑:High ↓:Low");
	
	//タイマー開始
	StartTimer();

}
//---------------------------------------------------------------------------
void HighPitchVoiceOut(void)
{

	//周波数を2倍にして高い声で出力

	unsigned short wNow;

	//以下のピッチ変換出力アルゴリズムは試行錯誤の上見つけ出したもの
	//なので、必ずしも正しいとは言えないかもしれませんが、一応リアル
	//タイムでピッチ変換できているようです(^^;	
	if(g_nPitchOutCount < PITCHOUTCOUNT / 2)
	{
		//ピッチ変換期間の前半は以前のデータから音声データを補充
		//こうすることで再生時間を通常と同じにする
		
		//新しい音声データを作成
		wNow = (unsigned short)(g_nPitchBuff[(g_nPitchOutStartIndex + g_nPitchOutCount * 2)
							    & PITCHBUFF_MAX] + 512);
		
		//前回のデータとの平均を出力することでノイズを軽減する
		CR1A = (g_wOldPitchOut + wNow) / 2;		
		g_wOldPitchOut = wNow;
	}
	else
	{
		//後半はピッチを縮めた音声データを出力

		//新しい音声データを作成
		wNow = (unsigned short)(g_nPitchBuff[g_nPitchOutIndex] + 512);

		//前回のデータとの平均を出力することでノイズを軽減する
		CR1A = (g_wOldPitchOut + wNow) / 2;		
		g_wOldPitchOut = wNow;

		g_nPitchOutIndex = (g_nPitchOutIndex + 2) & PITCHBUFF_MAX;						 
	}

	//カウンタ更新
	g_nPitchOutCount++;
	g_nPitchOutCount %= PITCHOUTCOUNT;
	
	if(g_nPitchOutCount == 0)
		g_nPitchOutStartIndex = (g_nPitchOutStartIndex + PITCHOUTCOUNT) & PITCHBUFF_MAX;

}
//---------------------------------------------------------------------------
void LowPitchVoiceOut(void)
{

	//周波数を2分の1にして低い声で出力

	unsigned short wNow;

	//以下のピッチ変換出力アルゴリズムは試行錯誤の上見つけ出したもの
	//なので、必ずしも正しいとは言えないかもしれませんが、一応リアル
	//タイムでピッチ変換できているようです(^^;	
	
	//新しい音声データを作成
	wNow = (unsigned short)(g_nPitchBuff[g_nPitchOutStartIndex] + 512);
	
	//前回のデータとの平均を出力することでノイズを軽減する
	CR1A = (g_wOldPitchOut + wNow) / 2;
	g_wOldPitchOut = wNow;

	//カウンタ更新
	g_nPitchOutIndex = (g_nPitchOutIndex + 1) & PITCHBUFF_MAX;						 
	if(g_nPitchOutCount & 1)	//2回に1回実行
		g_nPitchOutStartIndex = (g_nPitchOutStartIndex + 1) & PITCHBUFF_MAX;

	g_nPitchOutCount++;
	g_nPitchOutCount %= PITCHOUTCOUNT;

	if(g_nPitchOutCount == 0)
		g_nPitchOutStartIndex = g_nPitchOutIndex;

}
//---------------------------------------------------------------------------
void PitchVoiceInterrupt(unsigned short wADdata)
{
	
	//ピッチ変換ボイス出力処理
	
	int nData;
	
	//波形データを取得
	nData = (int)wADdata - 512;

	//音量調整
	nData *= g_nVolume;
	if(nData > 511) nData = 511;
	if(nData < -512) nData = -512;
	
	//普通にサンプリングして保存
	g_nPitchBuff[g_nPitchInIndex] = nData;
	g_nPitchInIndex = (g_nPitchInIndex + 1) & PITCHBUFF_MAX;

	//モードによって出力方法を変える
	switch(g_nPitchMode)
	{
		case 0:	//高い声
		
			//高い声に変換
			HighPitchVoiceOut();

			break;

		case 1:	//通常の声
	
			//出力
			CR1A = (unsigned short)nData + 512;

			break;
	
		case 2:	//低い声
		
			//低い声に変換
			LowPitchVoiceOut();
		
			break;
	}

}	
//---------------------------------------------------------------------------
void PitchModeSelectAndDraw(unsigned long pad)
{

	//ピッチ変換モードの選択と表示
	static char aModeText[3][20] =
	{ 
      "周波数２倍モード　","通常の声　　　　　","周波数1/2倍モード　"
	};
	
	static int nOldMode = 0;	//前回のモード
	
	//十字PADの上下でモード選択
	if(pad & TRG_UP)
		g_nPitchMode--;
	if(pad & TRG_DN)
		g_nPitchMode++;
	
	if(g_nPitchMode < 0) g_nPitchMode = 0;
	if(g_nPitchMode > 2) g_nPitchMode = 2;

	//モードが変更されたか？
	if(nOldMode != g_nPitchMode)
	{
		//インデックスの初期化
		g_nPitchOutIndex = PITCHOUTSTART;
		g_nPitchInIndex  = 0;
		g_nPitchOutStartIndex = (PITCHOUTSTART - PITCHOUTCOUNT) & PITCHBUFF_MAX;
		g_nPitchOutCount = 0;
		g_wOldPitchOut = 0;

		//新しいモードを保存
		nOldMode = g_nPitchMode;
	}

	//現在のモード情報を描画
	pceFontSetPos(10,70);
	pceFontPrintf(aModeText[g_nPitchMode]);

}
//---------------------------------------------------------------------------
void PitchVoiceMode(unsigned long pad)
{

	//ピッチ変換ボイスモードの処理

	//ボリューム調整
	VolumeControl(pad);

	//現在のボリュームを表示
	pceFontSetPos(20,50);
	pceFontPrintf("Vol.:%2d",g_nVolume);
	
	//ピッチ変換モードの選択と表示
	PitchModeSelectAndDraw(pad);

}
//---------------------------------------------------------------------------
//■■ メインループ ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
//---------------------------------------------------------------------------
void pceAppInit(void)
{

	//液晶画面表示ストップ
	pceLCDDispStop();

	//仮想画面用ワークエリアのポインタを設定
	//（仮想画面を作成）
	pceLCDSetBuffer(g_aVbuff);

	//仮想画面初期化
	memset(g_aVbuff,0,128 * 88);

	//アプリの周期を指定します
	pceAppSetProcPeriod(16);

	//A/D変換関連の初期化
	InitAD();

	//16bitタイマ3関連の初期化
	InitP16Timer3();

	//サウンド出力関連の初期化
	InitSoundOutTimer();

	//表示文字の設定
	//半角5*10 全角10*10のフォントで
	//自動改行しない
	pceFontSetType(0x80);

	//モードを初期化
	g_nState = eNormalVoice;
	InitNormalVoiceMode();

	//液晶画面表示スタート
	pceLCDDispStart();

}
//---------------------------------------------------------------------------
void ModeSelect(void)
{

	//セレクトボタンでモード切替
	
	//次のモードへ
	g_nState++;
	if(g_nState == eModeNum) g_nState = eNormalVoice;

	//各モードでの初期化処理
	switch(g_nState)
	{
	
		case eNormalVoice:	//ノーマルボイスモード
			InitNormalVoiceMode();
			break;

		case eOsiro:		//簡易オシロモード
			InitOsiroMode();
			break;

		case eEchoVoice:	//エコーボイスモード
			InitEchoVoiceMode();
			break;

		case ePitchVoice:	//ボイスピッチ変換モード
			InitPitchVoiceMode();
			break;

	}

}
//---------------------------------------------------------------------------
void ModeFunc(unsigned long pad)
{
	
	//各モードの処理を実行
	
	switch(g_nState)
	{
	
		case eNormalVoice:	//ノーマルボイスモード
			NormalVoiceMode(pad);
			break;

		case eOsiro:		//簡易オシロモード
			OsiroMode(pad);
			break;

		case eEchoVoice:	//エコーボイスモード
			EchoVoiceMode(pad);
			break;

		case ePitchVoice:	//ボイスピッチ変換モード
			PitchVoiceMode(pad);
			break;

	}

}
//---------------------------------------------------------------------------
void pceAppProc( int cnt )
{

	//メインループの処理
	unsigned long pad = 0;
	
	//トリガ情報取得
	pad = pcePadGet();	
	
	//STARTボタンで終了
	if(pad & TRG_START)
		pceAppReqExit(0);

	//SELECTボタンでモード切替
	if(pad & TRG_SELECT)
		ModeSelect();
	
	//各モードの処理
	ModeFunc(pad);

	//画面更新
	pceLCDTrans();

}
//---------------------------------------------------------------------------
void pceAppExit(void)
{

	//アプリ終了時の処理

	//16bitタイマ3の終了処理
	EndTimer();

	//A/D変換の終了処理
	EndAD();	//←ここでリセットがかかります
	
}
//---------------------------------------------------------------------------
