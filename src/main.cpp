#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <SPI.h>

AsyncWebServer server(80);

static const int NEXT_BAND = 15;
static const int bandPins[] = {5,18,0,16,17,4};
static const String bandName[] = {"6","10/12/15","17/20","30/40","80","160"};
int band = -1;

unsigned int previousMillis = 0;
unsigned const int interval = 30*1000;

void addFloat(JsonDocument *doc, String name, float value) {
    (*doc)[name].set(value);
}

void addInt(JsonDocument *doc, String name, int value) {
    (*doc)[name].set(value);
}

void addString(JsonDocument *doc, String name, String value) {
    (*doc)[name].set(value);
}

void addBool(JsonDocument *doc, String name, bool value) {
  if (value) {
    (*doc)[name].set("true");
  } else {
    (*doc)[name].set("false");
  }
}

void readBand() {
    for (int i = 0; i < sizeof(bandPins)/sizeof(int); i++) {
        if (digitalRead(bandPins[i])) {
            band = i;
        }
    }
    band = -1;
}

void getStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<1024> doc;
  if (band > 0) {
    addString(&doc,"band",bandName[band]);
  } else {
    addString(&doc,"band","unknown");
  }
  char buffer[1024];
  serializeJson(doc, buffer);
  
  request->send(200, "application/json", buffer);
}


void setupApi() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    getStatus(request);
  });
  server.on("/api/next", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(NEXT_BAND,true);
    delay(200);
    digitalWrite(NEXT_BAND,false);
    getStatus(request);
  });
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/static/", SPIFFS, "/");
  server.begin();
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500); 


  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if(!res) {
      Serial.println("Failed to connect");
      ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("Connected...yeey :)");
  }

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  pinMode(NEXT_BAND,OUTPUT);
  digitalWrite(NEXT_BAND,false);
  for (int pin : bandPins) {
    pinMode(pin,INPUT_PULLDOWN);
  }

  setupApi();
}


void loop() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  readBand();
  delay(1000);
}