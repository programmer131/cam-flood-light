/*
 * ip/config?token=flespi token here
 * ip/config?topic_id=id here
 * 192.168.18.106/config?token=2o3ij4o23rjo2i3nro24nion42t3n4t
 * 192.168.18.106/config?id=001&restart=true
 * ip/update to udpate firmware
 */

// https://github.com/knolleary/pubsubclient/issues/403#issuecomment-432098156

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
#include <TimeLib.h>
#include <TimeAlarms.h>
eSPIFFS fileSystem;

#define TEST_BUILD 1
// Update these with values suitable for your network.
#ifdef TEST_BUILD
#define LIGHT_ON 0
#define LIGHT_OFF 1
#define LIGHT_SWITCH BUILTIN_LED
#else
#define LIGHT_ON 1
#define LIGHT_OFF 0
#define LIGHT_SWITCH D8
#endif
enum states
{
  off = 0,
  on
};
struct hms_time
{
  int hours;
  int minutes;
  int seconds;
} auto_st_time, auto_en_time;

bool enable_detection = false;
bool publish_status = false;
bool esp_restart = false;
bool auto_mode = false;

AlarmId start_id=dtINVALID_ALARM_ID, end_id=dtINVALID_ALARM_ID, light_off_timer=dtINVALID_ALARM_ID;
unsigned long finish_time = 0, pause_time = 0;
const char *mqtt_server = "mqtt.flespi.io";
char mqtt_topic_id[4] = {0};
char mqtt_user_name[65] = {0}; //  "flespi token";
char device_alias[30] = {0};
int activate_light = 3; // only 0 and 1 is recognized at the moment
char tx_buf[512] = {0};
DynamicJsonDocument doc(1024);
enum states light_state = off;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5 * 3600, 60000);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
char pub_topic[20] = "header/status/";
char sub_topic[20] = "header/command/";

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
  doc.clear();
  doc["ident"] = WiFi.macAddress();
  doc["ssid"] = WiFi.SSID();
  doc["wifi_quality"] = getQuality();
  doc["ip"] = WiFi.localIP().toString();
  doc["obj_detection"] = enable_detection;
  doc["light_state"] = light_state;
  doc["dev_time"] = timeClient.getFormattedTime();
  doc["online"] = 1;
  doc["alias"] = device_alias;
  doc["auto_mode"] = auto_mode;
  JsonArray auto_mode_time = doc.createNestedArray("auto_mode_time");
  auto_mode_time.add(auto_st_time.hours);
  auto_mode_time.add(auto_st_time.minutes);
  auto_mode_time.add(auto_en_time.hours);
  auto_mode_time.add(auto_en_time.minutes);
  serializeJson(doc, tx_buf);
}
void generate_will_message()
{
  StaticJsonDocument<256> staticdoc;
  staticdoc["ident"] = WiFi.macAddress();
  staticdoc["online"] = 0;
  serializeJson(staticdoc, tx_buf);
}
void set_light(enum states new_state)
{
  static states active_light_state = off;
  if (new_state == active_light_state)
    return;
  active_light_state = new_state;
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
  light_state = new_state;
  publish_status = true;
}
void detection_enable()
{
  enable_detection = true;
  activate_light = 0;
  publish_status = true;
  Serial.print("detection:");
  Serial.println(enable_detection);
}
void detection_disable()
{
  enable_detection = false;
  activate_light = 0;
  publish_status = true;
  Serial.print("detection:");
  Serial.println(enable_detection);
}
void mode_set()
{
  Serial.print("mode set:");
  Serial.println(auto_mode);
  if(auto_mode)
  {
    Alarm.free(start_id); // start_id,end_id;
    Alarm.free(end_id);
    start_id = dtINVALID_ALARM_ID;
    end_id = dtINVALID_ALARM_ID;
    start_id = Alarm.alarmRepeat(auto_st_time.hours, auto_st_time.minutes, auto_st_time.seconds, detection_enable);
    end_id = Alarm.alarmRepeat(auto_en_time.hours, auto_en_time.minutes, auto_en_time.seconds, detection_disable);
  }
  else
  {
    Alarm.free(start_id); // start_id,end_id;
    Alarm.free(end_id);
    start_id = dtINVALID_ALARM_ID;
    end_id = dtINVALID_ALARM_ID;
  }
}
void callback(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<256> staticdoc;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char cmd_payload[256]={0};
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    cmd_payload[i]=payload[i];
  }
  Serial.println();
  DeserializationError error = deserializeJson(staticdoc, payload, length);
  if (error)
  {
    // publish some will
  }
  if (staticdoc.containsKey("motion"))
  {
    const char *key_val = staticdoc["motion"];
    if (strcmp(key_val, "detected") == 0 && enable_detection == true && (millis() - pause_time) > 8000)
    {
      activate_light = 1;
    }
    else if (strcmp(key_val, "finished") == 0 && enable_detection == true)
    {
      activate_light = 0;
      finish_time = millis();
    }
  }
  if (staticdoc.containsKey("auto_mode"))
  {
    auto_mode=staticdoc["auto_mode"]; 
    activate_light = 0;
    enable_detection=false;        
    mode_set();   
    publish_status = true;
  }
  if (staticdoc.containsKey("obj_detection"))
  {
    enable_detection = staticdoc["obj_detection"];
    activate_light = 0;
    publish_status = true;
    auto_mode=false; 
    mode_set();
  }
  if (staticdoc.containsKey("auto_mode_time"))
  {
    auto_st_time.hours = staticdoc["auto_mode_time"][0];
    auto_st_time.minutes = staticdoc["auto_mode_time"][1];
    auto_st_time.seconds = 0;
    auto_en_time.hours = staticdoc["auto_mode_time"][2];
    auto_en_time.minutes = staticdoc["auto_mode_time"][3];
    auto_en_time.seconds = 0;
    fileSystem.saveToFile("/json.txt", cmd_payload);
    publish_status = true;
  }
  if (staticdoc.containsKey("ping"))
  {
    publish_status = true;
  }
  if (staticdoc.containsKey("light_state"))
  {
    set_light((enum states)staticdoc["light_state"]);
    enable_detection = false;
    auto_mode=false; 
    mode_set();
    publish_status = true;
  }
}

void reconnect()
{
  // Loop until we're reconnected
  if (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    generate_will_message();
    // Create a random client ID
    String clientId = "CamLight-" + WiFi.macAddress();
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    // mqtt_user_name
    if (client.connect(clientId.c_str(), mqtt_user_name, "", pub_topic, 0, 0, tx_buf))
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
void handleConfig()
{
  httpServer.send(200, "text/plain", "New configuration will be applied after restart");
  for (uint8_t i = 0; i < httpServer.args(); i++)
  {
    if (httpServer.argName(i).equals("token"))
    {
      const char *newCharBuffer;
      String token = "";
      token += httpServer.arg(i);
      fileSystem.saveToFile("/token.txt", token);
      strcpy(mqtt_user_name, httpServer.arg(i).c_str());
    }
    if (httpServer.argName(i).equals("id"))
    {
      const char *newCharBuffer;
      String topic_id = "";
      topic_id += httpServer.arg(i);
      fileSystem.saveToFile("/id.txt", topic_id);
      strcpy(mqtt_topic_id, httpServer.arg(i).c_str());
    }
    if (httpServer.argName(i).equals("alias"))
    {
      const char *newCharBuffer;
      String alias = "";
      alias += httpServer.arg(i);
      fileSystem.saveToFile("/alias.txt", alias);
      strcpy(device_alias, httpServer.arg(i).c_str());
    }
    if (httpServer.argName(i).equals("restart"))
    {
      esp_restart = true;
    }
  }
}
void sync_time(void)
{
  Serial.println("syncing time");
  timeClient.update();
  setTime(timeClient.getEpochTime());
}
void two_sec_call()
{
  if (esp_restart)
    ESP.restart();
  if (!client.connected())
  {
    reconnect();
  }
}
void update_from_memory()
{
  char mem_buf[512];
  const char *newCharBuffer;
  if (fileSystem.openFromFile("/token.txt", newCharBuffer))
    strcpy(mqtt_user_name, newCharBuffer);
  if (fileSystem.openFromFile("/id.txt", newCharBuffer))
    strcpy(mqtt_topic_id, newCharBuffer);
  if (fileSystem.openFromFile("/alias.txt", newCharBuffer))
    strcpy(device_alias, newCharBuffer);
  if (fileSystem.openFromFile("/json.txt", newCharBuffer))
  {
    strcpy(mem_buf, newCharBuffer);
    DeserializationError error = deserializeJson(doc, mem_buf);
    if (error)
    {
      // publish some will
    }
    else
    {
      if (doc.containsKey("auto_mode_time") )
      {
        auto_st_time.hours = doc["auto_mode_time"][0];
        auto_st_time.minutes = doc["auto_mode_time"][1];
        auto_st_time.seconds = 0;
        auto_en_time.hours = doc["auto_mode_time"][2];
        auto_en_time.minutes = doc["auto_mode_time"][3];
        auto_en_time.seconds = 0;
      }
    }
  }
  else // set default time interval
  {
    auto_st_time.hours = 18;
    auto_st_time.minutes = 0;
    auto_st_time.seconds = 0;
    auto_en_time.hours = 6;
    auto_en_time.minutes = 10;
    auto_en_time.seconds = 0;
  }
  strcat(pub_topic, mqtt_topic_id);
  strcat(sub_topic, mqtt_topic_id);
  // strcat(will_topic,mqtt_topic_id);
  Serial.println(mqtt_user_name);
  Serial.println(mqtt_topic_id);
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
  update_from_memory();
  client.setBufferSize(512);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpUpdater.setup(&httpServer);
  httpServer.on("/config", handleConfig);
  httpServer.begin();
  Alarm.timerRepeat(60*60*2, sync_time);//sync time every 2 hours
  Alarm.timerRepeat(3600, send_device_status);//send status every hour
  Alarm.timerRepeat(2, two_sec_call);
  sync_time();
}
void send_device_status()
{
  generate_status_message();
  client.publish(pub_topic, tx_buf, 1);
}
void loop()
{  
  const unsigned long REFRESH_INTERVAL2 = 8000; // 8sec
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
  if (publish_status)
  {
    publish_status = 0;
    send_device_status();    
  }
  httpServer.handleClient();
  client.loop();
  Alarm.delay(100);
}
