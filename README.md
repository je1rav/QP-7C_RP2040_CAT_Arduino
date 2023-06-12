# QP-7C_RP2040_CAT_Arduino
## QP-7C（CRkits共同購入プロジェクト）の改造 IV （QP-7C_RP2040のCAT対応 Arduino版）

 ”QP-7C（CRkits共同購入プロジェクト）の改造 III” (https://github.com/je1rav/QP-7C_RP2040_CAT/) と基本的には同じですが，Arduino IDEでUSB AudioとSerial通信の同時使用が一応できるようになりましたので，公開します．  
しかし，こちらも ”QP-7C（CRkits共同購入プロジェクト）の改造 III” (https://github.com/je1rav/QP-7C_RP2040_CAT/) とは違う問題があります．  
ハードウェアは，https://github.com/je1rav/QP-7C_RP2040/ をご参照ください．  

### 経緯
 ”QP-7C（CRkits共同購入プロジェクト）の改造 II” https://github.com/je1rav/QP-7C_RP2040/ では，pico用ボードマネジャー”Arduino Mbed OS RP2040 Boards”を使用し，Arduino IDEで開発しました．  
(もともとは，Seeed XIAO RP2040用のMbed対応ボードマネージャーを使用: このボードマネージャーは現在開発中止)  
従来，Arduino Mbed OSでは，USB AudioとUSB Seralの同時使用に問題があるようで，Windowsではどちらか一方しか使用できませんでした．  
”Arduino Core for mbed enabled devices”のgithubの，issue中”PluggableUSBAudio not working #546”でこのことが議論されています．  
実際に試したところ,MacOSでは同時使用も可能でした（多分Linuxも大丈夫）が，Windowsではダメでした．  

このため， ”QP-7C（CRkits共同購入プロジェクト）の改造 III”　https://github.com/je1rav/QP-7C_RP2040_CAT/ では，tinyusbをフルセットで使用できるpico-sdk上で開発し，一応作動するようになりました．  
Mbed coreのWindowsで動作しないファームウエアと，tinyusbを使ったWindowsで動作するファームウエアの，挙動の違いを”usbview.exe”を使って観察したところ，動作しないファームウエアにはUSBAudio用の”IAD descriptor”の記述が無いことが分かりました．  

この解消のため，Arduino Mbed OS上でUSB AudioとUSB Seralの同時使用を可能にする”PluggableUSBAudio.h”と”USBAudio.cpp”を少し修正してみると,
Windowsがデバイスを認識し，デバイスマネジャーにUSBSerialとUSBAudioが使用可能な状態で現れるようになりました．  
（”PluggableUSBAudio.h”と”USBAudio.cpp”はオリジナルのものから少し変更しています．）

当初，ファームウエアのプログラム本体は ”QP-7C（CRkits共同購入プロジェクト）の改造 II”にCAT部分を付け足せば，そのままで動くのでは無いかと期待しましたが，USBAudioの使い方の作法が少し違うことや，よく分からないエラーなどに遭遇し，そのままでは作動しなかったので，少し変更しています．  

### 使用上の問題点
PCと接続するとオーディオデバイスとして”RaspberryPi Pico”が現れますので，WSJT-Xのオーディオ入出力にお使いください．  
WSJT-Xと共に使用する場合しか検証していませんが，今回の場合”RaspberryPi Pico”のスピーカーを「既存のデバイス」に指定すると受信できませんので，「既存のデバイス」にしないでください．  
また， ”QP-7C（CRkits共同購入プロジェクト）の改造 III”とは違って，「このデバイスを聴く」にチェックをつけないでください．  
「このデバイスを聴く」にチェックをつけると受信できません．  

シリアルポートには新しい”USBシリアルデバイス”が現れますが，そのままではwindows上ではWSJT-XでのCATコントロールが動きませんでした．  
その原因について現時点では良く分かりませんが，以下の方法でPCを使ってリモート周波数変更が可能です．  
MacOS上では，問題なくWSJT-XでのCATコントロールが可能でした．  

#### Windows上でのCAT制御法 1
	Arduino IDEのシリアルモニタやターミナルソフトで通信可能です．  
	KENWOOD TS-2000のCATコマンドを送信することで，制御できます．  
	Python 3で作った簡素な周波数変更スプリプト”QP-7C_RP2040_freq.py”とそれをWindows用実行ファイルにした”QP-7C_RP2040_freq.exe”をアップしておきます．  
	リグのCOMポート番号の設定と目的周波数をkHz単位で入力して，実行するとリグの周波数が変更できます．  
	この場合，WSJT-XのCATコントロールは使用せず，こちらのソフトで周波数変更を行なって下さい．  
#### Windows上でのCAT制御法 2
	仮想シリアル(COM) ポートドライバ"com0com"を導入することで，WSJT-XでのCATコントロールを使用することができます．  
	"com0com"は2つの仮想のCOMポートを作成し，それらをクロスケーブルで接続したように見せかけるソフトです．  
	"com0com"をインストールして作成した２つのCOMポートの一方のポートをWSJT-XでのCATコントロール用に設定します．  
	もう一方のポートには，WSJT-XからのCATコントロール信号が行き来することになりますので，その信号をリグに転送するとCATコントロールが可能になります．  
	そのためには，"com0com"のもう一方のポートからリグのシリアルポートにシリアルデータを転送するソフトが必要で，それはPython 3で作りました．  
	そのシリアルデータ転送スプリプト”WSJT_X-RP2040.py”とそれをWindows用実行ファイルにした”WSJT_X-RP2040.exe”もアップしておきます．  
	このソフトでは　"com0com"のもう一つのCOMポート番号とリグのCOMポート番号を設定して，実行します．  
	(例えばcom0comでCOM11とCOM12ができたとすると，WSJT-Xの設定にCOM11を指定し，シリアルデータ転送ソフトにはCOM12を指定します．（逆でも可）)  
	WSJT-X使用中は，この”WSJT_X-RP2040.exe”をバックグラウンドで実行しておきます．  
	そうすることで，WSJT-XのCATデータが，"com0com"とこのシリアルデータ転送ソフト経由でリグに伝わることになります．  
	この経由によりCATコントロールが可能となる理由は良く分かりませんが，とにかく一応動いています．  

### ビルド上の問題
”QP-7C（CRkits共同購入プロジェクト）の改造 II” https://github.com/je1rav/QP-7C_RP2040/ でも示していますが，pico用ボードマネジャー”Arduino Mbed OS RP2040 Boards”でそのままコンパイルしても，Seeed XIAO RP2040では作動しません．  
これは，使用しているI2Cのピンが違うためです．  
(もともとは，Seeed XIAO RP2040用のMbed対応ボードマネージャーで開発していたのですが，このボードマネージャーが現在開発中止になっています．)  
このため，”Arduino Mbed OS RP2040 Boards”自身のファイルの1つを変更する必要があります．  
  
例えばMAC OSのArduino IDEの1.8.19では,   
“/Users/ユーザー名/Library/Arduino15/packages/arduino/hardware/mbed_rp2040/4.0.2/variants/RASPBERRY_PI_PICO/pins_arduino.h”   
WINDOWSのArduino IDEの1.8.19のインストーラ版では,  
“C:\Users\ユーザー名\AppData\Local\Arduino15\packages\arduino\hardware\mbed_rp2040\4.0.2\variants\RASPBERRY_PI_PICO\pins_arduino.h”   
にある”pins_arduino.h”ファイルの中身を一部書き換えます．   
ここで，”4.0.2”はボードマネージャーのヴァージョンを示しています．   

”pins_arduino.h”は使用するピンの指定やピンの名前付けなどの設定ファイル（ヘッダーファイル）です．   
このファイルの   
// Wire   
#define PIN_WIRE_SDA        (4u)   
#define PIN_WIRE_SCL        (5u)   
の部分を   
// Wire   
#define PIN_WIRE_SDA        (6u)   
#define PIN_WIRE_SCL        (7u)   
に変更します．   
これは，I2Cで使用するSDAピンとSCLピンについて，それぞれ4番ピンと5番ピンだったのを，6番ピンと7番ピンに変更することを意味します．   
この変更によって，Seeed XIAO RP2040で使用できるようになります．   
ファイルを書き換えた後は，Arduino IDEを再立ち上げして，ファイルの変更が確実に有効になってからコンパイルすると良いでしょう．   

