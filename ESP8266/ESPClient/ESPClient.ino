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

#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>

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
//#define FIREBASE_HOST "example.firebaseio.com"
#define FIREBASE_HOST "example.wilddogio.com"
#define FIREBASE_AUTH ""

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
  // 设置AP模式下led灯
  pinMode(ledAPPin, OUTPUT);
  
  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

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
  // 开始firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  // 获取firebase初始化数据
  

}

void loop() {
  Serial.print("status: ");
  Serial.println(Firebase.getInt("redlight"));
//  Serial.println(Firebase.getBool("gpio/lp_iot_002/lp_iot_00201/status"));
  digitalWrite(ledPin, Firebase.getInt("redlight"));
  delay(200);
}
