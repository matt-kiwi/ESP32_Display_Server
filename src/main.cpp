#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <SPI.h>
#include "SPIFFS.h"
#include <RH_RF95.h>
#include <TimeLib.h>
#include <cstring>
#include <string>
#include <ArduinoJson.h>


//Libraries for OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// lilygo T3 v2.1.6
// lora SX1276/8
#define LLG_SCK 5
#define LLG_MISO 19
#define LLG_MOSI 27
#define LLG_CS  18
#define LLG_RST 23
#define LLG_DI0 26
#define LLG_DI1 33
#define LLG_DI2 32


//OLED pins
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Forward declerations
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

const char* wifi_ssid = "ESP32-Access-Point";
const char* wifi_password = "123456789";
IPAddress ip_address;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
RH_RF95 rf95(LLG_CS, LLG_DI0);

StaticJsonDocument<1024> json_rx;
StaticJsonDocument<1024> json_tx;


// Set LED GPIO
const int ledPin = LED_BUILTIN;
// Stores LED state
String ledState;


void sendLoraMessage( const char * data );

// Replaces placeholder with LED state value
String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if(digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

void init_oled(){
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  //initialize OLED
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("IP:" );
  display.print(ip_address );
  display.display();
}

void action_update(AsyncWebServerRequest *request)
{
  // Step through POST params
  int params = request->params();
  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter* p = request->getParam(i);
    // Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    if( strcmp(p->name().c_str(), "d_text\0") == 0 ){
      Serial.printf(" Found: %s ",p->value().c_str());
      sendLoraMessage(p->value().c_str());
    }
  }
  request->send(SPIFFS, "/index.html", String(), false, processor);
}

void sendLoraMessage( const char * data ){
    rf95.send( (uint8_t *) data, strlen(data) );
    rf95.waitPacketSent();
}



void server_routes(){
  // https://github.com/me-no-dev/ESPAsyncWebServer#handlers-and-how-do-they-work
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  // Websockets init.
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
    Serial.println("Send index.html");
  });

  // Process LCD update
  server.on("/update", HTTP_POST, action_update);

  // Serve static files
  server.serveStatic("/static/", SPIFFS,"/static/");
  
  // Route to set GPIO to HIGH
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
    uint8_t data[] = "on";
    digitalWrite(ledPin, HIGH);
    request->send(SPIFFS, "/index.html", String(), false, processor);
    rf95.send(data, sizeof(data));
    rf95.waitPacketSent();
  });
  
  // Route to set GPIO to LOW
  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
    uint8_t data[] = "off";
    digitalWrite(ledPin, LOW);
    request->send(SPIFFS, "/index.html", String(), false, processor);
    rf95.send(data, sizeof(data));
    rf95.waitPacketSent();
  });
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.printf("WS data:%s\n",data);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
 void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void setup(){
  Serial.begin(115200);
  SPI.begin();
  if (!rf95.init()){
    Serial.println("LoRa Radio: init failed.");
  } else {
    Serial.println("LoRa Radio: init OK!");
  }
  rf95.setTxPower(10, false);
  rf95.setFrequency(915.0);
  pinMode(ledPin,OUTPUT);
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  WiFi.softAP("EconodeDisplay");
  ip_address = WiFi.softAPIP();
  Serial.println(ip_address);
  dnsServer.start(53, "*", ip_address );
  server_routes();

  init_oled();

  // Start server
  server.begin();
  Serial.print("Webserver started on IP:");
  Serial.println(ip_address);
  

}

time_t time1 = 0;
void loop(){
  char data[100];
  dnsServer.processNextRequest();
  ws.cleanupClients();
  delay(50);
  if( now() - time1 > 15 ){
    time1 = now();
    Serial.println("ping...");
    ws.textAll("ping.. ");
    json_tx["age"]= 21;
    json_tx["color"] = "red";
    size_t len = serializeJson(json_tx, data);
    ws.textAll(data, len);
    Serial.printf("JSON:: %s\n",data);
    // uint8_t data[] = "ping";
    // rf95.send(data, sizeof(data));
    // rf95.waitPacketSent();
  } 
}