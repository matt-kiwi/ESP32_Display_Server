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
void sendLoraMessage( const char * data );

const char* wifi_ssid = "EconodeDisplay";
const char* wifi_password = "123456789";
IPAddress ip_address;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
RH_RF95 rf95(LLG_CS, LLG_DI0);

StaticJsonDocument<2048> json_rx;
StaticJsonDocument<2048> json_tx;


// Set LED GPIO
const int ledPin = LED_BUILTIN;
// Stores LED state
String ledState;


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

void sendLoraMessage( const char * data ){
    rf95.send( (uint8_t *) data, strlen(data) );
    rf95.waitPacketSent();
}

void server_routes(){
  // https://github.com/me-no-dev/ESPAsyncWebServer#handlers-and-how-do-they-work
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });

  // Websockets init.
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve static files
  server.serveStatic("/", SPIFFS,"/html/");
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.printf("WS RX rawstring:%s\n",data);
    DeserializationError err = deserializeJson(json_rx,data);
    if (err) {
      Serial.printf("ERROR: deserializeJson() failed with: %s\n",err.c_str() );
      Serial.printf("Can not process websocket request\n");
      return;
    }
    const char* messageType = json_rx["messageType"];
    Serial.printf("WS RX->messageType:%s\n",messageType);

    // Route websocket message based on messageType
    // Display message
    if( strcmp(messageType,"display") == 0 ){
      const char* dTxt = json_rx["payload"]["dTxt"];
      const char* dColor = json_rx["payload"]["dColor"];
      int dBright =  json_rx["payload"]["dBright"];
      Serial.printf("WS RX display dColor:%s, dBright:%d, dTxt:\"%s\"\n",dColor,dBright,dTxt);
      sendLoraMessage(dTxt);
      return;
    }


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
  WiFi.softAP(wifi_ssid);
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