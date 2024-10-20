#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncHTTPUpdateServer.h>
#include <SPIFFS.h>
#include <SPI.h>

AsyncWebServer server(80);
ESPAsyncHTTPUpdateServer updateServer;

static const int NEXT_BAND = 15;
static const int MUX_SIG = 34;
static const int MUX_EN = 26;
static const int MUX_S0 = 25;
static const int MUX_S1 = 33;
static const int MUX_S2 = 32;
static const int MUX_S3 = 35; // bad choice - input only. not neded for this application though
static const String bandName[] = {"160","10/12/15","17/20","6","80","30/40"};
int bandVoltages[] = {0,0,0,0,0,0};
int band = -1;
unsigned long ota_progress_millis = 0;

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

void addIntArray(JsonDocument *doc, String name, int value[]) {
  JsonArray doc_0 = (*doc).createNestedArray(name);
  for (int i = 0; i < 6; i++) {
    doc_0.add(bandVoltages[i]);
  }
}

int readAnalog(int channel) {
  //Serial.printf("select %d,%d,%d\n",HIGH && (channel & B00000001),HIGH && (channel & B00000010),HIGH && (channel & B00000100));
  digitalWrite(MUX_S0,HIGH && (channel & B00000001));
  digitalWrite(MUX_S1,HIGH && (channel & B00000010));
  digitalWrite(MUX_S2,HIGH && (channel & B00000100));
  digitalWrite(MUX_EN,LOW);
  delay(10);
  //digitalWrite(MUX_S3,HIGH && (channel & B00001000));
  int val = analogReadMilliVolts(MUX_SIG);
  digitalWrite(MUX_EN,HIGH);
  //Serial.printf("channel %d = %d\n", channel, val);
  return val;
}

void readBand() {
    //Serial.println("-----");
    band = -1;
    for (int i = 0; i < 6; i++) {
      bandVoltages[i] = readAnalog(i);
      if (bandVoltages[i] > 1000) {
          band = i;
      }
    }
}

void getStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<1024> doc;
  if (band >= 0) {
    addString(&doc,"band",bandName[band]);
  } else {
    addString(&doc,"band","unknown");
  }
  addIntArray(&doc,"voltages",bandVoltages);
  char buffer[1024];
  serializeJson(doc, buffer);
  
  request->send(200, "application/json", buffer);
}

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
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
  server.on("/api/longpress", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(NEXT_BAND,true);
    delay(4000);
    digitalWrite(NEXT_BAND,false);
    getStatus(request);
  });
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/static/", SPIFFS, "/");
  updateServer.setup(&server);
  updateServer.setup(&server,OTA_USER,OTA_PASSWORD);
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

  analogReadResolution(13);
  analogSetAttenuation(ADC_ATTENDB_MAX);
  pinMode(NEXT_BAND,OUTPUT);
  digitalWrite(NEXT_BAND,false);
  pinMode(MUX_S0,OUTPUT);
  pinMode(MUX_S1,OUTPUT);
  pinMode(MUX_S2,OUTPUT);
  //pinMode(MUX_S3,OUTPUT);
  pinMode(MUX_SIG,ANALOG);
  pinMode(MUX_EN,OUTPUT);

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
  delay(500);
}