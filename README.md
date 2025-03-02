# RmppAtom
## Overview／概要
A compact power pack for N-scale railway models operated with a web browser using ATOM Lite.
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
	<tbody>
	<tr>
	<th colspan="2">Item／項目</th>
	<th>specification／仕様</th>
	</tr>
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

## Others ／ その他
Coming soon
近日公開予定

## License ／ ライセンス
This project is licensed under the MIT License. See the LICENSE.md file for details.
このプロジェクトは MIT ライセンスの元にライセンスされています。 詳細は LICENSE.md をご覧ください。

## Author
[X(Twitter)](https://x.com/rapid_mifu)