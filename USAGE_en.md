# RmppAtom Usage

## Wi-Fi Connection

### Connecting directly to a smartphone or other device
 1. Switch the power pack to Access Point mode.
 1. Select the power pack’s SSID (RmppAtom-softAP) on your smartphone or other device, and enter the password (p@ass1234) to connect.
 1. The SSID and password can be changed using serial communication commands.
### Connecting via a Wi-Fi router or access point
 1. Switch the power pack to Station mode.
 1. Use serial communication commands to set the SSID and password for the access point. The SSID and password will be saved in the power pack's non-volatile memory.
 1. The power pack will automatically connect to the access point using the saved SSID and password.
 1. If you want to set a static IP address, you can modify it using serial communication commands.
### Switching between Access Point mode and Station mode
 1. Press and hold the operation button on ATOM Lite for 0.5 seconds or more. Once the mode change is complete, the LED on ATOM Lite will blink.
 1. Next, press the reset button on ATOM Lite. The power pack will begin operating in the new mode.

### Basic Operation
 1. Open the power pack’s control screen on a smartphone or other device’s web browser. Enter the power pack’s hostname (rmpp-atom.local) or IP address in the URL. Once the control screen opens, it will display the input voltage and CPU temperature.
 1. Tap the Forward or Reverse button. The button will turn green when the output is turned on.
 1. Adjust the speed using the slider.
 1. Tap the OFF button. The button will return to its original color when the output is turned off.
### Changing the Direction
 1. Set the speed to 0 using the slider or tap the Stop button.
 1. Tap the OFF button.
 1. Tap the Forward or Reverse button to change the direction.
### Emergency Stop
 1. Tap the OFF button or press the operation button on ATOM Lite. This will immediately turn off the output, even while in motion.

## LED Indicators

<table>
	<thead>
		<tr>
			<th rowspan="2">Status</th>
			<th rowspan="2">Light Pattern</th>
			<th colspan="2">Light Color</th>
		</tr><tr>
			<th>In station mode</th>
			<th>In access point mode</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Initializing</td>
			<td>ON</td>
			<td>White</td>
			<td>White</td>
		</tr><tr>
			<td>Connecting to the AP</td>
			<td>Fast Blink</td>
			<td>Blue</td>
			<td>---</td>
		</tr><tr>
			<td>Output OFF<br/>(Web browser is not connect)</td>
			<td>Blink (with short on time)</td>
			<td>Blue</td>
			<td>Magenta</td>
		</tr><tr>
			<td>Output OFF<br/>(Web browser is connected)</td>
			<td>Blink (with long on time)</td>
			<td>Blue</td>
			<td>Magenta</td>
		</tr><tr>
			<td>Output ON</td>
			<td>Blink (with long on time)</td>
			<td>Green</td>
			<td>Green</td>
		</tr><tr>
			<td>In protection</td>
			<td>Fast blink</td>
			<td>Red</td>
			<td>Red</td>
		</tr>
	</tbody>
</table>

## Serial Communication Commands
 You can change settings through serial communication or via the web interface.

### Connection Settings

#### Wi-Fi Authentication Information (Station Mode)
 To connect the power pack to a smartphone or other device via a Wi-Fi router or access point, set the access point’s SSID and password.<br/>
 **Command Example :** `WIFI mySSID myPassword`

#### Wi-Fi Authentication Information (Access Point Mode)
 To connect the power pack directly to a smartphone or other device in Access Point mode, set the power pack’s SSID and password. Settings will be applied after a reset.<br/>
 **Command Example :** `WFAP mySSID myPassword`

#### Host name
 Set the mDNS (Multicast DNS) hostname. Settings will be applied after a reset.<br/>
 **Command Example :** `HOST MyDevice`

### TCP/IP Settings

#### Local Address
 Set the power pack’s static local IP address. If not set, it will automatically receive an IP address. Settings will be applied after a reset.<br/>
 **Command Example :** `IPAD 192.168.0.1`

#### Default Gateway
 Set the IP address for the default gateway. Settings will be applied after a reset.<br/>
 **Command Example :** `GWAY 192.168.0.1`

#### Subnet Mask
 Set the subnet mask. Settings will be applied after a reset.<br/>
 **Command Example :** `SNET 255.255.255.0`

## Other
 - When the output is on, the smartphone or device’s sleep mode will be prevented.
 - If communication between the power pack and the web browser is interrupted, the output will automatically turn off for safety reasons.
