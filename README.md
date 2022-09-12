# cam-flood-light
Chinese v380 cam in use, can be any, ispy sends mqtt message when there is motion detection, esp8266 triggers flood light,  

### setup guide
* Download and install agent DVR https://www.ispyconnect.com/download.aspx
* Download and install Deepstack AI https://www.deepstack.cc/
* follow guide to setup deepstack www.ispyconnect.com/userguide-agent-deepstack-ai.aspx
* Start agent, optionally add service to auto start after system power up/reboots
* From settings, Configure camera, mqtt server, ftp if any
* From Detector, select 'Simple' detector
* Once camera is added, from cam edit key, go to alerts, add new alert, select Mode as 'Detected'
* Optionally configure intelligence in Alerts, to alert only on person detection for example
* Select actions, go to actions and configure for both Alert and Alert stopped (if alert, then mqtt)
* Verify from some mqtt client if agent able to push mqtt messages on motion detection
* Setup hardware to receive mqtt messages and turn on/off light

**To start deepstack on port 8000** 

```docker run --restart=always -e VISION-DETECTION=True -v localstorage:/datastore -p 8000:5000 deepquestai/deepstack``` 

**To start senseAi on port 5000** 

```docker run --restart=always -p 5000:5000 --name CodeProject.AI-Server -d -v /usr/share/CodeProject/AI:/usr/share/CodeProject/AI codeproject/ai-server```

**Deepstack dark model**
Follow the guide https://forum.deepstack.cc/t/deepstack-exdark-detect-objects-in-dark-night-images-and-videos/934 

To copy model from local filesystem to docker (one time process), then stop and start docker with same above command 

```sudo docker cp /home/models 92e57955f907:/datastore/```

#### ispy messages
**topic** header/command/0002  
**payload** {"ident":"ispy-agent-0002","motion": "detected"}  
**payload** {"ident":"ispy-agent-0002","motion": "finished"} 

**mqtt status topic** 
header/statue/device_id 

**mqtt command topic** 
header/command/device_id 
#### MQTT Test commands

{ 
"ident" : "flespi-board", 
"ota_url":"http://myiotota.000webhostapp.com/v9.bin" 
} 

{ 
"ident" : "flespi-board", 
"ota_url":"http://192.168.18.24:2222/flood-light.ino.d1_mini.bin" 
} 

{
  "auto_mode": true
}

{
  "auto_mode": false
}

{
"motion": "detected"
}

{
"motion": "finished"
}

{
  "light_state": 0
}

{
  "light_state": 1
}

{ 
"auto_mode_time":[18,10,6,10] 
} 

{ 
"ping":1 
} 
#### status message from device 
```
{
   "ident":"58:BF:25:DC:5B:53",
   "ssid":"TP-LINK_C1CA",
   "wifi_quality":54,
   "ip":"192.168.0.103",
   "obj_detection":false,
   "light_state":0,
   "dev_time":"21:59:12",
   "online":1,
   "alias":"esp01-cam-light",
   "auto_mode":false,
   "fw_version":12,
   "device_group":99,
   "on_pause":20,
   "off_pause":5,
   "auto_mode_time":[
      18,
      40,
      5,
      20
   ]
} 
``` 
#### mqtt dashboard screenshot
![image](https://user-images.githubusercontent.com/12946496/161364333-2fbdeb47-92de-4f33-afff-e35535ab31a7.png)
