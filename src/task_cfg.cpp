// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_cfg.h"
#include "task_cli.h"

#include "config.h"

#include <WiFi.h>

#if defined(ESP32)
#include <Preferences.h>
#define CFG_USE_PREFERENCES
#else
#include <ArduinoJson.h>
#include <StreamUtils.h>
#endif

#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <EEPROM.h>
#include <FreeRTOS.h>
#include <task.h>
#endif

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#endif

#if defined(CFG_USE_PREFERENCES)
Preferences prefs;
#else
StaticJsonDocument<2048> jDoc;
#endif

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

#ifndef ESP32
// Wi-Fi setting for station mode
//  [key]
const char stid_key[] = "stid";
const char stpw_key[] = "stpw";
//  [initial value]
const String stid_default = "";
const String stpw_default = "";
//  [variable]
String sStModeSsid;
String sStModePass;
#endif

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

CallbackOnChangeSuccess cbChangeSuccess = NULL;

void cfg_actionSavedData(cli_cmd_t command);
void cfg_setWifiCredential(cli_cmd_t command);
void cfg_setWifiCredentialForAP(cli_cmd_t command);
void cfg_setLocalAddress(cli_cmd_t command);
void cfg_setDefaultGateway(cli_cmd_t command);
void cfg_setSubnetMask(cli_cmd_t command);
void cfg_setHostName(cli_cmd_t command);

/******************************************************************************
* Function Name: CFG_initTask
* Description  : コンソールタスク初期化
* Arguments    : none
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool CFG_initTask(void)
{
	size_t len = 0;

	Serial.println("Configuration data loading ...");
#if defined(CFG_USE_PREFERENCES)
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

#ifndef ESP32
	// wi-fi ssid for station mode
	if (!prefs.isKey(stid_key)) {
		prefs.putString(stid_key, stid_default);
	}
	sStModeSsid = prefs.getString(stid_key, stid_default);

	// wi-fi password for station mode
	if (!prefs.isKey(stpw_key)) {
		prefs.putString(stpw_key, stpw_default);
	}
	sStModePass = prefs.getString(stpw_key, stpw_default);
#endif

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

#else
	EEPROM.begin(2048);
	EepromStream eepromStream(0, 2048);
	deserializeJson(jDoc, eepromStream);
	EEPROM.end();

	// wi-fi operation for access point mode
	if (!jDoc[apen_key]){
		jDoc[apen_key] = apen_default;
	}
	isApMode = jDoc[apen_key].as<bool>();

	// wi-fi ssid for access point mode
	if (!jDoc[apid_key]) {
		jDoc[apid_key] = apid_default;
	}
	sApModeSsid = String(jDoc[apid_key]);

	// wi-fi password for access point mode
	if (!jDoc[appw_key]) {
		jDoc[appw_key] = appw_default;
	}
	sApModePass = String(jDoc[appw_key]);

	// wi-fi ssid for station mode
	if (!jDoc[stid_key]) {
		jDoc[stid_key] = stid_default;
	}
	sStModeSsid = String(jDoc[stid_key]);

	// wi-fi password for station mode
	if (!jDoc[stpw_key]) {
		jDoc[stpw_key] = stpw_default;
	}
	sStModePass = String(jDoc[stpw_key]);

	// local ip address
	if (!jDoc[ipad_key]) {
		jDoc[ipad_key] = IPAddress(ipad_default).toString();
	}
	ipLocal.fromString(jDoc[ipad_key].as<const char*>());

	// default gateway
	if (!jDoc[ipgw_key]) {
		jDoc[ipgw_key] = IPAddress(ipgw_default).toString();
	}
	ipGateway.fromString(jDoc[ipgw_key].as<const char*>());

	// subnet mask
	if (!jDoc[ipsn_key]) {
		jDoc[ipsn_key] = IPAddress(ipsn_default).toString();
	}
	ipSubnet.fromString(jDoc[ipsn_key].as<const char*>());

	// Host name for Multicast DNS (mDNS)
	if (!jDoc[host_key]) {
		jDoc[host_key] = host_default;
	}
	sHostName = String(jDoc[host_key]);

#endif

	// config function
	CLI_addCommand("CNFG", cfg_actionSavedData);
	// Wi-Fi setup
	CLI_addCommand("WIFI", cfg_setWifiCredential);
	// Wi-Fi setup (for access point mode)
	CLI_addCommand("WFAP", cfg_setWifiCredentialForAP);
	// IP Address setup
	CLI_addCommand("IPAD", cfg_setLocalAddress);
	// Gateway setup
	CLI_addCommand("GWAY", cfg_setDefaultGateway);
	// Subnet setup
	CLI_addCommand("SNET", cfg_setSubnetMask);
	// host name setup
	CLI_addCommand("HOST", cfg_setHostName);

	return true;
}

/******************************************************************************
* Function Name: CFG_resetSavedData
* Description  : 保存されたデータを初期値に戻す。
* Arguments    : none
* Return Value : none
******************************************************************************/
void CFG_resetSavedData(void)
{
#if defined(CFG_USE_PREFERENCES)
	prefs.begin(CFG_NAMESPACE);

	// wi-fi operation for access point mode
	prefs.putBool(apen_key, apen_default);

	// wi-fi ssid for access point mode
	prefs.putString(apid_key, apid_default);

	// wi-fi password for access point mode
	prefs.putString(appw_key, appw_default);
	
	// wi-fi ssid for station mode
	//prefs.putString(stid_key, stid_default);

	// wi-fi password for station mode
	//prefs.putString(stpw_key, stpw_default);

	// local ip address
	prefs.putBytes(ipad_key, ipad_default, sizeof(ipad_default));

	// default gateway
	prefs.putBytes(ipgw_key, ipgw_default, sizeof(ipgw_default));

	// subnet mask
	prefs.putBytes(ipsn_key, ipsn_default, sizeof(ipsn_default));

	// Host name for Multicast DNS (mDNS)
	prefs.putString(host_key, host_default);

	prefs.end();

#else
	// wi-fi operation for access point mode
	jDoc[apen_key] = apen_default;

	// wi-fi ssid for access point mode
	jDoc[apid_key] = apid_default;

	// wi-fi password for access point mode
	jDoc[appw_key] = appw_default;

	// wi-fi ssid for station mode
	jDoc[stid_key] = stid_default;

	// wi-fi password for station mode
	jDoc[stpw_key] = stpw_default;

	// local ip address
	jDoc[ipad_key] = IPAddress(ipad_default).toString();

	// default gateway
	jDoc[ipgw_key] = IPAddress(ipgw_default).toString();

	// subnet mask
	jDoc[ipsn_key] = IPAddress(ipsn_default).toString();

	// Host name for Multicast DNS (mDNS)
	jDoc[host_key] = host_default;

	EEPROM.begin(2048);
	EepromStream eepromStream(0, 2048);
	serializeJson(jDoc, eepromStream);
	EEPROM.end();
#endif
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
#ifndef ESP32
	Serial.printf("   Wi-Fi (station mode) credential\n");
	Serial.printf("    SSID            : %s\n", sStModeSsid.c_str());
	Serial.printf("    Password        : %s\n", sStModePass.c_str());
#endif
	Serial.printf("   TCP/IP network configuration\n");
	Serial.printf("    local address   : %u.%u.%u.%u\n", ipLocal[0], ipLocal[1], ipLocal[2], ipLocal[3]);
	Serial.printf("    default gateway : %u.%u.%u.%u\n", ipGateway[0], ipGateway[1], ipGateway[2], ipGateway[3]);
	Serial.printf("    subnet mask     : %u.%u.%u.%u\n", ipSubnet[0], ipSubnet[1], ipSubnet[2], ipSubnet[3]);
	Serial.printf("   Multicast DNS (mDNS)\n");
	Serial.printf("    host name       : %s\n", sHostName.c_str());	
	Serial.printf("\n");
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

#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putBool(apen_key, enable);
		prefs.end();
#else
		jDoc[apen_key] = enable;

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
void cfg_actionSavedData(cli_cmd_t command)
{
	if (command.command2 == "RESET") {
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
void cfg_setWifiCredential(cli_cmd_t command)
{
	// > WIFI [SSID] [KEY]
	if (command.command2.length() && command.command3.length()) {
#ifndef ESP32
		jDoc[stid_key] = command.command2;
		jDoc[stpw_key] = command.command3;
		
		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

		Serial.println("Change the Wi-Fi credentials and connect to the access point.");
		WiFi.mode(WIFI_STA);
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
	} else {
		Serial.printf("[failure] WIFI %s %s\n", command.command2.c_str(), command.command3.c_str());
	}
}

/******************************************************************************
* Function Name: cfg_setWifiCredentialForAP
* Description  : Wi-Fi認証情報の設定（アクセスポイントモード）
* Arguments    : command.command2 = SSID, command.command3 = PASSWORD
* Return Value : none
******************************************************************************/
void cfg_setWifiCredentialForAP(cli_cmd_t command)
{
	// > WFAP [SSID] [PASSWORD]
	if (command.command2.length() && command.command3.length()) {
#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putString(apid_key, command.command2.c_str());
		prefs.putString(appw_key, command.command3.c_str());
		prefs.end();
#else
		jDoc[apid_key] = command.command2;
		jDoc[appw_key] = command.command3;

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
* Function Name: CFG_getStaModeSSID
* Description  : Wi-Fi認証情報を取得（SSID、ステーションモード）
* Arguments    : none
* Return Value : SSID
******************************************************************************/
const char * CFG_getStaModeSSID(void)
{
#ifndef ESP32
	return sStModeSsid.c_str();
#else
	return "";
#endif
}

/******************************************************************************
* Function Name: CFG_getStaModePass
* Description  : Wi-Fi認証情報を取得（パスワード、ステーションモード）
* Arguments    : none
* Return Value : password
******************************************************************************/
const char * CFG_getStaModePass(void)
{
#ifndef ESP32
	return sStModePass.c_str();
#else
	return "";
#endif
}

/******************************************************************************
* Function Name: cfg_setLocalAddress
* Description  : ローカルアドレスの設定
* Arguments    : command.command2 = local address
* Return Value : none
******************************************************************************/
void cfg_setLocalAddress(cli_cmd_t command)
{
	// > IPAD 192.168.0.2
	if (ipLocal.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipLocal[0],ipLocal[1],ipLocal[2],ipLocal[3]};

#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipad_key, ipv4, sizeof(ipv4));
		prefs.end();
#else
		jDoc[ipad_key] = IPAddress(ipv4).toString();

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
void cfg_setDefaultGateway(cli_cmd_t command)
{
	// > GWAY 192.168.0.1
	if (ipGateway.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipGateway[0],ipGateway[1],ipGateway[2],ipGateway[3]};

#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipgw_key, ipv4, sizeof(ipv4));
		prefs.end();
#else
		jDoc[ipgw_key] = IPAddress(ipv4).toString();

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
void cfg_setSubnetMask(cli_cmd_t command)
{
	// > SNET 255.255.255.0
	if (ipSubnet.fromString(command.command2.c_str())) {
		uint8_t ipv4[] = {ipSubnet[0],ipSubnet[1],ipSubnet[2],ipSubnet[3]};
#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putBytes(ipsn_key, ipv4, sizeof(ipv4));
		prefs.end();
#else
		jDoc[ipsn_key] = IPAddress(ipv4).toString();

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
void cfg_setHostName(cli_cmd_t command)
{
	// > HOST [host name] 
	if (command.command2.length()) {
#if defined(CFG_USE_PREFERENCES)
		prefs.begin(CFG_NAMESPACE);
		prefs.putString(host_key, command.command2.c_str());
		prefs.end();
#else
		jDoc[host_key] = command.command2;

		EEPROM.begin(2048);
		EepromStream eepromStream(0, 2048);
		serializeJson(jDoc, eepromStream);
		EEPROM.end();
#endif

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
