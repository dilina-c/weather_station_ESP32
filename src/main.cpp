//Include required libraries
#include "WiFi.h"
#include <HTTPClient.h>
#include "time.h"
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;

// WiFi credentials
const char* ssid = "";// change SSID 
const char* password = "";    // change password 
// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "AKfycbya_pGm1BPL_80SSrPBDp6TZrprR-xdpPRWtSyMqfytxslqCagUGDKIMkHHxT6gt1Mg";    // change Gscript ID
//int count = 0;

#include <DHT.h>
#define DHT_SENSOR_PIN  21 // ESP32 pin GIOP21 connected to DHT11 sensor
#define DHT_SENSOR_TYPE DHT11

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  18        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

#include "SPIFFS.h"
#include <vector>
using namespace std;
#define station 6//station number 


DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void httprequest(String temperature, String humidity, String GOOGLE_SCRIPT_ID) {// this method create http reqests
  String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + "temp=" + temperature + "&humid=" + humidity+"&stat="+ station;
  //Serial.print("POST data to spreadsheet:");
  Serial.println(urlFinal);
  HTTPClient http;
  http.begin(urlFinal.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  //Serial.print("HTTP Status Code: ");
  //Serial.println(httpCode);
  //---------------------------------------------------------------------
  //getting response from google sheet
  String payload;
  if (httpCode > 0) {
    payload = http.getString();
    //Serial.println("Payload: " + payload);
  }
  //---------------------------------------------------------------------
  http.end();
  blinkLED();
}
void listAllFiles() {// this method remove file from SPIFF
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
}
void blinkLED(){
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(200);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(200);
  }
void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  SPIFFS.begin(true);
  File tempfile;
  File humidityfile;
  dht_sensor.begin(); // initialize the DHT sensor
  char i = 0;
  char j = 0;
  char k = 0;
  char t=0;
  char StartupEmptypoint;
  char n = 80; //offline recorded array length
  char WifiConnectWaitingTime=25;
  String temparray[n];//offline temperature log array
  String humidarray[n];//offline humidity log array
  delay(1000);

  // connect to WiFi
  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  Serial.flush();
  WiFi.begin(ssid, password);

  float humi  = dht_sensor.readHumidity();// read humidity
  float tempC = dht_sensor.readTemperature();// read temperature in Celsius
  float tempF = dht_sensor.readTemperature(true);// read temperature in Fahrenheit

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    k++;
    if (k > WifiConnectWaitingTime) { // if waiting time is over, then start offline data logging
      Serial.println("\ndata recording Offline...");
      if(!(SPIFFS.exists("/tempc.txt")) && !(SPIFFS.exists("/humidity.txt"))){
        File tempfile = SPIFFS.open("/tempc.txt", FILE_WRITE);
        File humidityfile = SPIFFS.open("/humidity.txt", FILE_WRITE);
        Serial.println("file write in write mode");
        tempfile.println(String(tempC));// log temperature to tempfile
        humidityfile.println(String(humi));//log humidity to humidityfile
        tempfile.close();
        humidityfile.close();  
      }else{
        File tempfilea = SPIFFS.open("/tempc.txt","a" );
        File humidityfilea = SPIFFS.open("/humidity.txt", "a");
        tempfilea.println(String(tempC));// log temperature to tempfile
        humidityfilea.println(String(humi));//log humidity to humidityfile
        Serial.println("file write in append mode");
        tempfilea.close();
        humidityfilea.close();
        }
      k = 0;
      break;// when process is done leave while() loop
    }

  }

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  print_wakeup_reason();
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP)+" Seconds");

  if ( isnan(tempC) || isnan(tempF) || isnan(humi)) {// check whether the reading is successful or not
    Serial.println("Failed to read from DHT sensor!");
  }

  if (WiFi.status() == WL_CONNECTED) {// when wifi is connected
    static bool flag = false;
    struct tm timeinfo;
//if there exists spiff files they should upload first to the google sheet
    if ((SPIFFS.exists("/tempc.txt")) && (SPIFFS.exists("/humidity.txt"))) {
     
      File tempfile1 = SPIFFS.open("/tempc.txt");
      File humidityfile1 = SPIFFS.open("/humidity.txt");
      vector<String> v1;
      vector<String> v2;

      while (tempfile1.available()) {
        v1.push_back(tempfile1.readStringUntil('\n'));
      }

      while (humidityfile1.available()) {
        v2.push_back(humidityfile1.readStringUntil('\n'));
      }

      tempfile1.close();
      humidityfile1.close();

      for (String s1 : v1) {
        //Serial.println(s1);
        temparray[i] = s1;//retrieve temperature log data to array 
        i++;
      }

      for (String s2 : v2) {
        //Serial.println(s2);
        humidarray[j] = s2;//retrieve humidity log data to array
        j++;
      }
      while (t <= n) {
        if(temparray[t]==0 && humidarray[t]==0){//prevent 0 values uploading to the google sheets when arrays not completly write
            Serial.println("\nArrays are empty..");
            StartupEmptypoint=t;
            break;
          }else{
            Serial.println("offline data uploading... ");
            Serial.print("temp :");
            Serial.print(temparray[t]);
            Serial.print("\t");
            Serial.print("humid :");
            Serial.print(humidarray[t]);
            Serial.print("\n");
            httprequest(temparray[t], humidarray[t], GOOGLE_SCRIPT_ID);

          }
          t++;  
      }
      if (t == n || t==StartupEmptypoint) {//remove two files after uploading log data to sheet
          Serial.println("\n\n---BEFORE REMOVING---");
          listAllFiles();
          SPIFFS.remove("/tempc.txt");
          SPIFFS.remove("/humidity.txt");
          Serial.println("\n\n---AFTER REMOVING---");
          listAllFiles();
          i = 0;
          j = 0;
        }
    }

    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    char timeStringBuff[50]; //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    Serial.print("Time:");
    Serial.println(asString);

    httprequest(String(tempC), String(humi), GOOGLE_SCRIPT_ID);// upload current data to sheet
    
  }
  //count++;
  delay(1000);
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();// start deep sleep
}
void loop() {}