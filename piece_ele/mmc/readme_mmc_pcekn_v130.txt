＜MMC対応改造カーネルの使用方法＞

どうも、こんにちは。
MMC(Multi Media Card)をどうしてもP/ECEから利用したくて改造カーネルを作ってしまっ
たまどかです。

今回のパッケージにはコンパイル済みのカーネルイメージとフォントイメージが含まれて
おり、以下に記述する手順でP/ECEにカーネルとフォントをインストールすることにより
P/ECEのプログラム上からMMC上のファイルに対してアクセスすることが可能になります。

ただし、カーネルのアップデートはいいかげんにやって失敗するとシステムを破壊する恐
れがあるので、このパッケージを利用して作業をする方は、すべて自己責任でお願い致し
ます。

また、このパッケージおよび関連情報を元にして行った作業で発生したいかなる障害・損
害に対して、制作者および株式会社アクアプラス様は一切の責任を負うことができません。

この作業はメーカー保証外のものですので、アクアプラス様に対して一切の問い合わせは
行わないようにして下さい。

問い合わせに関しては制作者であるまどか宛(madoka@olive.plala.or.jp)のみでお願い致
します。

<変更履歴>
2004/05/20   インターネット公開
Ver.1.21

2004/08/29   MMCライブラリのmmcFileReadSctで
Ver.1.22     mmcFileReadSct(&pfa, NULL, sct, 0);
             とした場合に、内部の代替バッファに何も読み込まれていなかった不具合
             を修正。
             make biosでアップデートしてください。
             スタートボタン長押しのメニューでVer.1.22になっていたらアップデート
             成功です。

2004/08/30   MMCライブラリがSDカードと初期化に時間がかかるMMCに対応しました。
Ver.1.23     確認したSDカードは8MB & 32MBです。32MB以下のメモリーカードを使用す
             る場合は下の方にある「容量の小さいメモリーカードを利用するには」を
             ご参照ください。
             今回の対応に関してnsawaさん(http://piece.no-ip.org/)に情報提供して
             頂きました。ありがとうございますm(-_-)m

2004/09/26   updateフォルダ内のpcekn.imgがVer.1.21のままになっていたのをVer.1.23
             のものに入れ替えました。
             情報提供ヅラChuさん(http://zurachu.net/)

2004/10/25   フォント再配置のmake font2を実行した後にハングアップしてしまう不具合
Ver.1.24     を修正。ku.srfのリセットし忘れが原因の模様。
             また、フォントの再配置も10x10全角フォントの最後が崩れないように調整
             しました。
             今回の対応に関してnsawaさん(http://piece.no-ip.org/)に情報提供して
             頂きました。ありがとうございますm(-_-)m
             ご報告内容はこちらです→http://piece.no-ip.org/mmc123-20041026.html

2004/11/02   nsawaさん(http://piece.no-ip.org/)の御要望によりAPIにmmcReadSector
Ver.1.25     を追加。

2004/11/06   nsawaさん(http://piece.no-ip.org/)からの情報提供により、スーパー
Ver.1.26     フロッピー形式に対応しました。
             また、動作には全く関係ありませんが、少々ソースを修正しました。
             詳しくはnsawaさんのご報告内容をご参照下さい。
             http://piece.no-ip.org/mmc125-20041104.html

2005/03/20   nsawaさんの「MMC/SDカードリーダーライター(2004/12/14)」のMMCライブ
Ver.1.27     ラリのソースをベースに、アプリ起動前にカーネル側でMMCを初期化し、
(編集中)     pceFile系APIをフックするように変更しました。
             そして、512MB以上1GB以下のメモリカードに対応しました!(^^
             カーネル側でのMMC初期化はpceAppNotifyのAPPNF_INITMMC問い合わせにて
             返答を返すことにより、抑制・実行を制御できます。
             また、カーネル側のMMC初期化の結果はmmcGetInitResultにより取得できます。

2005/04/02   システムメニューにでカーネル側でMMCを初期化するかどうかの設定項目
Ver.1.27     (MMC/SD利用ON/OFFフラグ)を追加。

2005/04/03   MMCライブラリのソースをちょっと整理しました。内容的には変わりはあ
Ver.1.27     りません。

2005/04/05   mmcFileReadSctでptr=NULL,len=0がまた正常に動かなくなってしまっていた
Ver.1.28     不具合を修正しました。
             この修正に伴ない、mmcFileReadMMCSctに切り詰め処理も追加。

2005/04/24   ファイル検索時に扱える最大サイズぴったりのファイルがはじかれる条件式
Ver.1.29     だったのを修正。(情報提供:nsawaさん)
(編集中)

2005/05/01   システムメニュー終了時にmmcExitを呼んでしまっていた不具合を修正。
Ver.1.29     MMC機能を利用しているアプリの場合、これによりフックが解除されてしまい
             システムメニュー表示後に正しくMMCからファイルを読めなくなってしまって
             いました。

2005/05/04   MMC版MP3プレイヤーを5倍速対応にした際に更新したMMCライブラリにソースを
Ver.1.29b    統合しました。機能的には全く変わりはありませんので、Ver.1.29bとします。

2005/05/07   MMCライブラリを更新しました。MMC対応カーネルでは該当することはありませ
Ver.1.29c    んが、扱う最大ファイルサイズがカードの容量を超えている場合、最大ファイ
             ルサイズをカードの容量のサイズに切り詰めるように修正しました。
             これにより、64MBのカードで50MB以上のファイルが扱えなかった不具合が修正
             されたのですが、元々MMC対応カーネルでの最大ファイルサイズは256KBなので
             今回の修正により機能の変化はありません。よって、カーネルのヴァージョン
             もVer.1.29cということにします。

2005/07/09   今回のVer.UPもMMCライブラリの更新です。PMFプレイヤー専用のVer.UPと
Ver.1.30     mmcFileReadSctとmmcFileReadMMCSctでファイルサイズを超えてのアクセスを
             チェックする部分の処理が上手く動いていなかった不具合を修正しました。
             

●パッケージの構成●
+--+
   +--lib------------+				       ライブラリフォルダ
   |                 |                     (この中にmmc_api.libを含めます)
   |                 +--mmc_api_lib_src    mmc_api.lib作成用
   |
   +--menu_sample----+                     MMC制御APIを使ったメニューアプリサンプル
   |                 |                     (MMC上のpexを実行できます)
   |                 |
   |                 +--mmc_lib            MMC制御用のAPIを定義したヘッダーファイル
   |                                       (mmc_api.h)とライブラリファイル(mmc_api.lib)
   |                                       があります。MMC制御APIを利用する場合は、
   |                                       この2つのファイルをご利用ください。
   |
   +--mmc_kurnel_win_sln                   VC++.Net2003用プロジェクトフォルダ
   |                                       (別に使わなくても良いです(^^;)
   |
   +--resource-------+
   |                 +--font               フォント作成用データ
   |
   +--sysdev---------+
   |                 +--ku                 カーネルアップデート用アプリ
   |                 +--pcekn_mmc          MMC対応カーネル本体
   |
   +--update                               フォント・カーネルアップデート用



●MMC対応カーネルの使用方法●

今回作成したMMC対応カーネルをお手持ちのP/ECEにインストールするには、いちばん簡単
な方法として、P/ECEとUSBケーブルで接続された状態で、updateフォルダからのコマンド
ラインで、

(念の為)
>make clean	   ... やらなくてもいいです。
>make          ... やらなくてもいいです。


>make font1

とし、通常フォントの再配置を行います。
この時、P/ECE上に書き込みを表す表示が出るので、指示に従って下さい。

>make font2_3

として、8x16半角フォントと4x6半角フォントのセットも再インストールします。
そして最後に、

>make bios

として、MMC対応カーネルをP/ECEにインストール(アップデート)します。

以上の作業でMMC対応カーネルのインストールは完了です。
あとは、MMCソケット接続基板を作って、MMCを買ってくるだけです(^^;
ちなみに、MMCへの書き込みは市販のMMC対応のカードライターを利用してください。
また、買ってきたばかりのMMCでは使用できない場合がありますので、一度Windows上で
フォーマットしてから使用するようにして下さい。

現在動作確認済みカードの一覧はサポートページをご覧下さい。
http://www2.plala.or.jp/madoka/Piece_ele/mmc/mmc.htm


ソースを改変してアップデートさせる場合は、sysdev\pcekn_mmcフォルダ上のコマンド
ラインで

>update

とするのが楽です(^^
この場合、先にフォントの再配置が必要です。



さあ、これであなたのP/ECEも大容量外部メモリを手に入れることができました。
MMCに沢山のゲームやMP3を入れて、P/ECEをさらに楽しみましょう(^^
※拙作のMMC版MP3プレイヤーを使えば、MMC上のMP3(32kHz48kbps)を再生することができ
  ます。一部の曲では途中で終了してしまうことがあります。その場合32kHz48kbpsにす
  ると上手くいく場合があるようです。

それでは～

2005/05/07 追記
●MMC/SD機能を利用するには●
更新履歴に書いただけで、こっちの説明に書いていなかったので簡単に説明します(^^;

Ver.1.27からカーネル側でMMCを初期化し、pceFile系APIをフックして、自動でMMC側の
データファイルを読みに行けるMMC/SD機能が追加されました。

この機能により、MMC非対応のアプリ(巷に出回っているアプリのほとんどがそうですが)
が無改造でMMC対応になっちゃいます。
（モノによってはプログラムの組み方でそのままでは対応できないものもあるかもしれ
ませんが、多分大丈夫です。）

さて、肝心の使い方ですが、スタートボタン長押しで表示されるシステムメニューの
2ページ目で、MMC/SDという項目がありますので、それをONにしてメニューを終了して
ください。
これでMMC対応は完了です。簡単でしょ(^^
ちなみに、この情報は電源を切ると忘れてしまうので、注意してください。
また、カーネルから起動しない場合（PC側から直接SRAMに転送して実行する場合）には
このMMC/SD機能が効かないので、注意してください。

MMC/SD機能はMMC上から実行するアプリにも有効なので、アプリとデータを全てMMC/SD上
に置いて、セーブデータだけP/ECEのフラッシュメモリ上に保存するということも可能です。

それでは、MMCで広がった新たなP/ECEライフをお楽しみください(^^

2005/08/02 追記
重要なことを今の今まで書き忘れていました。
MMCカーネル及びMMCライブラリでのMMC制御機能は、現在データの読み出しにしか対応
していません。(読み出し及び検索に対応。書き込み、削除、新規作成は未対応です)

正直、カーネルの残り領域に書き込み側の処理が入りきらないかも知れないと言うことと、
書き込み処理は読み込み処理以上にやることが多くて面倒なので、現在P/ECEプログラム
からのMMC/SD上への書き込み動作はできないようになっています。
あらかじめ御了承ください。



2004/08/30 追記
●容量の小さいメモリーカードを利用するには●
32MBより小さいメモリーカードですと、普通にフォーマットするとFAT12としてフォー
マットされるみたいなので、以下の様にしてFAT16としてフォーマットしてください。

 <MMC or SDカードがXドライブにある場合>
 >FORMAT X: /FS:FAT /A:1024

フォーマットが終了するとFAT16でフォーマットできたなら、「16ビット : FATエントリ
」と表示され、FAT12でフォーマットされたなら「12ビット : FATエントリ」と表示され
ます。
この時、FAT16になっていることを確認してください。
 (情報提供:nsawaさん)


ただし、32MB以上のものでも新品のMMCではフォーマットが微妙に違うせいか、上手く読
めないことがあります。一度Windows側でフォーマットしてからファイルを書き込んでく
ださい。
（Win2000で確認）



===今回のMMC対応カーネル作成で再配布の対象となるオフィシャルファイル===
+lib
 cstart.o
 ctype.lib
 fp.lib
 fpp.lib
 idiv.lib
 io.lib
 lib.lib
 math.lib
 muslib.lib
 pceapi.lib
 simple.lib
 sprite.lib
 string.lib

+resource--font
 5x10rk.fnt
 ascii4x6.bin
 ascii4x6.bmp
 knj10.bdf
 knj10.fnt
 lfont16.bin
 lfont16.bmp
 makefile
 maru10.fnt
 mkfont.c
 mkfont.exe 
 mkfont.obj
 sfont6.bin
 sfont6.bmp
 tmp.bmp
 tmp3.bmp
 tmp4.bmp
 zfont10.bin

+sysdev--ku
 0ism.par
 boot.c
 boot.o
 crc32.c
 crc32.o
 dec.c
 doprnt.c
 doprnt.o
 fwr.c
 fwr.o
 inflate.c
 inflate.h
 inflate.o
 ku.map
 ku.srf
 ku.sym
 kumain.c
 kumain.h
 kumain.o
 makefile
 piecezl.h

+sysdev--pcekn_mmc
 0ism.par
 a.c
 a.cmd
 a.idm
 a.srf
 biapp.c
 biapp.o
 cap.bmp
 chap_9.c
 chap_9.h
 chap_9.o
 crc32.c
 crc32.o
 critical.c
 critical.o
 d12ci.c
 d12ci.h
 d12ci.o
 d12cifs.c
 d12cifs.o
 dbtbl.c
 dbtbl.o
 doprnt.c
 doprnt.o
 draw.c
 draw.o
 endmark.c
 endmark.o
 file.c
 file.o
 fmacc.c
 fmacc.o
 fmacc1.c
 fmacc1.o
 fmacc2.c
 fmacc2.o
 fmacc3.c
 fmacc3.o
 font.c
 font.o
 gfs.c
 gfs.o
 gfs1.o
 gfs1.old
 gfs1.s
 gfs2.o
 gfs2.s
 gfstbl.c
 gfstbl.o
 hard.c
 hard.o
 heapman.c
 heapman.o
 inflate.c
 inflate.h
 inflate.o
 irsub.c
 irsub.o
 lcd.c
 lcd.o
 mainloop.c
 mainloop.o
 makefile
 newmon.bat
 pad.c
 pad.o
 pcekn.c
 pcekn.cm
 pcekn.h
 pcekn.img
 pcekn.map
 pcekn.o
 pcekn.srf
 pcekn.sym
 pceusb.h
 pieceerr.h
 piecezl.h
 powerman.c
 powerman.o
 protodma.c
 protodma.h
 res.bat
 rtc.c
 rtc.o
 runapp.c
 runapp.o
 snd.c
 snd.o
 sndfast.o
 sndfast.s
 sndfast2.o
 sndfast2.s
 syserr.c
 syserr.o
 timer.c
 timer.o
 usb100.h
 usbcom.c
 usbcom.o
 vector.h
 w.cmd
 w2.cmd
 wb33.sav
 work.c
 work.h
 work.o
 
+update
 lfont16.img
 makefile
 mfont4.img
 pcekn.img
 zfont10.img


まどか
madoka@olive.plala.or.jp
http://www2.plala.or.jp/madoka/



