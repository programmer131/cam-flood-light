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

bool enable_detection = 0;
bool publish_status = false;
const unsigned long REFRESH_INTERVAL1 = 1000; // 1sec
const unsigned long REFRESH_INTERVAL2 = 8000; // 8sec
unsigned long lastRefreshTime1 = 0;
unsigned long lastRefreshTime2 = 0;
unsigned long detect_time = 0, finish_time = 0, pause_time = 0;
const char *mqtt_server = "mqtt.flespi.io";
String mqtt_user_name = ""; //  "flespi token";
int activate_light = 3;     // only 0 and 1 is recognized at the moment
char status_buf[256];
enum states light_state = off;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5 * 3600, 60000);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
char pub_topic[] = "header/status";
char sub_topic[] = "header/command";
char will_topic[] = "header/will";

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
  doc["light_status"] = light_state;
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
  publish_status=true;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strncmp((char *)payload, "detected", length) == 0 && enable_detection == 1 && (millis() - pause_time) > 5000)
  {
    activate_light = 1;
  }
  else if (strncmp((char *)payload, "finished", length) == 0 && enable_detection == 1)
  {
    activate_light = 0;
    finish_time = millis();
  }
  else if (strncmp((char *)payload, "disable", length) == 0)
  {
    enable_detection = 0;
    publish_status=true;
  }
  else if (strncmp((char *)payload, "enable", length) == 0)
  {
    enable_detection = 1; 
    publish_status=true;
  }
  else if (strncmp((char *)payload, "on", length) == 0)
  {
    set_light(on);
    enable_detection = 0;
  }
  else if (strncmp((char *)payload, "off", length) == 0)
  {
    set_light(off);
    enable_detection = 0;
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
    if (client.connect(clientId.c_str(), mqtt_user_name.c_str(), "", will_topic, 0, 0, last_will))
    {
      Serial.println("connected");
      client.subscribe(sub_topic);
    }
    else
    {
      // Serial.print("failed, rc=");
    }
  }
}
void handleToken()
{
  String token = "";
  if (httpServer.argName(0).equals("token"))
  {
    token += httpServer.arg(0);
    fileSystem.saveToFile("/token.txt", token);
    httpServer.send(200, "text/plain", "token saved successfully");
    ESP.restart();
  }
  else
  {
    httpServer.send(200, "text/plain", "token not found");
  }
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
  timeClient.begin();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpUpdater.setup(&httpServer);
  httpServer.on("/config", handleToken);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  fileSystem.openFromFile("/token.txt", mqtt_user_name);
  Serial.println(mqtt_user_name);
  publish_status=true;
}

void loop()
{
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
