  #include <EEPROM.h>
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ArduinoOTA.h>
  #include <HTTPClient.h>
  #include<stdio.h>
  #include <string.h>

  #define pino_sensor 13
  #define pino_relay 4
  #define fator_calibracao 2.65

  // Configuration for fallback access point 
  // if Wi-Fi connection fails.
  String URL = "http://192.168.43.139:8080"; 
  const char * AP_ssid = "WaterBox";
  const char * AP_password = "qwerty12345";
  IPAddress AP_IP = IPAddress(10,1,1,1);
  IPAddress AP_subnet = IPAddress(255,255,255,0);
  const int batch = 0;

  struct WifiConf {

    char wifi_ssid[30];
    char ownerEmail[30];
    char myId[25];
    char wifi_password[30];
    
    char cstr_terminator = 0; // makse sure
  };
  WifiConf wifiConf;
  HTTPClient http;
  WiFiClient client;
  // Web server for editing configuration.
  // 80 is the default http port.
  WebServer server(80);
  volatile int count = 0;
  long tempo = 0;
  String id;


  void ICACHE_RAM_ATTR updateCount(){
    count++;
  }

  void setup() {

    writeWifiConf();
    Serial.begin(9600);
    Serial.println("Booting...");

    EEPROM.begin(512);
    readWifiConf();
    connectToWiFi();

    if (WiFi.status()!=WL_CONNECTED) {
      setUpAccessPoint();
      setUpWebServer();
    }
    
    setUpOverTheAirProgramming();
    
    pinMode(pino_relay,OUTPUT);
    pinMode(pino_sensor,INPUT);
    while(WiFi.status()!=WL_CONNECTED){
    server.handleClient();// Give processing time for the webserver.This must be called regularly for the webserver to work.  
    }
    checkInfo();

    Serial.print("Loop começando");
    attachInterrupt(pino_sensor,updateCount,FALLING);
    
  }

  void checkInfo(){
    id = wifiConf.myId;
    http.begin(client,URL + "/influxIOT/waterbox/checkInfo");
    while(!http.POST("{\"id\":\""+id+"\"}")){}
    if (http.getString()=="false"){ 
      Serial.println("id nao gravado");
      Serial.println(http.getString());
      http.end();
      requestInfo();
      }
    else {
      Serial.println("id gravado");
      Serial.println(wifiConf.myId);}

  }

  void requestInfo(){
    String email = wifiConf.ownerEmail;
    String payload = "{\"email\": \""+email+"\", \"name\": \"WaterBox_1.0\", \"batch\": "+batch+"}";  
    Serial.println(payload);
    http.begin(URL+"/influxIOT/waterbox/insertWaterBox");
    if(http.POST(payload)){
      String response = http.getString();
      Serial.println(response);
      response.toCharArray(wifiConf.myId,sizeof(wifiConf.myId));
      Serial.println(wifiConf.myId);
      writeWifiConf();
      id = response;
    
    }
    http.end();
  }


  void readWifiConf() {
    // Read wifi conf from flash
    for (int i=0; i<sizeof(wifiConf); i++) {
      ((char *)(&wifiConf))[i] = char(EEPROM.read(i));
      Serial.print(((char *)(&wifiConf))[i]);
    }
  //   // Make sure that there is a 0 
  //   // that terminatnes the c string
  //   // if memory is not initalized yet.
    wifiConf.cstr_terminator = 0;
  }


  void writeWifiConf() {
    for (int i=0; i<sizeof(wifiConf); i++) {
      EEPROM.write(i, ((char *)(&wifiConf))[i]);
    }
    EEPROM.commit();
  }




  bool connectToWiFi() {
    Serial.printf("Connecting to '%s'\n", wifiConf.wifi_ssid);
    long time = millis();
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiConf.wifi_ssid, wifiConf.wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if((millis()-time)>15000) return false;
    } 
  }

  void setUpAccessPoint() {
      Serial.println("Setting up access point.");
      Serial.printf("SSID: %s\n", AP_ssid);
      Serial.printf("Password: %s\n", AP_password);

      WiFi.mode(WIFI_AP_STA);
      WiFi.softAPConfig(AP_IP, AP_IP, AP_subnet);
      if (WiFi.softAP(AP_ssid, AP_password)) {
        Serial.print("Ready. Access point IP: ");
        Serial.println(WiFi.softAPIP());
      } else {
        Serial.println("Setting up access point failed!");
      }
  }

  void setUpWebServer() {
    server.on("/", handleWebServerRequest);
    server.begin();
  }

  void handleWebServerRequest() {
    bool save = false;

    if (server.hasArg("ssid") && server.hasArg("password")&&server.hasArg("email")) {
      server.arg("ssid").toCharArray(
        wifiConf.wifi_ssid,
        sizeof(wifiConf.wifi_ssid));
      server.arg("password").toCharArray(
        wifiConf.wifi_password,
        sizeof(wifiConf.wifi_password));
      server.arg("email").toCharArray(
        wifiConf.ownerEmail,
        sizeof(wifiConf.ownerEmail));

      Serial.println(server.arg("email"));
      Serial.println(wifiConf.wifi_ssid);

      writeWifiConf();
      save = true;
    }

    String message = "";
    message += "<!DOCTYPE html>";
    message += "<html>";
    message += "<head>";
    message += "<title>Water Box conf</title>";
    message += "</head>";
    message += "<body>";
    if (save) {
      message += "<div>Saved! Rebooting...</div>";
    } else {
      message += "<h1>Wi-Fi conf</h1>";
      message += "<form action='/' method='POST'>";
      message += "<div>Rede:</div>";
      message += "<div><input type='text' name='ssid' value='" + String(wifiConf.wifi_ssid) + "'/></div>";
      message += "<div>Password:</div>";
      message += "<div><input type='password' name='password' value='" + String(wifiConf.wifi_password) + "'/></div>";
      message += "<div>Email:</div>";
      message += "<div><input type='text' name='email' value='" + String(wifiConf.ownerEmail) + "'/></div>";
      message += "<div><input type='submit' value='Save'/></div>";
      message += "</form>";
    }
    message += "</body>";
    message += "</html>";
    server.send(200, "text/html", message);

    if (save) {
      Serial.println("Wi-Fi conf saved. Rebooting...");
      delay(1000);
      ESP.restart();
    }
  }

  void setUpOverTheAirProgramming() {
    
    // Change the name of how it is going to 
    // show up in Arduino IDE.
    ArduinoOTA.setHostname("waterBox");

    // Re-programming passowrd. 
    // No password by default.
    ArduinoOTA.setPassword("qwerty12345");

    ArduinoOTA.begin();
  }

  double getVolume(){
    return count*fator_calibracao;
  }

  void checkStatus(){
    http.begin(URL+"/influxIOT/waterbox/status");
    if(http.POST("{\"id\":\""+id+"\"}")){
      String result = http.getString();
      Serial.print(result);
      if (result=="0") digitalWrite(pino_relay,HIGH);
      else if (result=="1") digitalWrite(pino_relay,LOW);
      else if (result=="9") 
      while(true){
      ArduinoOTA.handle();// Give processing time for ArduinoOTA. This must be called regularly for the Over-The-Air upload to work. 
      }
      http.end();
    }

  }

  void sendMeasurement(){
        String payload = "{\"volume\": "+String(getVolume())+", \"flow\": "+String(getVolume())+", \"id\": \""+id+"\"}";
        http.begin(URL + "/influxIOT/waterbox/measurements");
        if(http.POST(payload)){
          Serial.println(http.getString());
          count = 0;
        }
        else{
          Serial.println("Num deu");
        }
        http.end();
  }

  void loop() {
    if (millis()-tempo>1000){
      
      detachInterrupt(pino_sensor);
      checkStatus(); 

      if (WiFi.status()!=WL_CONNECTED) {   
        setUpAccessPoint();
        while(WiFi.status()!=WL_CONNECTED) {
          server.handleClient();
          if(millis()%100000>90000) connectToWiFi();
        }
      }
      else{
        if (count>0){
          sendMeasurement();
        }
      }

      attachInterrupt(pino_sensor,updateCount,FALLING);
      tempo = millis();
    }
  }   
