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

#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <FirebaseArduino.h>   //https://github.com/firebase/firebase-arduino

//#include <ArduinoJson.h>  //https://github.com/bblanchon/ArduinoJson

// wifi ap connect
// needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

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

// Set these to run example.
//#define FIREBASE_HOST "androidthing-5c6d9.firebaseio.com"
#define FIREBASE_HOST "wd8078052585upgyqs.wilddogio.com"
#define FIREBASE_AUTH ""

// 从配置页面获取device id，用于连接野狗
char device_id[10];

String deviceInfo;

// 防止多次初始化引脚输入输出状态
int pinModeArrays[10];

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
  // Memory pool for JSON object tree.
  //
  // Inside the brackets, 200 is the size of the pool in bytes,
  // If the JSON object is more complex, you need to increase that value.
  // See https://bblanchon.github.io/ArduinoJson/assistant/
  StaticJsonBuffer<200> jsonBuffer;
  
  // 设置AP模式下led灯
  pinMode(ledAPPin, OUTPUT);
  
  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  // wifiManager.resetSettings();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter cus_device_id("dId", "device id", "esp_01", 10);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //add all your parameters here
  wifiManager.addParameter(&cus_device_id);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
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
  strcpy(device_id, cus_device_id.getValue());
  Serial.print("device_id: ");
  Serial.println(device_id);
  Serial.println();
  // 开始firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  // 获取device id，初始化数据
  deviceInfo = "device/" + String((char *)device_id);
    
   // loop from the lowest pin to the highest:
  for (int i = 0; i < 10; i++) {
    pinModeArrays[i] = 0;
  }

}

void loop() {
  Serial.print("status: ");
  int count = 0;
  FirebaseObject root = Firebase.get(deviceInfo);
  if (Firebase.failed()) {
    Serial.println("Firebase get failed");
    Serial.println(Firebase.error());
    return;
  }

  JsonVariant peripheralJV = root.getJsonVariant("peripheral");
  for( const auto& kv : peripheralJV.as<JsonObject>() ) {
    // 获取peripheral下包含的开关实例
    Serial.println(kv.key);
    int pGpio = -1;
    // 其kv.value作为jsonObject对象处理，即开关实例
    for( const auto& pin : kv.value.as<JsonObject>() ) {
      String key = String((char *)pin.key);
      if(String("pGpio").equals(key)){
        pGpio = pin.value.as<int>();
        Serial.print("pGpio:");  
        Serial.print(pGpio);
        Serial.print(" ");
        if(pinModeArrays[count] == 0){
          // 不能多次设置引脚输入输出状态
          pinMode(pGpio, OUTPUT);
          pinModeArrays[count] = 1;
        }
        count = count + 1;
      } else if(String("pStatus").equals(key)){
        bool pStatus = pin.value.as<bool>();
        Serial.print("pStatus:");
        Serial.println(pStatus);
        // gpio号如果为-1，则不设置其值，是无效的gpio号
        if(pGpio != -1){
          digitalWrite(pGpio, pStatus);
        }
      }
    }
    
//    String pInfo = deviceInfo + "/peripheral/" + String((char *)kv.key);
//    Serial.print("pInfo:");  
//    Serial.println(pInfo);
//    FirebaseObject peripheral = Firebase.get(pInfo);
//    int pGpio = peripheral.getInt("pGpio");
//    Serial.print("pGpio:");  
//    Serial.println(pGpio);
//    if(pinModeArrays[count] == 0){
//      // 不能多次设置引脚输入输出状态
//      pinMode(pGpio, OUTPUT);
//      pinModeArrays[count] = 1;
//     }
//    count = count + 1;
//    bool pStatus = peripheral.getBool("pStatus");
//    Serial.print("pStatus:");
//    Serial.println(pStatus);
//    digitalWrite(pGpio, pStatus);
    
    }

  // 每200毫秒更新一次数据
  delay(200);
}
