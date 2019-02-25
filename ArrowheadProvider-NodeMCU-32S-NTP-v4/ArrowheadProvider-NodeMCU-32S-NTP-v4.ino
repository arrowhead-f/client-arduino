/**********
 *  Copyright (c) 2018 AITIA International Inc.
 *
 *  This work is part of the Productive 4.0 innovation project, which receives grants from the
 *  European Commissions H2020 research and innovation programme, ECSEL Joint Undertaking
 *  (project no. 737459), the free state of Saxony, the German Federal Ministry of Education and
 *  national funding authorities from involved countries.
 * 
 *  Arrowhead framework Service Provider insecure client sketch for NodeMCU-32S development boards
 *  
 *  Important: you need to increase the maximum sketch size for the ESP-32S!
 *  See: https://github.com/espressif/arduino-esp32/issues/339
 *  To get the sketch working, you need to replace the boards.txt and default.csv files for ESP-32S.
 *  The boards.txt goes to ...\Documents\Arduino\hardware\espressif\esp32,
 *  while the default.csv goes to ...\Documents\Arduino\hardware\espressif\esp32\tools\partitions folders!
 *  
 *  This sketch uses a JSON serializert and an asyncronous HTTP(S) server implementations, see the imports. 
 *  This sketch needs an NTP (Network Time Protocol) server either locally or over the Internet to fetch current time. 
 * *******/

//generic ESP32 libs
#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <FS.h>

//json parser library: https://arduinojson.org
//NOTE: using version 5.13.1, since 6.x is in beta (but the Arduino IDE updates it!)
//Make sure you are using 5.13.x within the Package Manager!
#include <ArduinoJson.h>

//HTTP server library: https://github.com/me-no-dev/ESPAsyncWebServer
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//GLOBAL SETTINGS - NTP
const char* ntpServer = "0.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//TODO: change params, if needed!
#define SERVER_PORT 8454 
#define serviceVersion 1
const char* serviceURI = "temperature";
const char* ssid = "Arrowhead-RasPi-IoT";
const char* password = "arrowhead";

//TODO: modify these accordingly, and add additional service metadata at line 94 if needed!
const char* serviceRegistry_addr = "http://arrowhead.tmit.bme.hu:8442";
const char* serviceDefinition = "IndoorTemperature";
const char* serviceInterface = "JSON_Insecure_SenML";
const char* systemName = "InsecureTemperatureSensor";

//async webserver instance
AsyncWebServer server(SERVER_PORT);

void setup() {
  //debug log on serial
  Serial.begin(115200);
  delay(1000);   //Delay needed before calling the WiFi.begin

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { //Check for the connection
        delay(1000);
        Serial.print("Connecting to WiFi SSID: ");
        Serial.println(ssid);
  }
  Serial.print("Connected, my IP is: ");
  Serial.println(WiFi.localIP());

  //get clock from NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain network time, restarting!");
    ESP.restart();
    return;
  } else {
    Serial.print("Current time is:");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }

  //building ServiceRegistryEntry on my own
  StaticJsonBuffer<500> SREntry;
  JsonObject& root = SREntry.createObject();
  JsonObject& providedService = root.createNestedObject("providedService");
  providedService["serviceDefinition"] = serviceDefinition;
  JsonObject& metadata = providedService.createNestedObject("metadata");
  JsonArray& interfaces = providedService.createNestedArray("interfaces");
  interfaces.add(serviceInterface);

  //TODO: add service metadata if you need!
  metadata["unit"]="celsius";

  root.createNestedObject("provider");
  root["provider"]["systemName"] = systemName;
  root["provider"]["address"] = WiFi.localIP().toString();
  root["port"] =SERVER_PORT;
  root["serviceURI"] = serviceURI;
  root["version"] =serviceVersion;

  String SRentry;
  root.prettyPrintTo(SRentry);

  //registering myself in the ServiceRegistry
  HTTPClient http_sr;
  http_sr.begin(String(serviceRegistry_addr) + "/serviceregistry/register"); //Specify destination for HTTP request
  http_sr.addHeader("Content-Type", "application/json"); //Specify content-type header
  int httpResponseCode_sr = http_sr.POST(String(SRentry)); //Send the actual POST request
  
  if (httpResponseCode_sr<0) {
    Serial.println("ServiceRegistry is not available!");
    http_sr.end();  //Free resources
  } else {
    Serial.print("Registered to SR with status code:");
    Serial.println(httpResponseCode_sr);
    String response_sr = http_sr.getString();                       //Get the response to the request
    http_sr.end();  //Free resources
    if (httpResponseCode_sr!=HTTP_CODE_CREATED) { //SR responded properly, check if registration was successful
        Serial.println("Service registration failed with response:");
        Serial.println(response_sr);           //Print request answer
  
        //need to remove our previous entry and then re-register
        HTTPClient http_remove;
        http_remove.begin(String(serviceRegistry_addr) + "/serviceregistry/remove"); //Specify destination for HTTP request
        http_remove.addHeader("Content-Type", "application/json"); //Specify content-type header
        int httpResponseCode_remove = http_remove.PUT(String(SRentry)); //Send the actual PUT request
        Serial.print("Removed previous entry with status code:");
        Serial.println(httpResponseCode_remove);
        String response_remove = http_remove.getString();                       //Get the response to the request
        Serial.println(response_remove);           //Print request answer
        http_remove.end();  //Free resources
  
        HTTPClient http_renew;
        http_renew.begin(String(serviceRegistry_addr) + "/serviceregistry/register"); //Specify destination for HTTP request
        http_renew.addHeader("Content-Type", "application/json"); //Specify content-type header
        int httpResponseCode_renew = http_renew.POST(String(SRentry)); //Send the actual POST request
        Serial.print("Re-registered with status code:");
        Serial.println(httpResponseCode_renew);
        String response_renew = http_renew.getString();                       //Get the response to the request
        Serial.println(response_renew);           //Print request answer
        http_renew.end();  //Free resources
      }    
    } // if-else: SR is online and its responses were valid

    //waiting here a little for no reason, can be deleted!
    delay(1000);

  Serial.println("Starting server!");

  //adding a '/' in front of the serviceURI!
  char path[30];
  String pathString = String("/") + String(serviceURI);
  pathString.toCharArray(path, 20);

  //starting the webserver
  server.on(path, HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Received HTTP request, responding with:");

    //TODO: do your sensor measurement
    double temperature = 21.0;
    
    //build the SenML format
    time_t now;
    time(&now);
    StaticJsonBuffer<500> doc;
    JsonObject& root = doc.createObject();
    root["bn"]=systemName;
    root["bt"]=now;
    root["bu"]="celsius";
    root["ver"]=1;
    JsonArray& e = root.createNestedArray("e");
    JsonObject& meas = e.createNestedObject();
    meas["n"] = String(serviceDefinition) + "_" + String (serviceInterface);
    meas["v"] = temperature;
    meas["t"] = 0;

    String response;
    root.prettyPrintTo(response);
    request->send(200, "application/json", response);
    Serial.println(response);
  });
  server.begin();

  //prepare LED pin
  pinMode(LED_BUILTIN, OUTPUT);
}

//blink 1Hz if everything is set up, provider is running
int ledStatus = 0;
void loop(){
  if (ledStatus) {
    digitalWrite(LED_BUILTIN, HIGH);
    ledStatus =0;
  }
  else {
     digitalWrite(LED_BUILTIN, LOW);
     ledStatus = 1;
  }
  delay(1000);
}
