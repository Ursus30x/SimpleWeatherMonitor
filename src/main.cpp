#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "time.h"

// Local config
#include "config.h"
#include "custom_chars.h"

/* Pin definitons */

#define I2C_SDA 8
#define I2C_SCL 9

/* Misc defines */
constexpr char DEGREE_CHAR = 223;

/* Global variables */

bool wifiConnected = false;

/* Device initializations */

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

/* Data structs */

struct localSensorData_t{
  sensors_event_t humidity;
  sensors_event_t temp;
  float pressure;
} localSensorData;

struct weatherApiData_t {
  float temp;
  float windSpeed;
  int weatherCode;
} weatherApiData;

/* Functions used both in loop and setup*/

void collectWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  http.begin(weatherAPI);
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;

      if (!deserializeJson(doc, payload)) {
        weatherApiData.temp = doc["current"]["temperature_2m"];
        weatherApiData.windSpeed = doc["current"]["wind_speed_10m"];
        weatherApiData.weatherCode = doc["current"]["weather_code"];
      } else {
        Serial.print("JSON Parse failed");
      }
    }
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

/* Setup functions */

void i2cSetup()
{
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
}

void lcdSetup()
{
  lcd.init();
  lcd.backlight();

  // Register custom chars
  for(uint8_t char_index = WATER_DROP_CHAR; char_index < (uint8_t)UNDEF_CHAR; char_index++){
    lcd.createChar(char_index, custom_chars[char_index]);
  }
}

int sensorSetup()
{
  // Initialize AHT20
  if (!aht.begin()) {
    Serial.println("Could not find AHT20!");
    lcd.setCursor(0, 1);
    lcd.print("AHT20 Fail!");
    delay(2000);

    return 1;
  }

  // Initialize BMP280
  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println("BMP280 Fail!");
    lcd.setCursor(0, 1);
    lcd.print("BMP280 Fail!");
    delay(2000);

    return 2;
  }

  return 0;
}

void wifiInfoPrint(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Starting WiFi");
  lcd.setCursor(0,1);
  lcd.print(WIFI_SSID);

  delay(1000);
}

void wifiConfig() {
  String hostname = "ESP32C3-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  WiFi.disconnect(true);

  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(500);

  WiFi.mode(WIFI_STA);

  Serial.println(hostname.c_str());
  WiFi.setHostname(hostname.c_str());

  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  wifi_config_t conf = {};
  memset(&conf, 0, sizeof(conf));
  strcpy((char*)conf.sta.ssid, WIFI_SSID);
  strcpy((char*)conf.sta.password, WIFI_PASS);

  conf.sta.pmf_cfg.capable = false;
  conf.sta.pmf_cfg.required = false;
  conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  esp_wifi_set_config(WIFI_IF_STA, &conf);

  esp_wifi_start();
  delay(500);
}

void wifiConnect(){
  uint8_t timeout = 0;
  WiFi.begin();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting");
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    const uint8_t dotPosition = timeout % 3;

    lcd.setCursor(10 + dotPosition,0);
    if(dotPosition == 0){
      lcd.print("    ");
      lcd.setCursor(10 + dotPosition,0);
    }
    lcd.print(".");

    timeout++;
    delay(600);
  }
}

int wifiSetup()
{
  wifiInfoPrint();
  wifiConfig();
  wifiConnect();

  if (WiFi.status() == WL_CONNECTED)
  {
    lcd.clear();
    lcd.print("WiFi OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);
    return 0;
  }

  // Last ditch effort to try to connect
  delay(8000);
  WiFi.reconnect();

  if (WiFi.status() == WL_CONNECTED){
    lcd.clear();
    lcd.print("WiFi OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);
    return 0;
  }

  lcd.clear();
  lcd.print("WiFi Failed!");
  delay(3000);
  return 1;
}

void ntpSetup() {
  tm timeinfo;

  configTime(3600, 3600, ntpServer, "time.google.com");

  if(!getLocalTime(&timeinfo, 5000)){
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synced!");
  }
}

void setup()
{
  uint8_t ret = 0;
  localSensorData = {};
  weatherApiData  = {};

  Serial.begin(115200);
  randomSeed(analogRead(0));

  i2cSetup();
  lcdSetup();

  lcd.print("Starting device");
  delay(2000);
  ret = sensorSetup();
  if(ret) return; // TODO: add error state

  ret = wifiSetup();
  if(ret == 0) wifiConnected = true;
  if(ret) return; // TODO: add error state

  ntpSetup();

  delay(2000);
  lcd.clear();
}

/* Loop functions */

void maintainWifi()
{
  static wl_status_t lastWifiStatus = WiFi.status();
  wl_status_t currentWifiStatus = WiFi.status();

  if (currentWifiStatus != WL_CONNECTED) {
    lcd.setCursor(0,1);
    lcd.print("Connection lost ");
    delay(2000);

    Serial.print("Trying to reconnect");
    WiFi.reconnect();

    wifiConnected = false;
  }
  else if(currentWifiStatus == WL_CONNECTED && currentWifiStatus != lastWifiStatus){
    lcd.setCursor(0,1);
    lcd.print("Reconeccted!");
    delay(2000);
    wifiConnected = true;
  }

  lastWifiStatus = currentWifiStatus;
}

void collectSensorData()
{
  aht.getEvent(&localSensorData.humidity,&localSensorData.temp);
  localSensorData.pressure = bmp.readPressure() / 100.0f;
}

void updateWeatherIcon(int code) {
  static int lastLoadedCode = -1;

  int iconIndex = SUNNY_WEATHER_CHAR;

  if (code >= 0 && code <= 1) {
    iconIndex = SUNNY_WEATHER_CHAR;
  } else if ((code >= 2 && code <= 3) || (code >= 45 && code <= 48)) {
    iconIndex = CLOUDY_WEATHER_CHAR;
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    iconIndex = RAINY_WEATHER_CHAR;
  } else if (code >= 71 && code <= 77 || code == 85 || code == 86) {
    iconIndex = SNOWY_WEATHER_CHAR;
  } else if (code >= 95) {
    iconIndex = STORMY_WEATHER_CHAR;
  }
  else{ // use sunny as a fallback
    iconIndex = SUNNY_WEATHER_CHAR;
  }

  if (iconIndex != lastLoadedCode) {
    lcd.createChar(DYNAMIC_WEATHER_CHAR, (uint8_t*)dynamic_weather_chars[iconIndex]);
    lastLoadedCode = iconIndex;
    Serial.printf("LCD CGRAM: Loaded icon index %d into slot %d\n", iconIndex, DYNAMIC_WEATHER_CHAR);
  }
}


void printInfoRow0()
{
  static tm timeinfo;

  lcd.setCursor(0, 0);

  // Time
  if (!getLocalTime(&timeinfo))
  { // If time is not yet synced, show dashes
    lcd.setCursor(0, 0);
    lcd.print("--:--  ");
  }
  else
  {
    lcd.setCursor(0, 0);

    if (timeinfo.tm_hour < 10) lcd.print("0");
    lcd.print(timeinfo.tm_hour);
    lcd.print(":");

    if (timeinfo.tm_min < 10) lcd.print("0");
    lcd.print(timeinfo.tm_min);
    lcd.print("  ");
  } // TODO: add possibility of manual time setting

  // Humidity
  lcd.write(WATER_DROP_CHAR);
  lcd.print(localSensorData.humidity.relative_humidity, 0);
  lcd.print("%  ");

  // Local temp
  lcd.print(localSensorData.temp.temperature, 0);
  lcd.print(DEGREE_CHAR);
  lcd.print(" ");
}

void printInfoRow1()
{
  lcd.setCursor(0,1);

  // Wheater api provided temp and wheater code
  lcd.write(DYNAMIC_WEATHER_CHAR); //TODO: Add weahter symbol managment
  lcd.print(weatherApiData.temp,0);
  lcd.print(DEGREE_CHAR);

  // Wind speed (from wheater api)
  lcd.setCursor(6,1);
  lcd.print(weatherApiData.windSpeed, 0);
  lcd.print("k");
  lcd.write(WIND_CHAR);

  // Local pressure
  lcd.setCursor(10,1);
  if (localSensorData.pressure < 1000) lcd.print(" ");
  lcd.print(" ");
  lcd.write(PRESSURE_CHAR);
  lcd.print(localSensorData.pressure, 0);
}

void printInfo()
{
  printInfoRow0();
  if(wifiConnected){
    printInfoRow1();
  }else{
    lcd.setCursor(0,1);
    lcd.print("No wifi");
  }
}

void loop()
{
  static unsigned long lastWeatherUpdate = 0;

  // Check if wifi connection is still working
  maintainWifi();

  // Collect data
  collectSensorData();
  if (millis() - lastWeatherUpdate > 900000 || lastWeatherUpdate == 0) {
    collectWeatherData();
    updateWeatherIcon(weatherApiData.weatherCode);
    lastWeatherUpdate = millis();
  }

  // Print data to LCD
  printInfo();

  delay(1000);
}