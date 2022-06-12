/*
 * ip/config?token=flespi token here
 * ip/config?topic_id=id here
 * 192.168.18.106/config?token=2o3ij4o23rjo2i3nro24nion42t3n4t
 * 192.168.18.106/config?id=001&restart=true
 * ip/update to udpate firmware
 */

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Effortless_SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecureBearSSL.h>

eSPIFFS fileSystem;

#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

#define VERSION 20
// Update these with values suitable for your board
#define ESP_01
//#define WEMOS_D1_MINI_PROD
//#define WEMOS_D1_MINI_DEMO
#ifdef ESP_01
#define LIGHT_ON 0
#define LIGHT_OFF 1
#define LIGHT_SWITCH 0
#endif
#ifdef WEMOS_D1_MINI_DEMO
#define LIGHT_ON 0
#define LIGHT_OFF 1
#define LIGHT_SWITCH BUILTIN_LED
#endif
#ifdef WEMOS_D1_MINI_PROD
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
unsigned int auto_start_min,auto_end_min;

bool enable_detection = false;
bool publish_status = false;
bool esp_restart = false;
bool auto_mode = false;

unsigned int finish_time = 0, pause_time = 0;
const char *mqtt_server = "mqtt.flespi.io";
char mqtt_topic_id[4] = {0};
char mqtt_user_name[65] = {0}; //  "flespi token";
char device_alias[30] = {0};
int device_group=99;
unsigned int motion_finish_off_pause=5;
unsigned int motion_finish_on_pause=20;
int activate_light = 3; // only 0 and 1 is recognized at the moment
char tx_buf[512] = {0};
DynamicJsonDocument doc(1024);
enum states light_state = off;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 5 * 3600, 6000);
ESP8266WebServer httpServer(80);
char pub_topic[20] = "header/status/";
char sub_topic[20] = "header/command/";

String location_city;

void fw_update(char *url);
void generate_status_message();
void generate_will_message();

void update_auto_mode_minutes()
{
  auto_start_min=60*auto_st_time.hours+auto_st_time.minutes;
  auto_end_min=60*auto_en_time.hours+auto_en_time.minutes-30;
}
void get_sunrise_sunset_time(void)
{
   StaticJsonDocument<64> filter;
   filter["location"]["city"]=true; // true
   filter["sunrise"] = true;
   filter["sunset"] = true;
   doc.clear();
   std::unique_ptr<BearSSL::WiFiClientSecure>client1(new BearSSL::WiFiClientSecure);
   client1->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client1, "https://api.ipgeolocation.io/astronomy?apiKey=4ec65b0f872b4c2b9cea39c982f92af0")) {  // HTTPS

      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        
        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          //String payload_d = https.getString();
          DeserializationError error = deserializeJson(doc,https.getString(),DeserializationOption::Filter(filter));
          if (!error) 
          {
            location_city = doc["location"]["city"].as<String>(); // "Islamabad"            
            const char* sunrise = doc["sunrise"]; // "04:59"
            const char* sunset = doc["sunset"];// "19:11"
            char *ptr=NULL;            
            int index=0;
            char *strings[6];
            char Buf[10];
            strcpy(Buf,sunset);
            ptr = strtok(Buf, ":");            
             while (ptr != NULL)
             {
                strings[index] = ptr;
                index++;
                ptr = strtok(NULL, ":");
             }
             auto_st_time.hours = atoi(strings[0]);
             auto_st_time.minutes = atoi(strings[1]);
             auto_st_time.seconds = 0;
             strcpy(Buf,sunrise);
             ptr = strtok(Buf, ":");  // delimiter     
             while (ptr != NULL)
             {
                strings[index] = ptr;
                index++;
                ptr = strtok(NULL, ":");
             }            
            auto_en_time.hours = atoi(strings[2]);
            auto_en_time.minutes = atoi(strings[3]);
            auto_en_time.seconds = 0;
          }
        }
      } else {
           }

      https.end();
    } else {
       }
}

void generate_status_message()
{
  doc.clear();
  doc["ident"] = WiFi.macAddress();
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["city"]=location_city;
  doc["obj_detection"] = enable_detection;
  doc["light_state"] = light_state;
  doc["dev_time"] = timeClient.getFormattedTime();
  doc["online"] = 1;
  doc["alias"] = device_alias;
  doc["auto_mode"] = auto_mode;
  doc["fw_version"]=VERSION;
  doc["device_group"]=device_group;
  doc["on_pause"]=motion_finish_on_pause;
  doc["off_pause"]=motion_finish_off_pause;
  
  JsonArray auto_mode_time = doc.createNestedArray("auto_mode_time");
  auto_mode_time.add(auto_st_time.hours);
  auto_mode_time.add(auto_st_time.minutes);
  auto_mode_time.add(auto_en_time.hours);
  auto_mode_time.add(auto_en_time.minutes);
  serializeJson(doc, tx_buf);
}
void generate_will_message()
{
  StaticJsonDocument<100> staticdoc;
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

void callback(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<100> staticdoc;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char cmd_payload[256]={0};
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    cmd_payload[i]=payload[i];
  }
  Serial.println();
  DeserializationError error = deserializeJson(staticdoc, payload, length);
  unsigned int currentSec = (millis()/1000);
  if (error)
  {
    // publish some will
  }
  if (staticdoc.containsKey("motion"))
  {
    const char *key_val = staticdoc["motion"];
    if (strcmp(key_val, "detected") == 0 && enable_detection == true && (currentSec - pause_time) > motion_finish_off_pause)
    {
      activate_light = 1;
    }
    else if (strcmp(key_val, "finished") == 0 && enable_detection == true)
    {
      activate_light = 0;
      finish_time = currentSec;
    }
  }
  if(staticdoc.containsKey("device_group"))
  {
    int group=staticdoc["device_group"];    
    if(group!=device_group)
    return;
  }
  if (staticdoc.containsKey("auto_mode"))
  {
    auto_mode=staticdoc["auto_mode"]; 
    activate_light = 0;         
    publish_status = true;
    if(auto_mode)
    {
      update_auto_mode_minutes();
      auto_mode_time_check();
    }
  }
  if (staticdoc.containsKey("obj_detection"))
  {
    enable_detection = staticdoc["obj_detection"];
    activate_light = 0;
    publish_status = true;
    auto_mode=false; 
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
  if (staticdoc.containsKey("light_state"))
  {
    set_light((enum states)staticdoc["light_state"]);
    enable_detection = false;
    auto_mode=false; 
    publish_status = true;
  }
  else if(staticdoc.containsKey("ota_url"))//if only ota url received,
  {
    if(staticdoc.containsKey("fw_version"))
    {
      int fw_version=staticdoc["fw_version"];
      if( fw_version==VERSION)//already on same version
        return;
    }
    const char* ota_url= staticdoc["ota_url"];
    fw_update(ota_url);
  } 
  if(staticdoc.containsKey("off_pause"))//if only ota url received,
  {
    motion_finish_off_pause = staticdoc["off_pause"];  
    publish_status = true;  
  }
   if(staticdoc.containsKey("on_pause"))//if only ota url received,
  {
    motion_finish_on_pause = staticdoc["on_pause"];  
    publish_status = true;  
  }
  if (staticdoc.containsKey("ping"))
  {
    publish_status = true;
  }
}

void fw_update(const char *url)
{
 WiFiClient client;
 ESPhttpUpdate.update(client, url);
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
      client.subscribe("header/command/global_cmd"); 
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
      String token = "";
      token += httpServer.arg(i);
      fileSystem.saveToFile("/token.txt", token);
      strcpy(mqtt_user_name, httpServer.arg(i).c_str());
    }
    if (httpServer.argName(i).equals("id"))
    {
      String topic_id = "";
      topic_id += httpServer.arg(i);
      fileSystem.saveToFile("/id.txt", topic_id);
      strcpy(mqtt_topic_id, httpServer.arg(i).c_str());
    }
    if (httpServer.argName(i).equals("device_group"))
    {
      device_group = httpServer.arg(i).toInt();
      fileSystem.saveToFile("/device_group.txt", device_group);
    }
    if (httpServer.argName(i).equals("alias"))
    {
      String alias = "";
      alias += httpServer.arg(i);
      fileSystem.saveToFile("/alias.txt", alias);
      strcpy(device_alias, httpServer.arg(i).c_str());
    }
    if (!client.connected())
    {
      if (httpServer.argName(i).equals("restart"))
      {
        esp_restart = true;
      }
    }
  }
}

void one_sec_call()
{
  if (esp_restart)
  {
    ESP.restart();
  }  
  if (publish_status)
  {
    publish_status = 0;
    send_device_status();    
  }
  reconnect();
}
void update_from_memory()
{
  const char *newCharBuffer;
  fileSystem.openFromFile("/device_group.txt", device_group);
  if (fileSystem.openFromFile("/token.txt", newCharBuffer))
    strcpy(mqtt_user_name, newCharBuffer);
  if (fileSystem.openFromFile("/id.txt", newCharBuffer))
    strcpy(mqtt_topic_id, newCharBuffer);
  if (fileSystem.openFromFile("/alias.txt", newCharBuffer))
    strcpy(device_alias, newCharBuffer);
    
  strcat(pub_topic, mqtt_topic_id);
  strcat(sub_topic, mqtt_topic_id);
  // strcat(will_topic,mqtt_topic_id);
  Serial.println(mqtt_user_name);
  Serial.println(mqtt_topic_id);
  Serial.println(device_group);
}
void auto_mode_time_check()
{
  if(!auto_mode)
    return;
  unsigned int current_time_minutes=timeClient.getHours()*60+timeClient.getMinutes();
  /*Serial.print("current time: min ");
  Serial.println(current_time_minutes);
  Serial.print("auto_start_min time: min ");
  Serial.println(auto_start_min);
  Serial.print("auto_end_min time: min ");
  Serial.println(auto_end_min);
  */
  if(current_time_minutes>auto_start_min || current_time_minutes<auto_end_min)//auto_start_min,auto_end_min;
  {    
    enable_detection=1;
  }
  else
  {
    enable_detection=0;
  }
  Serial.print("detection:");
  Serial.println(enable_detection);
}
void runPeriodicFunc()
{
    const short REFRESH_INTERVAL1 = 1;
    static unsigned int prevSec1 = 0;
    const short REFRESH_INTERVAL2 = 3600;
    static unsigned int prevSec2 = 0;
    const short REFRESH_INTERVAL3 = 60;
    static unsigned int prevSec3 = 0;
    
    unsigned int currentSec = (millis()/1000);
    
    if(currentSec - prevSec1 >= REFRESH_INTERVAL1)
    {   
      prevSec1=currentSec;  
      one_sec_call();    
    }
    if(currentSec - prevSec2 >= REFRESH_INTERVAL2)
    {   
      send_device_status();
      prevSec2=currentSec;       
    }
    if(currentSec - prevSec3 >= REFRESH_INTERVAL3)
    {   
      if(auto_st_time.hours == 0 && auto_en_time.hours ==0)
      {
        get_sunrise_sunset_time();         
      }
      auto_mode_time_check();
      prevSec3=currentSec;       
    }
    if (activate_light == 0 && (currentSec - finish_time >= motion_finish_on_pause))
    {
      pause_time = currentSec;
      set_light(off);
      activate_light = 3;
    }
    if (activate_light == 1)
    {
      set_light(on);
      activate_light = 3;
    }    
}
void setup()
{
  pinMode(LIGHT_SWITCH, OUTPUT); // Initialize the pin as an output
  digitalWrite(LIGHT_SWITCH, LIGHT_OFF);
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  if (!fileSystem.checkFlashConfig())
  {
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  timeClient.begin();
  Serial.println("local ip");
  Serial.println(WiFi.localIP());    
  get_sunrise_sunset_time();
  
  update_from_memory();
  client.setBufferSize(512);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  httpServer.on("/config", handleConfig);
  httpServer.begin();  
}
void send_device_status()
{
  generate_status_message();
  client.publish(pub_topic, tx_buf, 1);
}
void loop()
{
  timeClient.update();   
  runPeriodicFunc();
  httpServer.handleClient();
  client.loop();
}
