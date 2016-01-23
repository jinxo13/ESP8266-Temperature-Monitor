/* 
 
   Based on:
      DHTServer - ESP8266 Webserver with a DHT sensor as an input
      Version 1.0  5/3/2014  Version 1.0   Mike Barela for Adafruit Industries

   Version 1.0 Hamish McNeish
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>
#include "ntp.h"
#include "logging.h"
#include "temp.h"

const char* SSID     = "<WIFI SID>";
const char* PASSWORD = "<WIFI PASSWORD>";
const char* SERVER_NAME = "DHT Weather Reading Server";
const char* TIME_SOURCE = "google.com.au";
const char* THINGS_SPEAK_SERVER = "184.106.153.149";
const char* THINGS_SPEAK_WRITE_API = "<THINGS SPEAK WRITE API>";

String IP_ADDRESS = "";

bool doLoop = true;
char buffer[256];

ESP8266WebServer server(80);
 
//Status
unsigned long startTime = 0;
unsigned long lastUpdated = 0;

typedef struct
{
   const char* key;
   String value;
} kv_t;
typedef struct
{
   const char* key;
   String value;
   bool isString;
} json_t;

const int STATE_SIZE=3;
kv_t STATES[] = {
  {"TimeSet","Time not set yet."},
  {"TempRead","No read yet"},
  {"Wifi","Not connected yet"}
};
String getState(const char* key){
  for (int i=0;i<STATE_SIZE;i++){
    if (strcmp(STATES[i].key,key)==0) {
      return STATES[i].value;
    }
  }
  return "";
}

void setState(const char* key, String value){
  for (int i=0;i<STATE_SIZE;i++){
    if (strcmp(STATES[i].key,key)==0) {
      STATES[i].value = value;
      break;
    }
  }
}

/**************** Utility/Common functions ***************************************/
void connectToWifi(void)
{
  // Connect to WiFi network
  WiFi.begin(SSID, PASSWORD);
  logInfo("\n\r \n\rWorking to connect");
  //Try forever to connect....
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  setState("Wifi","OK - Connected to: "+String(SSID));
  Serial.println("");
  logInfo(SERVER_NAME);
  logInfo("Connected to "+String(SSID));
  IPAddress ip = WiFi.localIP();
  sprintf(buffer,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
  IP_ADDRESS = String(buffer);
  logInfo("IP Address: "+IP_ADDRESS);
}

void setupTime()
{
  logInfo("Waiting for sync to time server...");
  WiFiClient client;
  int t[6] = {};
  webUnixTime(TIME_SOURCE,client,t);
  if (t[0]) {
    logInfo("Time received from: "+String(TIME_SOURCE));
    //setTime(hr,min,sec,day,month,yr)
    setTime(t[3],t[4],t[5],t[2],t[1],t[0]);
    milliOffset = millis();
    adjustTime(11*60*60);
    logInfo("Time set");
    setState("TimeSet","OK - Successfully set from "+String(TIME_SOURCE));
  }
  else {
    const char * msg = "WARN - Failed to set time. Defaulting to unix time 0. No functional impact.";
    logWarning(msg);
    setState("TimeSet",msg);
    setTime(0);
  }
}

String formatTimeSpan(const unsigned long& t)
{
  long seconds = t % 60;
  long minutes = (t/60) % 60;
  long hours = (t/60/60) % 24;
  long days = t/60/60/24;
  String timeUpStr = "";
  if (days > 0) timeUpStr+=String(days)+" day(s) ";
  if (hours > 0) timeUpStr+=String(hours)+" hour(s) ";
  if (minutes > 0) timeUpStr+=String(minutes)+" minute(s) ";
  if (seconds > 0) timeUpStr+=String(seconds)+" second(s)";
  return timeUpStr;
}

//Simple timed job support (up to 5)
typedef std::function<void(void)> THandlerFunction;
THandlerFunction funcs[5];
unsigned long lastRun[5];
unsigned long periods[5];
int funcCount = 0;
//TODO struct for job and add name
void addJob(unsigned long period, unsigned long offset, THandlerFunction f){
  if (funcCount >= 5) {
    logError("Only up to 5 timed jobs are supported.");
  }
  funcs[funcCount]=f;
  periods[funcCount++]=period;
}
void processTimedJobs(){
  unsigned long currentMillis = millis();
  for(int i=0;i<funcCount;i++) {
    if (lastRun[i] == 0) lastRun[i] = currentMillis;
    if (currentMillis - lastRun[i] >= periods[i]) {
      lastRun[i] = currentMillis;
      funcs[i]();
    }
  }
}

String buildJson(json_t json[], int arraySize) {
  //build result
  String result="{\n";
  for(int i=0;i<arraySize;i++){
    String sep="";
    if (i!=0) result+=",\n";
    result+="  \""+String(json[i].key)+"\": ";
    if (json[i].isString) sep = "\"";
    result+=sep+json[i].value+sep;
  }
  result+="\n}\n";
  return result;
}

String getStatus() {
  //TODO "starting"...
  //Time up, days, hours...
  unsigned long currentTime = now();
  unsigned long timeUp = currentTime - startTime;
  json_t json[7+STATE_SIZE];
  json_t t[7] = {
    {"CurrentTime", String(currentTime), false},
    {"TimeUp", formatTimeSpan(timeUp), true},
    {"LoopState", doLoop?"Running":"Stopped", true},
    {"LastUpdated", getDateTime(lastUpdated), true},
    {"LastRead", getDateTime(lastRead), true},
    {"LastTemperature", String(temp_c,DHT_DECIMALS), false},
    {"LastHumidity", String(humidity,DHT_DECIMALS), false}
  };
  for(int i=0;i<7;i++) {
    json[i]=t[i];
  }
  for(int i=0;i<STATE_SIZE;i++){
    json[7+i] = {STATES[i].key, STATES[i].value, true};
  }
  return buildJson(json,7+STATE_SIZE);
}

/**
 * Standard server responses
 */
void setupServerUrls(void)
{
  server.on("/", [](){
    server.send(200, "text/plain", String(SERVER_NAME) + " running. Use /info, /status for more information. Other commands /reset, /start, /stop");
    delay(100);
  }
  );

  server.on("/info", [](){
    logDebug("Requested /info");
    json_t json[] = {
      {"DateTime", getDateTime(), true},
      {"ServerName", SERVER_NAME, true},
      {"IPAddress", IP_ADDRESS, true},
      {"SketchSize", String(ESP.getSketchSize()), false},
      {"FreeSize", String(ESP.getFreeSketchSpace()), false},
      {"Heap", String(ESP.getFreeHeap()), false},
      {"BootVersion", String(ESP.getBootVersion()), false},
      {"CPU", String(ESP.getCpuFreqMHz())+"MHz", true},
      {"SDK", ESP.getSdkVersion(), true},
      {"ChipID", String(ESP.getChipId()), false},
      {"FlashID", String(ESP.getFlashChipId()), false},
      {"FlashSize", String(ESP.getFlashChipRealSize()), false},
      {"Vcc", String(ESP.getVcc()), false}
    };
    server.send(200, "text/plain", buildJson(json,13));
  });

  server.on("/reset", [](){
    logDebug("Requested /reset");
    json_t json[] = {
         {"Command","reset",true},
         {"DateTime",getDateTime(), true},
         {"Result","OK", true}
      };
    server.send(200, "text/plain", buildJson(json,3));
    ESP.restart();
  });

  server.on("/stop", [](){
    logDebug("Requested /stop");
    doLoop = false;
    json_t json[] = {
         {"Command","stop",true},
         {"DateTime",getDateTime(), true},
         {"Result","OK", true}
      };
    server.send(200, "text/plain", buildJson(json,3));
  });
  
  server.on("/start", [](){
    logDebug("Requested /start");
    doLoop = true;
    json_t json[] = {
         {"Command","start",true},
         {"DateTime",getDateTime(), true},
         {"Result","OK", true}
      };
    server.send(200, "text/plain", buildJson(json,3));
  });

  server.on("/status", [](){
    logDebug("Requested /status");
    server.send(200, "text/plain", getStatus());
  });

}
/**************** Utility/Common functions ENDS ***************************************/



/**************** Custom functions ***************************************/

void updateTemp(){
  String temp = String(temp_c, DHT_DECIMALS);
  String hum = String(humidity, DHT_DECIMALS);
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(THINGS_SPEAK_SERVER, 80)) {
    logError("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String host = String(THINGS_SPEAK_SERVER);
  String url = "/update?api_key="+String(THINGS_SPEAK_WRITE_API)+"&field1="+temp+"&field2="+hum;
  logInfo("Requesting URL: " + url);
  
  // This will send the request to the server
  client.print("GET " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(2000);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    logDebug(line);
    if (line.startsWith("HTTP/1.1 200 OK")) {
      logInfo("Updated succeeded");
      lastUpdated=now();
    }
    else {
      logError("Updated failed");
    }
    break;
  }
  logDebug("Closing connection");
}

void setupServerCustomUrls(void)
{
  server.on("/temp", []()
  {
    logDebug("Requested /temp");
    json_t json[] = {
         {"Command","temp",true},
         {"DateTime",getDateTime(), true},
         {"TempRead",getState("TempRead"), true},
         {"TemperatureC",String(temp_c,DHT_DECIMALS), false},
         {"Humidity",String(humidity,DHT_DECIMALS), false},
         {"Result","OK", true}
      };
    server.send(200, "text/plain", buildJson(json,5));
  });
}

/**************** Custom Setup/Loop functions ***************************************/

//Add pre-setup steps here
void doPreSetup(void)
{
  dht.begin();
}

//Add any post setup steps here
//Wifi is available at this stage
void doPostSetup(void)
{
  //Run every 15 minutes
  addJob(15*60*1000,0,[](){
    if (!doLoop) {
      return;
    }
    if (gettemperature()) {
      if ((int)temp_c < 60 && (int)temp_c>-60) {
        setState("TempRead","OK");
        updateTemp();
      }
      else {
        setState("TempRead","WARN - Read ok but values outside expected range");
      }
    }
    else {
      setState("TempRead","ERROR - Unable to read values");
    }
  });
}

//Add loop logic here
void doCustomLoop()
{
}

/**************** Standard Setup/Loop functions ***************************************/

void setup(void)
{
  Serial.begin(115200);
  //Do any custom pre-setup steps
  doPreSetup();
  connectToWifi();
  
  //Settime, starttime will be 0 if failed to set time
  setupTime();
  startTime = now();

  //Setup web server
  setupServerUrls();
  setupServerCustomUrls();
  server.begin();
  logInfo("HTTP server started");

  //Heartbeat every 60 seconds
  addJob(60*1000,0,[](){
    logInfo("--- Server is alive --- IP: "+IP_ADDRESS);
  }
  );


  //Do any custom post setup steps
  doPostSetup();
}

long lastSecs = 0;
void loop(void)
{
  //Check wifi is ok
  if (WiFi.status() != WL_CONNECTED) {
    //Connection lost
    logWarning("Wifi connection has been lost. Reconnecting....");
    setState("Wifi","Error - Connection to "+String(SSID)+" lost.");
    connectToWifi();
  }

  //Process any timed jobs
  processTimedJobs();
  
  //Process any web server requests
  server.handleClient();

  //TODO Prevent too many timesyncs
  if (timeStatus() == timeNeedsSync) {
    logInfo("Time sync required...");
    setupTime();
  }

  //Allow ESP to do other stuff
  delay(10);
} 

