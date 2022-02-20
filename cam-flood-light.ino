/*
 * ip/config?token=flespi token here
 * ip/config?topic_id=id here
 * 192.168.18.106/config?token=2o3ij4o23rjo2i3nro24nion42t3n4t
 * 192.168.18.106/config?id=1001&restart=true
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
#include <ArduinoJson.h>
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
enum states
{
  off=0,
  on
};

bool enable_detection = false;
bool publish_status = false;

unsigned long  finish_time = 0, pause_time = 0;
const char *mqtt_server = "mqtt.flespi.io";
char mqtt_topic_id[4];
char mqtt_user_name[65]; //  "flespi token";
int activate_light = 3;     // only 0 and 1 is recognized at the moment
char status_buf[256];
enum states light_state = off;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5 * 3600, 60000);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
char pub_topic[20] = "header/status/";
char sub_topic[20] = "header/command/";
char will_topic[20] = "header/will/";

int getQuality()
{
  if (WiFi.status() != WL_CONNECTED)
    return -1;
  int dBm = WiFi.RSSI();
  if (dBm <= -100)
    return 0;
  if (dBm >= -50)
    return 100;
  return 2 * (dBm + 100);
}
void generate_status_message()
{
  StaticJsonDocument<256> doc;
  doc["ident"] = WiFi.macAddress();
  doc["ssid"] = WiFi.SSID();
  doc["wifi_quality"] = getQuality();
  doc["ip"] = WiFi.localIP().toString();
  doc["auto_mode"] = enable_detection;
  doc["light_state"] = light_state;
  serializeJson(doc, status_buf);
}
void set_light(enum states new_state)
{
  static states active_light_state=off;
  if(new_state==active_light_state)
    return;
  active_light_state=new_state;
  switch (new_state)
  {
  case off:
    digitalWrite(LIGHT_SWITCH, LIGHT_OFF);
    break;
  case on:
    digitalWrite(LIGHT_SWITCH, LIGHT_ON);
    break;
  default:
    break;
  }
  light_state=new_state;
  publish_status=true;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<200> doc;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  DeserializationError error =deserializeJson(doc, payload, length);
  if (error){
    //publish some will
  }
  if(doc.containsKey("motion"))
  {
    const char* key_val=doc["motion"];
    if (strcmp(key_val, "detected") == 0 && enable_detection == true && (millis() - pause_time) > 5000)
    {
      activate_light = 1;
    }
    else if (strcmp(key_val, "finished") == 0 && enable_detection == true)
    {
      activate_light = 0;
      finish_time = millis();
    }
  }
  if(doc.containsKey("auto_mode"))
  {    
      enable_detection = doc["auto_mode"];
      publish_status=true;
  }
  if(doc.containsKey("light_state"))
  {
    set_light((enum states)doc["light_state"]);
    enable_detection = false;
  }
  else if (strncmp((char *)payload, "on", length) == 0)
  {
    set_light(on);
    enable_detection = false;
  }
  else if (strncmp((char *)payload, "off", length) == 0)
  {
    set_light(off);
    enable_detection = false;
  }
}

void reconnect()
{
  char *last_will = "{\"will\":0}";
  // Loop until we're reconnected
  if (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "CamLight-" + WiFi.macAddress();
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    // mqtt_user_name
    if (client.connect(clientId.c_str(), mqtt_user_name, "", will_topic, 0, 0, last_will))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish(pub_topic, "powered-up");
      // ... and resubscribe
      client.subscribe(sub_topic);
    }
    else
    {
      // Serial.print("failed, rc=");
    }
  }
}
void handleConfig()
{  
  for (uint8_t i = 0; i < httpServer.args(); i++)
  {
    if (httpServer.argName(i).equals("token"))
    {
      const char* newCharBuffer;
      String token = "";
      token += httpServer.arg(i);
      fileSystem.saveToFile("/token.txt", token);
      fileSystem.openFromFile("/token.txt", newCharBuffer);
      strcpy(mqtt_user_name,newCharBuffer);
      Serial.println(mqtt_user_name);
      Serial.println(mqtt_topic_id);
    }
    if (httpServer.argName(i).equals("id"))
    {
      const char* newCharBuffer;
      String topic_id = "";
      topic_id += httpServer.arg(i);
      fileSystem.saveToFile("/id.txt", topic_id);
      fileSystem.openFromFile("/id.txt", newCharBuffer);
      strcpy(mqtt_topic_id,newCharBuffer);
      Serial.println(mqtt_user_name);
      Serial.println(mqtt_topic_id);
    }
    if(httpServer.argName(i).equals("restart"))
    {
      client.disconnect();
      httpServer.send(200, "text/plain", "id saved successfully");
      ESP.restart();
    }  
  } 
  httpServer.send(200, "text/plain", "New configuration will be applied after restart"); 
}
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++)
  {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
}

void setup()
{
  const char* newCharBuffer;
  pinMode(LIGHT_SWITCH, OUTPUT); // Initialize the pin as an output
  digitalWrite(LIGHT_SWITCH, LIGHT_OFF);
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  if (!fileSystem.checkFlashConfig())
  {
    Serial.println("Flash size was not correct! Please check your SPIFFS config and try again");
  }
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP", "password");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  //  fileSystem.saveToFile("/CharBuffer.txt", newCharBuffer);
  timeClient.begin();
  fileSystem.openFromFile("/token.txt", newCharBuffer);
  strcpy(mqtt_user_name,newCharBuffer);
  fileSystem.openFromFile("/id.txt", newCharBuffer);
  strcpy(mqtt_topic_id,newCharBuffer);  
  strcat(pub_topic,mqtt_topic_id);
  strcat(sub_topic,mqtt_topic_id);
  strcat(will_topic,mqtt_topic_id);
  Serial.println(mqtt_user_name);
  Serial.println(mqtt_topic_id);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpUpdater.setup(&httpServer);
  httpServer.on("/config", handleConfig);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();  
  publish_status=true;
}

void loop()
{
  const unsigned long REFRESH_INTERVAL1 = 1000; // 1sec
  const unsigned long REFRESH_INTERVAL2 = 8000; // 8sec
  static unsigned long lastRefreshTime1 = 0;
  static unsigned long lastRefreshTime2 = 0;
  if (millis() - lastRefreshTime1 >= REFRESH_INTERVAL1)
  {
    if (!client.connected())
    {
      reconnect();
    }
    lastRefreshTime1 = millis();
  }
  if (activate_light == 0 && (millis() - finish_time >= REFRESH_INTERVAL2))
  {
    pause_time = millis();
    set_light(off);
    activate_light = 3;
  }
  if (activate_light == 1)
  {
    set_light(on);
    activate_light = 3;
  }
  if(publish_status)
  {    
    publish_status=0;
    generate_status_message();
    client.publish(pub_topic, status_buf);
  }
  httpServer.handleClient();
  client.loop();
}
