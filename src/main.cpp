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

uint16_t messageId = 9;
struct   displayPacket_t {
  uint8_t packetType =1;
  uint8_t filler = 0;
  uint16_t messageId;
  uint8_t dBright;
  uint8_t dColor; // 1=Red, 2=Blue, 3=Green, 4=White
  char dTxt[14];
};
displayPacket_t displayPacket;

struct  displayPacketAck_t {
  uint8_t packetType =1;
  uint16_t messageId;
  uint8_t dBright; // 1=Low, 2=Medium, 3=Bright
  uint8_t dColor; // 1=Red, 2=Green, 3=Blue, 4=White
  char * dTxt;
};
displayPacketAck_t displayPacketAct;


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
  display.setTextSize(2);
  display.setCursor(0,0);
  //display.print(ip_address );
  display.print("Econode");
  display.display();
}

void sendLoraMessage( uint8_t * data, unsigned int len ){
    rf95.send( data, len );
    rf95.waitPacketSent();
}

void sendLoraMessage( const char * data ){
  rf95.send( (uint8_t *) data, strlen(data) );
  rf95.waitPacketSent();
}

uint8_t colorLookup( const char * data ){
  // 1=Red, 2=Green, 3=Blue, 4=White
  int8_t color = 1; // Default to red
  if( strcmp(data,"RED")==0) color = 1;
  if( strcmp(data,"GREEN")==0) color = 2;
  if( strcmp(data,"BLUE")==0) color = 3;
  if( strcmp(data,"WHITE")==0) color = 4;
  return color;
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
      displayPacket.messageId = messageId++;
      displayPacket.dBright = dBright;
      displayPacket.dColor = colorLookup(dColor);
      memset(displayPacket.dTxt,0,sizeof(displayPacket.dTxt));
      strncpy(displayPacket.dTxt,dTxt,sizeof(displayPacket.dTxt)-1);
      Serial.printf("WS RX display messageId:%d dColor:%d, dBright:%d, dTxt:\"%s\"\n",
      displayPacket.messageId,displayPacket.dColor,displayPacket.dBright,displayPacket.dTxt);
      uint8_t * buf = reinterpret_cast<uint8_t*>(&displayPacket);
      sendLoraMessage(buf, sizeof(displayPacket) );
      //
      display.begin();
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(2);
      display.setCursor(0,0);
      display.print(dTxt);
      display.display();
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
  rf95.setTxPower(22, false);
  rf95.setFrequency(915.0);

  // rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
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
  dnsServer.processNextRequest();
  ws.cleanupClients();
  delay(50);
  if( now() - time1 > 15 ){
    time1 = now();
    // Do some stuff every 15 seconds
  } 
}