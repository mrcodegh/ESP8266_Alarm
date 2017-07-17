#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// Enable debugging monitoring
//#define DEBUG

#ifdef DEBUG
  #define DBG(message)    Serial.print(message)
  #define DBGL(message)    Serial.println(message)
  #define DBGW(message)    Serial.write(message)
  #define WRITE_LED
  #define TOGGLE_LED
  #define SET_LED(x)
#else
  #define DBG(message)
  #define DBGL(message)
  #define DBGW(message)
  #define LED_OUTPUT_LEVEL (led_on == true ? LOW : HIGH)
  #define WRITE_LED digitalWrite(LED_BUILTIN, LED_OUTPUT_LEVEL)
  #define TOGGLE_LED (led_on = !led_on)
  #define SET_LED(x) (led_on = x)
  bool led_on = true;
#endif        


//define your default values here, if there are different values in config.json, they are overwritten.
const int host_max_len = 80;
char host[host_max_len];
const int apiKey_max_len = 80;
char apiKey[apiKey_max_len];
bool shouldSaveConfig = false;

#define DOOR_OPEN_STATE LOW
const int door_pin = 2;
bool door_open = false;
unsigned long door_open_cnt = 0;

unsigned long previousMillis = 0;
const unsigned check_wifi_interval_hours = 4;
const unsigned max_days_for_AP_config = 2;  // after this time device idles and waits for pwr reset
const unsigned long sleep_time_in_ms = 2000;  // poll rate of door signal


//callback notifying us of the need to save config
void saveConfigCallback () {
  DBGL("Should save config");
  shouldSaveConfig = true;
}

void sleepCallback() {
#ifdef DEBUG
  DBGL();
  Serial.flush();
#endif
}

void connect() {
  unsigned wait_cnt;
  const unsigned connect_seconds_max = 30;
    
  if(WiFi.status() != WL_CONNECTED) {
    DBGL("connecting WiFi...");
    //wifi_fpm_do_wakeup();
    WiFi.forceSleepWake();
    delay(100);
    wifi_fpm_close;
    wifi_set_sleep_type(MODEM_SLEEP_T);
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();

    wait_cnt = 0;
    while(wait_cnt++ < connect_seconds_max && WiFi.status() != WL_CONNECTED) {
      delay(1000);
      DBG(wait_cnt);
    }
    if(WiFi.status() != WL_CONNECTED) {
      DBGL("unable to connect - reset");
      ESP.reset();
      delay(3000);
    }
  }
}

void disconnect() {
  DBGL("disconnecting WiFi...");
  wifi_set_opmode(NULL_MODE); 
  wifi_fpm_open(); 
  wifi_fpm_set_wakeup_cb(sleepCallback);
  WiFi.forceSleepBegin();
  delay(100);
}


void setup() {

WiFiManager wifiManager;
int day_cnt = 0;

#ifdef DEBUG
  Serial.begin(115200);
  delay(100);
  DBGL();
  // Delay 3 seconds to get USB/COM terminal window active to read debug
  delay(3000);
#else
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_OUTPUT_LEVEL);
#endif

  // Drive GPIO0 LOW to enable active LOW detect on door pin
  digitalWrite(0, LOW);  // set data state first before switching to output
  pinMode(0, OUTPUT);
  pinMode(door_pin, INPUT);

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  DBGL("mounting FS...");

  if (SPIFFS.begin()) {
    DBGL("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DBGL("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DBGL("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          DBGL("\nparsed json");
          strcpy(host, json["host"]);
          strcpy(apiKey, json["apiKey"]);
        } else {
          DBGL("failed to load json config");
        }
      }
    }
  } else {
    DBGL("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_host("host", "Host Website ie. maker.ifttt.com", host, host_max_len);
  WiFiManagerParameter custom_apiKey("apiKey", "api key", apiKey, apiKey_max_len);

  
#ifdef DEBUG
  wifiManager.setDebugOutput(true);
#else
  wifiManager.setDebugOutput(false);
#endif

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_host);
  wifiManager.addParameter(&custom_apiKey);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  
  wifiManager.setTimeout(24*3600);  // 1 day
  //wifiManager.setTimeout(120);  // test only - 120 seconds

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  while(day_cnt++ < max_days_for_AP_config) {
    if (wifiManager.autoConnect("AutoConnectAP", "alarmpassword")) {
      break;
    }
  }

  // Idle if failed to connect
  if(WiFi.status() != WL_CONNECTED) { 
    DBGL("failed to connect and hit timeout");
    disconnect();  // config for lower pwr idle WiFi
    SET_LED(false);
    WRITE_LED;
    while(1) {  // Wait for next power cycle
      delay(2000);
    }
  }
  
  //if you get here you have connected to the WiFi
  DBGL("connected...yeey :)");

  //read updated parameters
  strcpy(host, custom_host.getValue());
  strcpy(apiKey, custom_apiKey.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DBGL("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["host"] = host;
    json["apiKey"] = apiKey;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DBGL("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  delay(2000); // a couple seconds connect time before disconnect
  //Wifi.disconnect(); // Test only to remove stored SSIDs
  disconnect();
  delay(sleep_time_in_ms); 

  previousMillis = millis(); 
  // Initial conditions set - door closed, disconnected at current time
}

void loop() {
  // Peridocally connect to WiFi to confirm availability and reset if none
  // Also check door pin and WiFi connect and send signal if door(s) open

  unsigned long currentMillis;

  door_open = digitalRead(door_pin) == DOOR_OPEN_STATE;
  if(door_open) {
    door_open_cnt++;
  }
  else {
    door_open_cnt = 0;
  }
  
  currentMillis = millis();

  // Connect WiFi if door open or periodic availability check
  if(door_open_cnt == 1 || (currentMillis - previousMillis >= check_wifi_interval_hours*3600*1000)) {
    previousMillis = currentMillis; 

    connect();
    
    door_open |= digitalRead(door_pin) == DOOR_OPEN_STATE; // in case went active during WiFi connect
    if(door_open) {    
      WiFiClient client;
      const int httpPort = 80;

      DBG("connecting to ");
      DBGL(host);
        
      if (!client.connect(host, httpPort)) {
        DBGL("connection failed");
        ESP.reset(); // or retry?
      }
  
      String url = "/trigger/door_opened/with/key/";
      url += apiKey;
      
      DBG("Requesting URL: ");
      DBGL(url);
      client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" + 
                   "Content-Type: application/x-www-form-urlencoded\r\n" + 
                   "Content-Length: 13\r\n\r\n" +
                   "value1=" + "open" + "\r\n");
      delay(1000);
      
    }

    disconnect();

    DBG("Heap ");
    DBGL(ESP.getFreeHeap());

  }    

  // At this point wifi is off, only polling door pin
  TOGGLE_LED;
  WRITE_LED;
  
  delay(sleep_time_in_ms);
}
