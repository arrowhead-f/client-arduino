/**********
 *  Copyright (c) 2018 AITIA International Inc.
 *
 *  This work is part of the Productive 4.0 innovation project, which receives grants from the
 *  European Commissions H2020 research and innovation programme, ECSEL Joint Undertaking
 *  (project no. 737459), the free state of Saxony, the German Federal Ministry of Education and
 *  national funding authorities from involved countries.
 * 
 *  Arrowhead framework Service Consumer insecure client sketch for NodeMCU-32S development boards
 *  
 *  Important: you need to increase the maximum sketch size for the ESP-32S!
 *  See: https://github.com/espressif/arduino-esp32/issues/339
 *  To get the sketch working, you need to replace the boards.txt and default.csv files for ESP-32S.
 *  The boards.txt goes to ...\Documents\Arduino\hardware\espressif\esp32,
 *  while the default.csv goes to ...\Documents\Arduino\hardware\espressif\esp32\tools\partitions folders!
 *  
 *  This sketch uses a JSON serializer, see the imports. 
 *  This sketch needs an NTP (Network Time Protocol) server either locally or over the Internet to fetch current time. 
 * *******/

#include <WiFi.h>
#include <HTTPClient.h>

//json parser library: https://arduinojson.org
//NOTE: using version 5.1.13, since 6.x is in beta (but the Arduino IDE updates it!)
//Make sure you are using 5.1.x within the Package Manager!
#include <ArduinoJson.h>

const char* ssid = "Arrowhead-RasPi-IoT";
const char* password = "arrowhead";

const char* orchestrator_addr = "http://arrowhead.tmit.bme.hu:8440";
const char* systemName = "client1";
const char* serviceDefinition = "IndoorTemperature";
const char* interface1 = "json"; //if you need more add at line 

//we will store the service endpoint we receive here
String endpoint;
bool orchSuccess=false;


void setup() {
  Serial.begin(115200);
  delay(100);   //Some delay needed before calling the WiFi.begin
  
  //connectting to network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
  }
  Serial.print("Connected, my IP is: ");
  Serial.println(WiFi.localIP());


  //building the orchestration request
  StaticJsonBuffer<500> orch_req;
  JsonObject& root = orch_req.createObject();
  JsonObject& requesterSystem = root.createNestedObject("requesterSystem");
  requesterSystem["systemName"] = systemName;
  requesterSystem["address"] = WiFi.localIP().toString();
  requesterSystem["port"] = "8080";
  JsonObject& requestedService = root.createNestedObject("requestedService");
  requestedService["serviceDefinition"] = serviceDefinition;
  JsonArray& interfaces = requestedService.createNestedArray("interfaces");
  interfaces.add(interface1);
  JsonObject& metadata = requestedService.createNestedObject("serviceMetadata");
  JsonObject& orchestrationFlags = root.createNestedObject("orchestrationFlags");

  //TODO: add additional metadata you need!
  metadata["unit"] = "celsius";

  //TODO: set custom orchestration flags yourself, if needed!
  orchestrationFlags["overrideStore"] = "true";
  orchestrationFlags["metadataSearch"] = "true";
  orchestrationFlags["enableInterCloud"] = "true";
  orchestrationFlags["pingProviders"] = "false";

  String orchRequest;
  root.prettyPrintTo(orchRequest);

  //requesting orchestration
  Serial.println("Sending orchestration request with the following payload:");
  Serial.println(orchRequest);
  HTTPClient http_orch;
  http_orch.begin(orchestrator_addr + String("/orchestrator/orchestration"));
  http_orch.addHeader("Content-Type", "application/json"); //Specify content-type header
  int httpResponseCode_orch = http_orch.POST(String(orchRequest)); //Send the actual POST request
  String orch_response = http_orch.getString();//Get the response to the request
  Serial.println("Orchestration completed, with status code: ");
  Serial.println(orch_response);           //Print request answer
  http_orch.end();

  //parsing orchestration response
  StaticJsonBuffer<2000> JSONBuffer_orchResp; //Memory pool
  JsonObject& root_orchresp = JSONBuffer_orchResp.parseObject(orch_response);
  if (httpResponseCode_orch<0) {
    Serial.println("Orchestrator not available!");
    ESP.restart();
    return;
  }
  if (httpResponseCode_orch==HTTP_CODE_OK) {  
    const char* address = root_orchresp["response"][0]["provider"]["address"];
    const char* port = root_orchresp["response"][0]["provider"]["port"];
    const char* uri = root_orchresp["response"][0]["serviceURI"];
    endpoint = "http://" + String(address) + ":" + String(port) + "/" + String(uri);
    Serial.println("Received endpoint, connecting to: " + endpoint); 
    orchSuccess = true;  
  } else {
    Serial.println("Orchestration failed!");
  }

}

void loop() {
  if (orchSuccess) {
    HTTPClient http_srv;
    http_srv.begin(endpoint);
    http_srv.addHeader("Content-Type", "application/json"); //Specify content-type header
    int httpResponseCode_srv = http_srv.GET(); //Send the actual GET request
    String srv_response = http_srv.getString();//Get the response to the request
    Serial.println("Service Response:");
    Serial.println(srv_response);           //Print request answer
    http_srv.end();

    //parsing service response (SenML)
    StaticJsonBuffer<500> JSONBuffer_srv; //Memory pool
    JsonObject& srv_root = JSONBuffer_srv.parseObject(srv_response);
    const char* temp_raw = srv_root["e"][0]["v"];
    String temp_str = String(temp_raw);
    Serial.println("The temperature is " + temp_str + "°C.");
    delay(5000);
    
  } else {
    Serial.println("No endpoint to connect to! Restarting!");
    ESP.restart();
  }
}
