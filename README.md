# cam-flood-light
Chinese v380 cam in use, can be any, ispy sends mqtt message when there is motion detection, esp8266 triggers flood light,  

### setup guide
* Download and install agent DVR https://www.ispyconnect.com/download.aspx
* Download and install https://www.deepstack.cc/
* follow guide to setup deepstack www.ispyconnect.com/userguide-agent-deepstack-ai.aspx
* Start agent, optionally add service to auto start after system power up/reboots
* From settings, Configure camera, mqtt server, ftp if any
* From Detector, select 'Simple' detector
* Once camera is added, from cam edit key, go to alerts, add new alert, select Mode as 'Detected'
* Optionally configure intelligence in Alerts, to alert only on person detection for example
* Select actions, go to actions and configure for both Alert and Alert stopped (if alert, then mqtt)
* Verify from some mqtt client if agent able to push mqtt messages on motion detection
* Setup hardware to receive mqtt messages and turn on/off light

#### To start deepstack on port 8000
docker run -e VISION-DETECTION=True -v localstorage:/datastore -p 8000:5000 deepquestai/deepstack

#### ispy messages
topic: header/command/0002  
{"ident":"ispy-agent-0002","motion": "detected"}  
{"ident":"ispy-agent-0002","motion": "finished"}  


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
