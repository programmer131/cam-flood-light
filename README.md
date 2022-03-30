# cam-flood-light
Chinese v380 cam in use, can be any, ispy sends mqtt message when there is motion detection, esp8266 triggers flood light,  

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
