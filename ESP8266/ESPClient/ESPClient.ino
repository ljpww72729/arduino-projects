//
// Copyright 2015 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// FirebaseRoom_ESP8266 is a sample that demo using multiple sensors
// and actuactor with the FirebaseArduino library.

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <FirebaseArduino.h>   //https://github.com/firebase/firebase-arduino

//#include <ArduinoJson.h>  //https://github.com/bblanchon/ArduinoJson

// wifi ap connect
// needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ESP8266HTTPClient.h>

#define USE_SERIAL Serial

// for AP LED status
#include <Ticker.h>
Ticker ticker;

const int ledAPPin = 16;
// 以AP模式运行时，指示灯闪烁
void tick()
{
  //toggle state
  int state = digitalRead(ledAPPin);  // get the current state of GPIO1 pin
  digitalWrite(ledAPPin, !state);     // set pin to the opposite state
}

// 从配置页面获取host及device id，用于连接野狗
char wilddog_host[50];
char device_id[10];
char changed_url[100];

// 实时通信引擎设备信息请求串
String deviceInfo;
// 重置wifi串
String resetStr;
// 数据更新串
String dataChanged;
// 设备状态串
String connections;
// 默认不重置
String reset = "0";
String resetConfig = "/reset.json";
String resetKey = "reset";

// host地址
String hostConfig = "/host.json";
String hostKey = "host";

// deviceId值
String deviceIdConfig = "/deviceId.json";
String deviceIdKey = "deviceId";

// data changed 监听地址
String changedUrlConfig = "/changedUrl.json";
String changedUrlKey = "changedUrl";

// 防止多次初始化引脚输入输出状态导致崩溃
int pinModeArrays[10];

bool connectInternet = true;

int requestTotal = 0;

//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    // wait serial port initialization
  }

  //exit after config instead of connecting
  //  wifiManager.setBreakAfterConfig(true);

  //clean FS, for testing
  //  SPIFFS.format();
  Serial.println("start setup");
  // 设置AP模式下led灯
  pinMode(ledAPPin, OUTPUT);

  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  // 获取处于重置的状态
  String resetTemp = String(readConfig(resetConfig, resetKey));
  if (resetTemp.length() != 0) {
    reset = resetTemp;
  }
  Serial.println("resetTemp:");
  Serial.println(resetTemp);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  //  WiFiManager wifiManager;
  //reset settings
  if (reset.equals(String("1"))) {
    wifiManager.resetSettings();
  }

  //  wifiManager.resetSettings();
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter cus_wilddog_host("wilddogHost", "wilddog host", readConfig(hostConfig, hostKey), 50);
  WiFiManagerParameter cus_device_id("dId", "device id", readConfig(deviceIdConfig, deviceIdKey), 10);
  WiFiManagerParameter cus_changed_url("changedUrl", "data changed url", readConfig(changedUrlConfig, changedUrlKey), 100);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //add all your parameters here
  wifiManager.addParameter(&cus_wilddog_host);
  wifiManager.addParameter(&cus_device_id);
  wifiManager.addParameter(&cus_changed_url);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  // 连接成功后清除定时器
  ticker.detach();
  // 同时保持AP灯长亮
  digitalWrite(ledAPPin, HIGH);

  // 打印分配的IP地址
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  //read updated parameters
  strcpy(wilddog_host, cus_wilddog_host.getValue());
  strcpy(device_id, cus_device_id.getValue());
  strcpy(changed_url, cus_changed_url.getValue());
  Serial.print("wilddog_host: ");
  Serial.println(wilddog_host);
  Serial.print("device_id: ");
  Serial.println(device_id);
  Serial.print("changed_url: ");
  Serial.println(changed_url);
  Serial.println();

  // 如果获取到的参数为空，则重置wifi并重新配置
  String wilddog_host_trim = String(wilddog_host);
  wilddog_host_trim.trim();
  if (wilddog_host_trim.length() == 0) {
    Serial.println("wilddog_host is empty!");
    resetESP();
    return;
  }
  String device_id_trim = String(device_id);
  device_id_trim.trim();
  if (device_id_trim.length() == 0) {
    Serial.println("device_id is empty!");
    resetESP();
    return;
  }
  String changed_url_trim = String(changed_url);
  changed_url_trim.trim();
  if (changed_url_trim.length() == 0) {
    Serial.println("changed_url is empty!");
    resetESP();
    return;
  }

  // 连接成功重置reset状态
  writeConfig(resetConfig, resetKey, "0");

  // 开始firebase
  Firebase.begin(wilddog_host, "");
  // 获取device id，初始化数据
  deviceInfo = "/gpio/" + String((char *)device_id);
  resetStr = "/device/" + String((char *)device_id) + "/reset";
  dataChanged = "/device/" + String((char *)device_id) + "/changed";
  connections = "/device/" + String((char *)device_id) + "/connections";
  // loop from the lowest pin to the highest:
  for (int i = 0; i < 10; i++) {
    pinModeArrays[i] = 0;
  }
  writeConfig(deviceIdConfig, deviceIdKey, String((char *)device_id));
  writeConfig(hostConfig, hostKey, String((char *)wilddog_host));
  writeConfig(changedUrlConfig, changedUrlKey, String((char *)changed_url));

}

void resetESP() {
  writeConfig(resetConfig, resetKey, "1");
  delay(3000);
  ESP.reset();
  delay(5000);
}
const char* host = "192.168.31.128";
void loop() {
  Serial.print("deviceInfo String: ");
  Serial.println(deviceInfo);
  Serial.print("dataChanged String: ");
  Serial.println(dataChanged);
  if (requestTotal % 300 == 0) {
    // 一分钟更新一次状态，告知服务器自己在线
    notifyDeviceOnline();
    if (requestTotal >= 300) {
      requestTotal = 0;
    }
  }
  requestTotal = requestTotal + 1;

  //是否从云端获取数据更新
  bool getChangedFromRealtimeData = false;

  bool getChangedSucceed = false;

  int changed = 0;

  if (getChangedFromRealtimeData) {
    // 1. 从云端直接获取数据是否更改
    changed = Firebase.getInt(dataChanged);
    if (Firebase.failed()) {
      Serial.println("Firebase get data changed failed");
      Serial.println(Firebase.error());
    } else {
      // 已收到更新数据通知，重置为未更新数据状态
      Firebase.set(dataChanged, 0);
      getChangedSucceed = true;
    }
  } else {
    // 2. 访问Android Things获取数据是否更改
    HTTPClient httpClient;
    Serial.println("[HTTP] begin...\n");
    Serial.println(String((char *)changed_url));
    // configure traged server and url
    httpClient.begin(changed_url); //HTTP
    // start connection and send HTTP header
    int httpCode = httpClient.GET();
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        String resp = httpClient.getString();
        USE_SERIAL.println(resp);
        changed = resp.toInt();
        getChangedSucceed = true;
      } else {
        USE_SERIAL.printf("[HTTP] GET... NOT OK, error: %s\n", httpClient.errorToString(httpCode).c_str());
      }
    } else {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
    }
    httpClient.end();
  }

  if (!getChangedSucceed) {
    if (connectInternet) {
      ticker.attach(0.5, tick);
    }
    connectInternet = false;
    // 请求失败后延迟2秒再请求
    delay(2000);
    return;
  } else {
    if (!connectInternet) {
      ticker.detach();
      connectInternet = true;
    }
    if (changed == 0) {
      // 没有数据更新，延迟200ms再查看是否有数据更新
      delay(200);
      return;
    }
    //通知设备我在线
    notifyDeviceOnline();
  }
  int count = 0;
  Serial.print("root:");
  FirebaseObject root = Firebase.get(deviceInfo);

  if (Firebase.failed()) {
    Serial.println("Firebase get failed");
    Serial.println(Firebase.error());
    ticker.attach(0.5, tick);
    connectInternet = false;
    return;
  } else {
    if (!connectInternet) {
      ticker.detach();
      connectInternet = true;
    }
  }

  bool resetBool = Firebase.getBool(resetStr);
  Serial.print("resetBool : ");
  Serial.println(resetBool);
  if (resetBool) {
    Firebase.set(resetStr, false);
    // 写入到配置文件中
    Serial.println("saving device config");
    writeConfig(resetConfig, resetKey, "1");
    delay(3000);
    ESP.reset();
    delay(5000);
    return;
  }

  JsonVariant peripheralJV = root.getJsonVariant();
  for ( const auto& kv : peripheralJV.as<JsonObject>() ) {
    // 获取peripheral下包含的开关实例
    Serial.println(kv.key);
    // 其kv.value作为jsonObject对象处理，即开关实例
    JsonObject& peripheral = kv.value.as<JsonObject>();
    String gpioStr = peripheral["gpio"];
    int gpio = gpioStr.toInt();
    Serial.print("gpio:");
    Serial.println(gpio);
    if (pinModeArrays[count] == 0) {
      // 不能多次设置引脚输入输出状态
      pinMode(gpio, OUTPUT);
      pinModeArrays[count] = 1;
    }
    count = count + 1;

    bool status = peripheral["status"];
    Serial.print("status:");
    Serial.print(status);
    // gpio号如果为-1，则不设置其值，是无效的gpio号
    digitalWrite(gpio, status);

  }

  // 每200毫秒更新一次数据
  delay(200);
}

void notifyDeviceOnline() {
  // 先false再true是为了让主机更新日期
  Firebase.set(connections, false);
  delay(100);
  Firebase.set(connections, true);
}

// 写配置文件
void writeConfig(String config_file, String key, String value) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json[key] = value;

  File configFile = SPIFFS.open(config_file, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.prettyPrintTo(Serial);
  json.printTo(configFile);
  configFile.close();
  Serial.println("success to write config file!");
  //end save
}

// 读取配置文件
char* readConfig(String config_file, String key) {
  char result[50] = "";
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(config_file)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File deviceFile = SPIFFS.open(config_file, "r");
      if (deviceFile) {
        Serial.println("opened config file");
        size_t size = deviceFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        deviceFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          json.prettyPrintTo(Serial);
          strcpy(result, json[key]);
          Serial.println("success to read json config");
        } else {
          Serial.println("failed to load json config");
        }
      } else {
        Serial.println("open device.json failed!");
      }
    } else {
      Serial.println("device.json not exist!");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  return result;
}
