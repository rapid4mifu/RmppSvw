# RmppAtom
## Overview／概要
A compact power pack for N-scale railway models operated with a web browser using ATOM Lite.<br/>
ATOM Liteを使用したWebブラウザで操作するコンパクトなNゲージ鉄道模型用パワーパックです。

![ATOM Liteを使用した鉄道模型用パワーパック](DSC02939.jpg)

## Features／特徴
- It is can run N-scale railway models by operating a web browser on a smartphone or other device.
  スマートフォン等のWebブラウザの操作により、Nゲージ鉄道模型を走らせることができます。
- It is designed to be operated exclusively from a web browser, without switches or dials. By utilizing existing hardware products, it is easy to create power pack.
  スイッチやダイヤルを省略し、Webブラウザからの操作専用の仕様です。既存のハードウェア製品を活用することで、パワーパックを容易に自作することが可能です。
- Achieves low cost and space saving by using SoC (Espressif Systems : ESP32) compatible with Wi-Fi standard.
  Wi-Fi規格に対応したSoC(Espressif Systems : ESP32)を採用し、低コスト・省スペースを実現できます。
- Even if the SoC operation becomes unstable, safety is ensured by the motor driver IC with built-in overcurrent protection.
  万が一SoCの動作が不安定になっても、過電流保護を内蔵したモータドライバICにより安全性を確保します。

## Block Diagram／ブロック図
![](BlockDiagram_RmppAtom.svg)

## Specification Overview ／ 仕様概要
<table>
	<thead>
		<tr>
		<th colspan="2">Item／項目</th>
		<th>specification／仕様</th>
		</tr>
	</thead>
	<tbody>
		<tr>
		<td colspan="2">Rated Input Voltage<br>定格入力電圧</td>
		<td>DC12V</td>
		</tr>
		<tr>
		<td colspan="2">Rated Output Current<br>定格出力電流</td>
		<td>1.5A</td>
		</tr>
		<tr>
		<td rowspan="2">Output Control<br>出力制御</td>
		<td>Method<br>方式</td>
		<td>PWM (Frequency : 19kHz, Resolution : 12bit)</td>
		</tr>
		<tr>
		<td>Polarity<br>極性</td>
		<td>both directions (forward and reverse)<br>両方向（前進／後退）
		</tr>
		<tr>
		<td colspan="2">Display<br>表示</td>
		<td>RGB LED</td>
		</tr>
		<tr>
		<td colspan="2">Protections<br>保護</td>
		<td>built-in motor driver IC (over current, thermal shutdown)<br>モータドライバIC内蔵（過電流、過熱）</td>
		</tr>
	</tbody>
</table>

## Requirement ／ 必要要件
### Hardware ／ ハードウェア
- [ATOM Lite ESP32 IoT Development Kit](https://shop.m5stack.com/products/atom-lite-esp32-development-kit)
- [ATOMIC H-Bridge Driver Base (DRV8876)](https://shop.m5stack.com/products/atomic-h-bridge-driver-base-drv8876)

### Software ／ ソフトウェア
#### Framework ／ フレームワーク
- Arduino
#### Development Environment ／ 開発環境
- VSCode & PlatformIO

## Usage ／ 使用方法

### Wi-Fi接続方法

#### パワーパックとスマートフォン等の端末を直接接続する場合
1. パワーパックをアクセスポイントモードに切り替えます。
1. スマートフォン等の端末でパワーパックのSSID（RmppAtom-softAP）を選択し、パスワード（p@ass1234）を入力して接続します。
1. SSID、及びパスワードは、シリアル通信コマンドを使用して変更できます。

#### Wi-Fiルータ等のアクセスポイントを経由して接続する場合
1. パワーパックをステーションモードに切り替えます。
1. シリアル通信コマンドを使用して、アクセスポイントのSSIDとパスワードを設定します。SSIDとパスワードはパワーパック内に不揮発性メモリに保存されます。
1. 保存されたSSIDとパスワードを使用して、パワーパックは自動的にアクセスポイントに接続します。
1. IPアドレスを固定にする場合は、シリアル通信コマンドを使用して変更できます。

#### アクセスポイントモード／ステーションモードの切り替え
1. ATOM Liteの操作ボタンを0.5秒以上長押しします。
1. モード設定が完了すると、ATOM LiteのLEDが点滅します。
1. 次に、ATOM Liteのリセットボタンを押します。
1. パワーパックは切り替え後のモードで動作を開始します。

### 基本操作
1. スマートフォン等のWebブラウザで、パワーパックの操作画面を開きます。URLには、パワーパックのIPアドレス、またはホスト名（rmpp-atom.local）を入力してください。
1. 操作画面が開くと、入力電圧やCPUの温度が表示されます。
1. Forwardボタン、又はReverseをタップします。
1. 出力がオンになると、タップしたボタンが緑色になります。
1. スライダーで速度を調整します。
1. OFFボタンをタップします。
1. 出力がオフになると、緑色だったボタンが元の色に戻ります。

### 進行方向の切り替え
1. スライダーで速度を0にするか、Stopボタンをタップします。
1. OFFボタンをタップします。
1. Forwardボタン、又はReverseをタップして、進行方向を切り替えます。

### 緊急停止
1. OFFボタンをタップするか、ATOM Liteの操作ボタンを押します。
1. 走行中でも直ちに出力がオフになります。

### その他
- 出力をオンにしている間は、スマートフォン等の端末のスリープが抑制されます。
- パワーパックとWebブラウザの通信が途絶えた場合、安全のため、出力がオフになります。

### Display ／ 表示

<table>
	<thead>
		<tr>
			<th rowspan="2">Status<br>状態</th>
			<th rowspan="2">Light Pattern<br>点灯パターン</th>
			<th colspan="2">Light Color<br>点灯色</th>
		</tr>
		<tr>
			<th>In station mode<br>ステーションモード</th>
			<th>In access point mode<br>アクセスポイントモード</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Initializing<br>初期化中</td>
			<td>ON</td>
			<td>White</td>
			<td>White</td>
		</tr>
		<tr>
			<td>Connecting to the AP<br>アクセスポイントへ接続中</td>
			<td>Fast Blink</td>
			<td>Blue</td>
			<td>---</td>
		</tr>
		<tr>
			<td>Output OFF<br>(Web browser is not connect)<br>出力オフ(Webブラウザ未接続)</td>
			<td>Blink (with short on time)</td>
			<td>Blue</td>
			<td>Magenta</td>
		</tr>
		<tr>
			<td>Output OFF<br>(Web browser is connected)<br>出力オフ(Webブラウザ接続済)</td>
			<td>Blink (with long on time)</td>
			<td>Blue</td>
			<td>Magenta</td>
		</tr>
		<tr>
			<td>Output ON<br>出力オン</td>
			<td>Blink (with long on time)</td>
			<td>Green</td>
			<td>Green</td>
		</tr>
		<tr>
			<td>In protection<br>保護機能作動</td>
			<td>Fast blink</td>
			<td>Red</td>
			<td>Red</td>
		</tr>
	</tbody>
</table>

### シリアル通信コマンド
 シリアル通信、又はWeb画面を通じて、設定変更などを行うことができます。

#### 接続設定

##### Wi-Fi認証情報（ステーションモード）
 パワーパックとスマートフォン等の端末をWi-Fiルータ等を経由して接続する場合、Wi-Fiルータ等のアクセスポイントのSSIDとパスワードを設定します。
 **コマンド例 :** `WIFI mySSID myPassword`

##### Wi-Fi認証情報（ステーションモード）
 パワーパックとスマートフォン等の端末をWi-Fiルータ等を経由して接続する場合、Wi-Fiルータ等のアクセスポイントのSSIDとパスワードを設定します。
 **コマンド例 :** `WIFI mySSID myPassword`

##### Wi-Fi認証情報（アクセスポイントモード）
 パワーパックとスマートフォン等の端末を直接接続する場合、アクセスポイントとして動作するパワーパックのSSIDとパスワードを設定します。リセット後に設定が反映されます。
 **コマンド例 :** `WFAP mySSID myPassword`

##### ホスト名設定
 mDNS（Multicast DNS）用のホスト名を設定します。リセット後に設定が反映されます。
 **コマンド例 :** `HOST MyDevice`

#### TCP/IP設定

##### ローカルアドレス
 パワーパックのローカルアドレスを固定して使用する場合に設定します。リセット後に設定が反映されます。設定しない場合は自動割当となります。
 **コマンド例 :** `IPAD 192.168.0.1`

##### デフォルトゲートウェイ
 デフォルトゲートウェイのIPを設定します。リセット後に設定が反映されます。
 **コマンド例 :** `GWAY 192.168.0.1`

##### サブネットマスク
 サブネットマスクを設定します。リセット後に設定が反映されます。
 **コマンド例 :** `SNET 255.255.255.0`

#### その他

##### リセット
 SoCをリセットして、パワーパックを再起動します。
 **コマンド例 :** `RESET`

## License ／ ライセンス
This project is licensed under the MIT License. See the LICENSE.md file for details.<br/>
このプロジェクトは MIT ライセンスの元にライセンスされています。 詳細は LICENSE.md をご覧ください。

## Author
[X(Twitter)](https://x.com/rapid_mifu)