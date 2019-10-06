#include <WiFi.h>
#include <Wire.h>  // i2c bus
#include <time.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoUniqueID.h>
#include <ArduinoOTA.h>
#include "SSD1306.h"  // OLED display
const uint8_t vbatPin = 35;
float VBAT; // battery voltage from ESP32 ADC read
SSD1306 display(0x3c, 21, 22);   // OLED display object definition (address, SDA, SCL)
int ONBOARD_LED = 14; // onboard blue LED
int WiFiLED = 0;  // Set WiFi disconnect status LED
char* device = "Clock";
char* model = "ClockTBeamLoRaESP32";
char* ver = "v1.0";
char* fw = "v1.05.051019";
char* fwname = "ESP32_vBat_OTA_Time_Date_Connectionv1.05";
char* criticalWiFi = "N";  // If Y, system reset performed after WiFi reconect fail followed by user defined timeout (criticalWiFiTimeOut) loop.
char* criticalWiFiTimeOut = "1000";  // default 1 second
char* screen_select = "Main_Screen";  // Call screen to display on base unit
const char* ssid = "SSID";
const char* password = "PASSWORD";
int MainDisplay = 1;
int HighFor = 50;
int LowFor = 10000; //10secs
int LowVolt = 3.29;
int highSince = 0;
int lowSince = 0;
int ledStatus = LOW;
char* wifi = "WiFi";
int count = 0;
int dwncount = 0;
int dwnflag = 0;
int upflag = 0;
const long updwntimer = 60000;
const long StartTimeout = 15000;
int resetcounter = 0;
int DebugMode = 0;
unsigned long previousMillis = 0;
unsigned long VBATpreviousMillis = 0;
unsigned long WIFIpreviousMillis = 0;
unsigned long LEDpreviousMillis = 0;
const long LEDinterval = 300;  // 3sec interval at which to flash LED (milliseconds)
String SerialNo;
// Change to your WiFi credentials and select your time zone
const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";  // UK, see below for others and link to database
String Format = "X";  // Time format M for dd-mm-yy and 23:59:59, "I" for mm-dd-yy and 12:59:59 PM, "X" for Metric units.
static String         Day_str, Time_str, WkDay_str, Mth_str, UpTime_str, DwnTime_str;
volatile unsigned int local_Unix_time = 0, next_update_due = 0;
volatile unsigned int update_duration = 60 * 60;  // Time duration in seconds, so synchronise every hour
static unsigned int   Last_Event_Time;
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
//unsigned long previousMillis = 0;        // will store last time TS was updated
const long interval = 1800000;           // 30min interval at which to run Write_TS()(milliseconds)
int TScount = 0;
//*****************************************************************************************
void IRAM_ATTR Timer_TImeout_ISR() {
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL_ISR(&timerMux);
  local_Unix_time++;
  portEXIT_CRITICAL_ISR(&timerMux);
}
//*****************************************************************************************
void Esp32Reset() {
  display.clear();
  display.drawString(0, 0, "WiFi Connect..");
  display.display();
  unsigned long currentMillis = millis();
  if (DebugMode == 1) {
    Serial.println("ESP Restart counter initiated");
    Serial.println(millis());
  }
  if (currentMillis >= 10000) display.drawString(0, 10, "Restart in 5 secs..");
  display.display();
  if (currentMillis - previousMillis >= StartTimeout)
    resetcounter = 1;
}
//*****************************************************************************************
WiFiClient  client;
String readString;
WiFiServer server(80);
String header;

void setup() {
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, LOW);
  Serial.begin(115200);
  SerialNo = (UniqueID, HEX);
  config_wire();
  UniqueIDdump(Serial);  // Get device serial number
  Serial.println (model);
  Serial.println (fw);
  Start_SSD1306_Setup();
  //  BatteryCheck();
  VBAT = (float)(analogRead(vbatPin)) / 4095 * 2 * 3.3 * 1.1;
  StartWiFi();
  WiFi.softAP("TBeamLoRa_CnC", "AP_PASSWORD");
  Start_Time_Services();
  Setup_Interrupts_and_Initialise_Clock();       // Now setup a timer interrupt to occur every 1-second, to keep seconds accurate
  server.begin();
}
//*****************************************************************************************
void loop()
{ ArduinoOTA.handle();
  yield();
  if (WiFi.status() == WL_DISCONNECTED) WiFi_Lost();
  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(ONBOARD_LED, LOW);
    dwnflag = 1;
    wifi = "WiFi";
  }
  Check_WiFi_ConnectStatus();  // Checks for loss of WiFi network cinnection & auto reconnect once WiFi network is available again.
  BatteryCheck();  if (VBAT < 3.29)LowBatt();
  if (VBAT < 3.05) BatteryTooLow();
  UpdateLocalTime();     // The variables 'Date_str' and 'Time_str' now have current date-time values
  if (VBAT >= 3.10) {
    display.clear();
    display.setFont(ArialMT_Plain_16);  // Set the Font size larger
    display.drawString(43, 48, String((VBAT), 2) + "v");
    display.drawString(0, 0, WkDay_str);  // Display current month
    display.drawString(95, 0, Mth_str);  // Display current week day
    display.setFont(ArialMT_Plain_24);  // Set the Font size larger
    display.drawString(50, 0, Day_str);  // Display current day number
    display.setFont(ArialMT_Plain_10); display.drawString(0, 28, wifi);
    display.setFont(ArialMT_Plain_16); display.drawString(0, 48, String (WiFi.RSSI()));
    display.setFont(ArialMT_Plain_10); display.drawString(100, 28, "Lost"); display.setFont(ArialMT_Plain_16); display.drawString(100, 38, String (dwncount)); //
    display.setFont(ArialMT_Plain_24); display.drawString((Format == "I" ? 0 : 34), 20, Time_str);  // Adjust position for addition of AM/PM indicator if required
    display.setFont(ArialMT_Plain_10); display.drawString(100, 53, DwnTime_str);
    display.setFont(ArialMT_Plain_10);  // Set the Font to normal
    display.display();  // Update display
  }
}
//*****************************************************************************************
void config_wire() {
  Wire.begin(21, 22);               // (sda,scl) Start the Wire service for the OLED display using pin=22 for SCL and Pin-21 for SDA on ESP32 Dev1 board.
  // Wire.begin(SDA, SCL, 100000);       // (sda,scl,bus_speed) Start the Wire service for the OLED display using pin=22 for SCL and Pin-21 for SDA on ESP32 Dev1 board.
}
//*****************************************************************************************
void UpdateLocalTime() {
  time_t now;
  if (local_Unix_time > next_update_due) { // only get a time synchronisation from the NTP server at the update-time delay set
    time(&now);
    //   Serial.println("Synchronising local time, time error was: " + String(now - local_Unix_time));
    // If this displays a negative result the interrupt clock is running fast or positive running slow
    local_Unix_time = now;
    next_update_due = local_Unix_time + update_duration;
  } else now = local_Unix_time;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  char time_output[30], day_output[30], wkday_output[30], mth_output[30];
  if (Format == "M" || Format == "X") {
    strftime(day_output, 30, "%d", localtime(&now)); // Formats date as: 24-05-17
    strftime(time_output, 30, "%R", localtime(&now));      // Formats time as: 14:05
    strftime(wkday_output, 30, "%a", localtime(&now)); // Formats date as: 24-05-17
    strftime(mth_output, 30, "%b", localtime(&now)); // Formats date as: 24-05-17
  }
  else {
    strftime(day_output, 30, "%m-%d-%y", localtime(&now)); // Formats date as: 05-24-17
    strftime(time_output, 30, "%r", localtime(&now));      // Formats time as: 2:05:49pm
  }
  Day_str = day_output;
  Time_str = time_output;
  WkDay_str = wkday_output;
  Mth_str = mth_output;
}
//*****************************************************************************************
void StartWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  if (DebugMode == 1) {
    Serial.print(F("\r\nConnecting to: ")); Serial.println(ssid);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    if (WiFi.status() == WL_CONNECTED)
    {
      digitalWrite(ONBOARD_LED, LOW);
      UpTime_str = Time_str;
      wifi = "WiFi";
    }
    if (WiFi.status() == WL_DISCONNECTED)
    {
      WiFi_Lost();
      Esp32Reset();
    }
    if (resetcounter == 1) ESP.restart();
    Serial.print(F("."));
  }
  if (DebugMode == 1) {
    Serial.print("WiFi connected to address: ");
    Serial.println(WiFi.localIP());
  }
  resetcounter = 2;

  // Port defaults to 3232                            // ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname(model); // Hostname defaults to esp3232-[MAC]               // ArduinoOTA.setHostname("myesp32");
  // ArduinoOTA.setPassword("ESP32Dev1"); // No authentication by default                     // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)         Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  ArduinoOTA.handle();
}
//*****************************************************************************************
void Setup_Interrupts_and_Initialise_Clock() {
  hw_timer_t * timer = NULL;
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &Timer_TImeout_ISR, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  //Now get current Unix time and assign the value to local Unix time counter and start the clock.
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  time_t now;
  time(&now);
  local_Unix_time = now + 1; // The addition of 1 counters the NTP setup time delay
  next_update_due = local_Unix_time + update_duration;
}
//*****************************************************************************************
void Start_Time_Services() {
  // Now configure time services
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);  // See below for other time zones
  delay(1000);  // Wait for time services
}
//*****************************************************************************************
void Start_SSD1306_Setup() {

  display.init();  // Initialise the display
  display.flipScreenVertically();  // Flip the screen around by 180°
  display.setContrast(255);  // Display contrast, 255 is maxium and 0 in minimum, around 128 is fine
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, model);
  display.drawString(0, 10, fwname);
  display.drawString(0, 20, fw);
  display.display();
}
//*****************************************************************************************
void Check_WiFi_ConnectStatus() {

  if (WiFi.status() == WL_DISCONNECTED)
  {
    wifbegincycle();
    if (DebugMode == 1) {
      Serial.println ("WiFi Start (Check_WiFi_ConnectStatus()");
    }
  }
}
//*****************************************************************************************
void wifbegincycle() {
  unsigned long WIFIcurrentMillis = millis();
  if (WIFIcurrentMillis - WIFIpreviousMillis >= 30000) {
    WIFIpreviousMillis = WIFIcurrentMillis;
    WiFi.begin(ssid, password);
  }
}
//*****************************************************************************************
void WiFi_Lost() {
  unsigned long currentMillis = millis();
  if (currentMillis - LEDpreviousMillis > LEDinterval) {
    LEDpreviousMillis = currentMillis;
    if (count % 2 == 0)
    {
      digitalWrite(ONBOARD_LED, HIGH);
    }
    else
    {
      digitalWrite(ONBOARD_LED, LOW);
    }
    count ++;
  }
  if (dwnflag == 1)
  {
    wifi = "- - - -";
    DwnTime_str = Time_str;
    dwncount++;
    dwnflag = 2;
  }

}
//*****************************************************************************************
void BatteryCheck() {
  unsigned long VBATcurrentMillis = millis();
  if (VBATcurrentMillis - VBATpreviousMillis >= 30000) {
    VBATpreviousMillis = VBATcurrentMillis;
    VBAT = (float)(analogRead(vbatPin)) / 4095 * 2 * 3.3 * 1.1;
  }
}
//*****************************************************************************************
void BatteryTooLow() {
  WiFi.mode(WIFI_OFF);
  display.clear();
  display.setFont(ArialMT_Plain_16);  // Set the Font size larger
  display.drawString(10, 10, "Battery too low");
  display.drawString(20, 30, "Recharge...");
  display.display();
  if (VBAT >= 3.10) {
    StartWiFi();
  }
}
//*****************************************************************************************
void LowBatt() {
  int now = millis();
  if (ledStatus == LOW && now - lowSince >= LowFor)
  {
    highSince = now;
    ledStatus = HIGH;
    digitalWrite(ONBOARD_LED, HIGH);
  }
  else if (ledStatus == HIGH && now - highSince >= HighFor)
  {
    lowSince = now;
    ledStatus = LOW;
    digitalWrite(ONBOARD_LED, LOW);
  }
}
//*****************************************************************************************

/*
  WiFi event status:
  Event: 0 - WIFI_READY
  Event: 2 - STA_START
  Event: 3 - Normal network connection
  Event: 4 - STA_CONNECTED
  Event: 5 - STA_DISCONNECTED & Reason: 3 - AUTH_LEAVE / Reason: 202 - AUTH_FAIL / Reason: 201 - NO_AP_FOUND
  Event: 6 - No network connection
  Event: 7 - STA_GOT_IP
  Event: 8 - STA_LOST_IP
  Example time zones see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  //const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";     // UK
  //const char* Timezone = "MET-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
  //const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
  //const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
  //const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA
  //const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
  //const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
  //const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
  //const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
  //const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia
*/
