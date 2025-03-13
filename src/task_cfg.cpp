// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_cfg.h"
#include "lib/EspEasySerialCommandEx.h"

#include "board.h"
#include "config.h"

#ifdef CFG_ATTACH_CALLBACK_FROM_SERVER
#include "task_server.h"
#endif

#include <WiFi.h>
#include <Preferences.h>

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

EspEasySerialCommandEx cmds(Serial);

Preferences prefs;

// Wi-Fi setting for access point mode
//  [key]
const char apen_key[] = "apen";
const char apid_key[] = "apid";
const char appw_key[] = "appw";
//  [initial value]
const bool apen_default = false;
const String apid_default = AP_SSID_DEFAULT;
const String appw_default = AP_PASS_DEFAULT;
//  [variable]
bool isApMode;
String sApModeSsid;
String sApModePass;

// TCP/IP network configuration
//  [key]
const char ipad_key[] = "ipad";
const char ipgw_key[] = "ipgw";
const char ipsn_key[] = "ipsn";
//  [initial value]
const uint8_t ipad_default[] = {0, 0, 0, 0};
const uint8_t ipgw_default[] = {0, 0, 0, 0};
const uint8_t ipsn_default[] = {255, 255, 255, 0};
//  [variable]
IPAddress ipLocal;
IPAddress ipGateway;
IPAddress ipSubnet;

// Host name for Multicast DNS (mDNS)
//  [key]
const char host_key[] = "host";
//  [initial value]
const String host_default = HOST_DEFAULT;
//  [variable]
String sHostName;

TaskHandle_t hTaskConfig;
CallbackOnChangeSuccess cbChangeSuccess = NULL;

static void cfg_processTask(void* pvParameters);

void cfg_actionSavedData(EspEasySerialCommandEx::command_t command);
void cfg_setWifiCredential(EspEasySerialCommandEx::command_t command);
void cfg_setWifiCredentialForAP(EspEasySerialCommandEx::command_t command);
void cfg_setLocalAddress(EspEasySerialCommandEx::command_t command);
void cfg_setDefaultGateway(EspEasySerialCommandEx::command_t command);
void cfg_setSubnetMask(EspEasySerialCommandEx::command_t command);
void cfg_setHostName(EspEasySerialCommandEx::command_t command);

/******************************************************************************
* Function Name: CFG_initTask
* Description  : コンフィグレーションタスク初期化
* Arguments    : none
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool CFG_initTask(void)
{
	size_t len = 0;

	Serial.println("Configuration data loading ...");

	// open namespace
	prefs.begin(CFG_NAMESPACE);

	// wi-fi operation for access point mode
	if (!prefs.isKey(apen_key)) {
		prefs.putBool(apen_key, apen_default);
	}
	isApMode = prefs.getBool(apen_key, apen_default);

	// wi-fi ssid for access point mode
	if (!prefs.isKey(apid_key)) {
		prefs.putString(apid_key, apid_default);
	}
	sApModeSsid = prefs.getString(apid_key, apid_default);

	// wi-fi password for access point mode
	if (!prefs.isKey(appw_key)) {
		prefs.putString(appw_key, appw_default);
	}
	sApModePass = prefs.getString(appw_key, appw_default);
	
	// local ip address
	len = prefs.getBytesLength(ipad_key);
	if (len == 4) {
		uint8_t buffer[len];  // prepare a buffer for the data
		prefs.getBytes(ipad_key, buffer, len);
		ipLocal = IPAddress(buffer[0], buffer[1], buffer[2], buffer[3]);
	} else {
		prefs.putBytes(ipad_key, ipad_default, sizeof(ipad_default));
		ipLocal = IPAddress(ipad_default);
	}

	// default gateway
	len = prefs.getBytesLength(ipgw_key);
	if (len == 4) {
		uint8_t buffer[len];  // prepare a buffer for the data
		prefs.getBytes(ipgw_key, buffer, len);
		ipGateway = IPAddress(buffer[0], buffer[1], buffer[2], buffer[3]);
	} else {
		prefs.putBytes(ipgw_key, ipgw_default, sizeof(ipgw_default));
		ipGateway = IPAddress(ipgw_default);
	}

	// subnet mask
	len = prefs.getBytesLength(ipsn_key);
	if (len == 4) {
		uint8_t buffer[len];  // prepare a buffer for the data
		prefs.getBytes(ipsn_key, buffer, len);
		ipSubnet = IPAddress(buffer[0], buffer[1], buffer[2], buffer[3]);
	} else {
		prefs.putBytes(ipsn_key, ipsn_default, sizeof(ipsn_default));
		ipSubnet = IPAddress(ipsn_default);
	}

	// Host name for Multicast DNS (mDNS)
	if (!prefs.isKey(host_key)) {
		prefs.putString(host_key, host_default);
	}
	sHostName = prefs.getString(host_key, host_default);

	// close namespace
	prefs.end();

	// preset Reset Function
	cmds.addCommand("RESET", EspEasySerialCommandEx::resetCommand);
	// host name setup
	cmds.addCommand("CNFG", cfg_actionSavedData);
	// Wi-Fi setup
	cmds.addCommand("WIFI", cfg_setWifiCredential);
	// Wi-Fi setup (for access point mode)
	cmds.addCommand("WFAP", cfg_setWifiCredentialForAP);
	// IP Address setup
	cmds.addCommand("IPAD", cfg_setLocalAddress);
	// Gateway setup
	cmds.addCommand("GWAY", cfg_setDefaultGateway);
	// Subnet setup
	cmds.addCommand("SNET", cfg_setSubnetMask);
	// host name setup
	cmds.addCommand("HOST", cfg_setHostName);

#ifdef CFG_ATTACH_CALLBACK_FROM_SERVER
	SRV_attachWsTextListener(CFG_processCommand);
#endif

	Serial.println("Configuration task is now starting ...");
	BaseType_t taskCreated = xTaskCreateUniversal(cfg_processTask, "cfg_processTask", 4096, nullptr, 1, &hTaskConfig, APP_CPU_NUM);
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create Configuration task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: cfg_processTask
* Description  : コンフィグレーションタスク
* Arguments    : none
* Return Value : none
******************************************************************************/
void cfg_processTask(void* pvParameters)
{
	while (1)
	{
		cmds.task();
		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: CFG_resetSavedData
* Description  : 保存されたデータを初期値に戻す。
* Arguments    : none
* Return Value : none
******************************************************************************/
void CFG_resetSavedData(void)
{
	prefs.begin(CFG_NAMESPACE);

	// wi-fi operation for access point mode
	prefs.putBool(apen_key, apen_default);

	// wi-fi ssid for access point mode
	prefs.putString(apid_key, apid_default);

	// wi-fi password for access point mode
	prefs.putString(appw_key, appw_default);
	
	// local ip address
	prefs.putBytes(ipad_key, ipad_default, sizeof(ipad_default));

	// default gateway
	prefs.putBytes(ipgw_key, ipgw_default, sizeof(ipgw_default));

	// subnet mask
	prefs.putBytes(ipsn_key, ipsn_default, sizeof(ipsn_default));

	// Host name for Multicast DNS (mDNS)
	prefs.putString(host_key, host_default);

	prefs.end();
}

/******************************************************************************
* Function Name: CFG_printSavedData
* Description  : 保存されたデータをシリアルコンソールに出力する。
* Arguments    : none
* Return Value : none
******************************************************************************/
void CFG_printSavedData(void)
{
	Serial.printf("Configuration data\n");
	Serial.printf("  Name Space : %s\n", CFG_NAMESPACE);
	Serial.printf("   Wi-Fi (access point mode) credential\n");
	Serial.printf("    SSID            : %s\n", sApModeSsid.c_str());
	Serial.printf("    Password        : %s\n", sApModePass.c_str());
	Serial.printf("   TCP/IP network configuration\n");
	Serial.printf("    local address   : %u.%u.%u.%u\n", ipLocal[0], ipLocal[1], ipLocal[2], ipLocal[3]);
	Serial.printf("    default gateway : %u.%u.%u.%u\n", ipGateway[0], ipGateway[1], ipGateway[2], ipGateway[3]);
	Serial.printf("    subnet mask     : %u.%u.%u.%u\n", ipSubnet[0], ipSubnet[1], ipSubnet[2], ipSubnet[3]);
	Serial.printf("   Multicast DNS (mDNS)\n");
	Serial.printf("    host name       : %s\n", sHostName.c_str());	
	Serial.printf("\n");
}

/******************************************************************************
* Function Name: CFG_processCommand
* Description  : コマンドを処理する
* Arguments    : id - for websocket client id (set to 0 when not in use)
* Return Value : none
******************************************************************************/
void CFG_processCommand(String command, uint32_t id)
{
	cmds.parseCommand(command);
}

/******************************************************************************
* Function Name: CFG_setApMode
* Description  : アクセスポイントモードを有効／無効にする
* Arguments    : true -> access point mode is enable
* Return Value : none
******************************************************************************/
void CFG_setApMode(bool enable)
{
	if (isApMode != enable) {
		isApMode = enable;
		
		prefs.begin(CFG_NAMESPACE);
		prefs.putBool(apen_key, enable);
		prefs.end();

		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	}

	if (isApMode) {
		Serial.printf("Access point mode is enable. will be applied after reset.\n");
	} else {
		Serial.printf("Access point mode is disable. will be applied after reset.\n");
	}
}

/******************************************************************************
* Function Name: CFG_toggleApMode
* Description  : アクセスポイントモードの動作を切り替える
* Arguments    : true -> access point mode is enable
* Return Value : none
******************************************************************************/
void CFG_toggleApMode(void)
{
	CFG_setApMode(!isApMode);
}

/******************************************************************************
* Function Name: CFG_isApModeEnabled
* Description  : アクセスポイントモードが有効であるか
* Arguments    : none
* Return Value : true -> access point mode is enable
******************************************************************************/
bool CFG_isApModeEnabled(void)
{
	return isApMode;
}

/******************************************************************************
* Function Name: cfg_actionSavedData
* Description  : 保存されたデータを扱う。
* Arguments    : command.command2 = local address
* Return Value : none
******************************************************************************/
void cfg_actionSavedData(EspEasySerialCommandEx::command_t command)
{
	if ("RESET" == command.command2) {
		CFG_resetSavedData();
	} else {
		CFG_printSavedData();
	}
}

/******************************************************************************
* Function Name: cfg_setWifiCredential
* Description  : Wi-Fi認証情報の設定
* Arguments    : command.command2 = SSID, command.command3 = PASSWORD
* Return Value : none
******************************************************************************/
void cfg_setWifiCredential(EspEasySerialCommandEx::command_t command)
{
	// > WIFI [SSID] [KEY]
	Serial.println("Change the Wi-Fi credentials and connect to the access point.");
	WiFi.begin(command.command2.c_str(), command.command3.c_str());

	int i = 0;
	while (WiFi.status() != WL_CONNECTED) {
		if (200 < i) {
			Serial.println(" [warning] Connection to the access point timed out.");
			WiFi.disconnect();
			return;
		}

		delay(50);
		i++;
	}

	Serial.println(" Connected to the access point.");
}

/******************************************************************************
* Function Name: cfg_setWifiCredentialForAP
* Description  : Wi-Fi認証情報の設定（アクセスポイントモード）
* Arguments    : command.command2 = SSID, command.command3 = PASSWORD
* Return Value : none
******************************************************************************/
void cfg_setWifiCredentialForAP(EspEasySerialCommandEx::command_t command)
{
	// > WFAP [SSID] [PASSWORD]
	if (command.command2.length() && command.command3.length()) {
		prefs.begin(CFG_NAMESPACE);
		prefs.putString(apid_key, command.command2.c_str());
		prefs.putString(appw_key, command.command3.c_str());
		prefs.end();

		Serial.printf("[success] WFAP %s %s. will be applied after reset.\n", 
			command.command2.c_str(), command.command3.c_str());
		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	} else {
		Serial.printf("[failure] WFAP %s %s\n", command.command2.c_str(), command.command3.c_str());
	}
}

/******************************************************************************
* Function Name: CFG_getApModeSSID
* Description  : Wi-Fi認証情報を取得（SSID、アクセスポイントモード）
* Arguments    : none
* Return Value : SSID
******************************************************************************/
const char * CFG_getApModeSSID(void)
{
	return sApModeSsid.c_str();
}

/******************************************************************************
* Function Name: CFG_getApModePass
* Description  : Wi-Fi認証情報を取得（パスワード、アクセスポイントモード）
* Arguments    : none
* Return Value : password
******************************************************************************/
const char * CFG_getApModePass(void)
{
	return sApModePass.c_str();
}

/******************************************************************************
* Function Name: cfg_setLocalAddress
* Description  : ローカルアドレスの設定
* Arguments    : command.command2 = local address
* Return Value : none
******************************************************************************/
void cfg_setLocalAddress(EspEasySerialCommandEx::command_t command)
{
	// > IPAD 192.168.0.2
	if (ipLocal.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipLocal[0],ipLocal[1],ipLocal[2],ipLocal[3]};

		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipad_key, ipv4, sizeof(ipv4));
		prefs.end();

		Serial.printf("[success] IPAD %s. will be applied after reset.\n", command.command2.c_str());
		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	} else {
		Serial.printf("[failure] IPAD %s\n", command.command2.c_str());
	}
}

/******************************************************************************
* Function Name: CFG_getLocalAddress
* Description  : ローカルアドレスを取得
* Arguments    : none
* Return Value : local address
******************************************************************************/
IPAddress CFG_getLocalAddress(void)
{
	return ipLocal;
}

/******************************************************************************
* Function Name: cfg_setDefaultGateway
* Description  : デフォルトゲートウェイの設定
* Arguments    : command.command2 = default gateway
* Return Value : none
******************************************************************************/
void cfg_setDefaultGateway(EspEasySerialCommandEx::command_t command)
{
	// > GWAY 192.168.0.1
	if (ipGateway.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipGateway[0],ipGateway[1],ipGateway[2],ipGateway[3]};

		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipgw_key, ipv4, sizeof(ipv4));
		prefs.end();

		Serial.printf("[success] GWAY %s. will be applied after reset.\n", command.command2.c_str());
		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	} else {
		Serial.printf("[failure] GWAY %s\n", command.command2.c_str());
	}
}

/******************************************************************************
* Function Name: CFG_getDefaultGateway
* Description  : デフォルトゲートウェイを取得
* Arguments    : none
* Return Value : default gateway
******************************************************************************/
IPAddress CFG_getDefaultGateway(void)
{
	return ipGateway;
}

/******************************************************************************
* Function Name: cfg_setSubnetMask
* Description  : サブネットマスクの設定
* Arguments    : command.command2 = subnet mask
* Return Value : none
******************************************************************************/
void cfg_setSubnetMask(EspEasySerialCommandEx::command_t command)
{
	// > SNET 255.255.255.0
	if (ipSubnet.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipSubnet[0],ipSubnet[1],ipSubnet[2],ipSubnet[3]};

		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipsn_key, ipv4, sizeof(ipv4));
		prefs.end();

		Serial.printf("[success] SNET %s. will be applied after reset.\n", command.command2.c_str());
		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	} else {
		Serial.printf("[failure] SNET %s\n", command.command2.c_str());
	}
}

/******************************************************************************
* Function Name: CFG_getSubnetMask
* Description  : ローカルアドレスを取得
* Arguments    : none
* Return Value : subnet mask
******************************************************************************/
IPAddress CFG_getSubnetMask(void)
{
	return ipSubnet;
}

/******************************************************************************
* Function Name: cfg_setHostName
* Description  : ホスト名の設定（mDNS）
* Arguments    : command.command2 = host name
* Return Value : none
******************************************************************************/
void cfg_setHostName(EspEasySerialCommandEx::command_t command)
{
	// > HOST [host name] 
	if (command.command2.length()) {
		prefs.begin(CFG_NAMESPACE);
		prefs.putString(host_key, command.command2.c_str());
		prefs.end();

		Serial.printf("[success] HOST %s. will be applied after reset.\n", 
			command.command2.c_str());
		if (NULL != cbChangeSuccess) {
			cbChangeSuccess();
		}
	} else {
		Serial.printf("[failure] HOST %s\n", command.command2.c_str());
	}
}

/******************************************************************************
* Function Name: CFG_getHostName
* Description  : ホスト名の取得（mDNS）
* Arguments    : none
* Return Value : host name
******************************************************************************/
const char * CFG_getHostName(void)
{
	return sHostName.c_str();
}

/******************************************************************************
* Function Name: CFG_attachChangeSuccessListener
* Description  : 設定変更が成功したときのコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void CFG_attachChangeSuccessListener(CallbackOnChangeSuccess callback)
{
	cbChangeSuccess = callback;
}
