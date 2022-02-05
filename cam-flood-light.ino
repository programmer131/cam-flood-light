/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
// Update these with values suitable for your network.
const char* host = "esp8266-webupdate";
#define LIGHT_ON 1
#define LIGHT_OFF 0
#define LIGHT_SWITCH D8
int enable_detection=0;
const unsigned long REFRESH_INTERVAL1 = 1000; // 1sec
const unsigned long REFRESH_INTERVAL2 = 10000; // 30sec
unsigned long lastRefreshTime1 = 0;
unsigned long lastRefreshTime2 = 0;
unsigned long detect_time=0, finish_time=0,pause_time=0;
const char* ssid = "ssid";
const char* password = "passwd";
const char* mqtt_server = "192.168.0.100";

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5*3600, 60000);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
int activate_light=0;
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if (strncmp((char*)payload,"detected",length) == 0 && enable_detection==1 && (millis()-pause_time)>10000) {
     activate_light=1;
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } 
  else if(strncmp((char*)payload,"finished",length) == 0 && enable_detection==1)
  {
     activate_light=0;  
     finish_time = millis();
  }
  else if (strncmp((char*)payload,"disable",length) == 0){
    enable_detection=0;
    client.publish("light", "disable");
      // Turn the LED off by making the voltage HIGH
  } else if (strncmp((char*)payload,"enable",length) == 0){
    client.publish("light", "enable");
    enable_detection=1;
      // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  if (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("motion");
    } else {
      //Serial.print("failed, rc=");
      
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(LIGHT_SWITCH, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(LIGHT_SWITCH, LIGHT_OFF); 
  Serial.begin(115200);
  setup_wifi();
  timeClient.begin();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void loop() {    
  if(millis() - lastRefreshTime1 >= REFRESH_INTERVAL1)
  { 
    if (!client.connected()) {
      reconnect();
      }        
    lastRefreshTime1 = millis();    
  }
  if(activate_light == 0 && (millis() - finish_time >= REFRESH_INTERVAL2))
  { 
    pause_time=millis();
    digitalWrite(LIGHT_SWITCH, LIGHT_OFF);
    client.publish("light", "off");
    activate_light=3;
  }
  if(activate_light == 1)
  {
    activate_light=3;
    client.publish("light", "on");
    digitalWrite(LIGHT_SWITCH, LIGHT_ON);   
  }
  httpServer.handleClient();  
  client.loop();  
}
