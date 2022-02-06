/*
 * ip/config?token=flespi token here
 * 192.168.18.106/config?token=2o3ij4o23rjo2i3nro24nion42t3n4t
 * ip/update to udpate firmware
 */
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <Effortless_SPIFFS.h>
eSPIFFS fileSystem;

#define TEST_BUILD 1
// Update these with values suitable for your network.
#define LIGHT_ON 1
#define LIGHT_OFF 0
#ifdef TEST_BUILD
#define LIGHT_SWITCH BUILTIN_LED
#else
#define LIGHT_SWITCH D8
#endif

int enable_detection=0;
const unsigned long REFRESH_INTERVAL1 = 1000; // 1sec
const unsigned long REFRESH_INTERVAL2 = 8000; // 30sec
unsigned long lastRefreshTime1 = 0;
unsigned long lastRefreshTime2 = 0;
unsigned long detect_time=0, finish_time=0,pause_time=0;
const char* mqtt_server = "mqtt.flespi.io";
String mqtt_user_name="";//  "flespi token";
bool shouldSaveConfig = false;
int activate_light=3;//only 0 and 1 is recognized at the moment
int alert = 0;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5*3600, 60000);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (100)
char msg[MSG_BUFFER_SIZE];
int value = 0;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strncmp((char*)payload,"detected",length) == 0 && enable_detection==1 && (millis()-pause_time)>5000) {
     activate_light=1;
  } 
  else if(strncmp((char*)payload,"finished",length) == 0 && enable_detection==1)
  {
     activate_light=0;  
     finish_time = millis();
  }
  else if (strncmp((char*)payload,"disable",length) == 0){
    enable_detection=0;
    client.publish("header/status", "disable");
  } else if (strncmp((char*)payload,"enable",length) == 0){
    client.publish("header/status", "enable");
    enable_detection=1;
  }
  else if (strncmp((char*)payload,"on",length) == 0){
    client.publish("header/status", "on");
    digitalWrite(LIGHT_SWITCH, LIGHT_ON); 
    alert=0;
  }
  else if (strncmp((char*)payload,"off",length) == 0){
    client.publish("header/status", "off");
    digitalWrite(LIGHT_SWITCH, LIGHT_OFF); 
    alert=0;
  }
  else if (strncmp((char*)payload,"alert",length) == 0){
    client.publish("header/status", "alert");
    alert=1;
  }
}

void reconnect() {
  // Loop until we're reconnected
  if (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "CamLightClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    //mqtt_user_name
    if (client.connect(clientId.c_str(),mqtt_user_name.c_str(),"")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("header/status", "powered-up");
      // ... and resubscribe
      client.subscribe("header/commands");
    } else {
      //Serial.print("failed, rc=");
      
    }
  }
}
void handleToken() {
  String token="";
  if(httpServer.argName(0).equals("token"))
  {
    token+=httpServer.arg(0);
    fileSystem.saveToFile("/token.txt",token);
    httpServer.send(200, "text/plain", "hello from esp8266!");
    ESP.restart();
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
 }

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(LIGHT_SWITCH, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(LIGHT_SWITCH, LIGHT_OFF); 
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  if (!fileSystem.checkFlashConfig()) {
    Serial.println("Flash size was not correct! Please check your SPIFFS config and try again");
  }
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP", "password");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  //  fileSystem.saveToFile("/CharBuffer.txt", newCharBuffer);    
  timeClient.begin();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpUpdater.setup(&httpServer);
  httpServer.on("/config", handleToken);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  fileSystem.openFromFile("/token.txt", mqtt_user_name);
  Serial.println(mqtt_user_name);
}

void loop() {    
  if(millis() - lastRefreshTime1 >= REFRESH_INTERVAL1)
  { 
    if (!client.connected()) {
      reconnect();
      }     
      if(alert>0)
      {        
        int num=alert%2;
        alert++;
        digitalWrite(LIGHT_SWITCH, num);   
        if (alert>10)
        {
          alert=0;   
          digitalWrite(LIGHT_SWITCH, LIGHT_OFF);   
        }
      }
    lastRefreshTime1 = millis();    
  }
  if(activate_light == 0 && (millis() - finish_time >= REFRESH_INTERVAL2))
  { 
    pause_time=millis();
    digitalWrite(LIGHT_SWITCH, LIGHT_OFF);
    client.publish("header/status", "off");
    activate_light=3;
  }
  if(activate_light == 1)
  {
    activate_light=3;
    client.publish("header/status", "on");
    digitalWrite(LIGHT_SWITCH, LIGHT_ON);   
  }
  httpServer.handleClient();  
  client.loop();  
}
