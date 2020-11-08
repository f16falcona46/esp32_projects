/*
    note: need add library FastLED from library manage
    Github: https://github.com/FastLED/FastLED

    note: Change Partition Scheme(Default -> NoOTA or MinimalSPIFFS)
*/
#include <M5StickC.h>
#include <math.h>
#include <FastLED.h>
#include <Wire.h>
#include <driver/rmt.h>
#include <driver/i2s.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_pm.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt_main.h>
#include <driver/adc.h>
#include <utility/MPU6886.h>
#include <MQTT.h>
#include <Adafruit_BMP280.h>
//#include <driver/adc_common.h>

extern const unsigned char error_48[4608];

//WiFi credentials
const char* ssid       = "Titan";
const char* password   = "4D24584210137E3"; 

// ntp Server that is used for timesync
const char* ntpServer =  "us.pool.ntp.org";

// define what timezone you are in
int timeZone = -18000;

//for MQTT
WiFiClient net;
MQTTClient client(512);
const char* mqtt_server = "192.168.1.108";

#define WAKEUP_DELAY (10 * 60 * 1000000)

//pressure sensor
#define BMP280_ADDR 0x76
Adafruit_BMP280 bmp;

hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t TimerCount = 0;

bool TestMode = false;

TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);

void IRAM_ATTR onTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    //digitalWrite(10, TimerCount % 2);
    TimerCount++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void I2C_Read_NBytes(uint8_t driver_Addr, uint8_t start_Addr, uint8_t number_Bytes, uint8_t *read_Buffer){
    
  Wire1.beginTransmission(driver_Addr);
  Wire1.write(start_Addr);  
  Wire1.endTransmission(false);
  uint8_t i = 0;
  Wire1.requestFrom(driver_Addr,number_Bytes);
  
  //! Put read results in the Rx buffer
  while (Wire1.available()) {
    read_Buffer[i++] = Wire1.read();
  }        
}

void I2C_Write_NBytes(uint8_t driver_Addr, uint8_t start_Addr, uint8_t number_Bytes, uint8_t *write_Buffer){

  Wire1.beginTransmission(driver_Addr);
  Wire1.write(start_Addr);
  Wire1.write(*write_Buffer);
  Wire1.endTransmission();

}

void ErrorMeg(uint8_t code, const char *str)
{
    printf("ErrorMeg: code=%02X str=%s\n", code, str);
    Disbuff.fillRect(0, 0, 160, 80, BLACK);
    Disbuff.pushImage(0, 16, 48, 48, (uint16_t *)error_48);
    Disbuff.setCursor(100, 10);
    Disbuff.setTextFont(2);
    Disbuff.printf("%02X", code);
    Disbuff.drawString("ERROR", 55, 10, 2);
    Disbuff.drawString("-----------------", 55, 30, 1);
    Disbuff.drawString(str, 55, 45, 1);
    Disbuff.drawString("check Hardware ", 55, 60, 1);
    Disbuff.pushSprite(0, 0);
    while (1)
        ;
}

void ErrorMeg(uint8_t code, const char *str1, const char *str2)
{
    printf("ErrorMeg: code=%02X str1=%s str2=%s\n", code, str1, str2);
    Disbuff.fillRect(0, 0, 160, 80, BLACK);
    Disbuff.pushImage(0, 16, 48, 48, (uint16_t *)error_48);
    Disbuff.setCursor(100, 10);
    Disbuff.setTextFont(2);
    Disbuff.printf("%02X", code);
    Disbuff.drawString("ERROR", 55, 10, 2);
    Disbuff.drawString(str1, 55, 30, 1);
    Disbuff.drawString(str2, 55, 45, 1);
    Disbuff.drawString("Check Hardware", 55, 60, 1);
    Disbuff.pushSprite(0, 0);
    while (1)
        ;
}

const char* convert_wifi_code(int code)
{
  switch (code) {
    case WL_CONNECTED:
    return "WL_CONNECTED";
    case WL_NO_SHIELD:
    return "WL_NO_SHIELD";
    case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
    case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
    default:
    return "unknown";
  }
}

void ntpSyncAlreadyConnected(bool display)
{
  // Set ntp time to local
  configTime(timeZone, 0, ntpServer);

  // Get local time
  struct tm timeInfo;
  if (getLocalTime(&timeInfo)) {
    // Set RTC time
    RTC_TimeTypeDef TimeStruct;
    TimeStruct.Hours   = timeInfo.tm_hour;
    TimeStruct.Minutes = timeInfo.tm_min;
    TimeStruct.Seconds = timeInfo.tm_sec;
    M5.Rtc.SetTime(&TimeStruct);

    RTC_DateTypeDef DateStruct;
    DateStruct.WeekDay = timeInfo.tm_wday;
    DateStruct.Month = timeInfo.tm_mon + 1;
    DateStruct.Date = timeInfo.tm_mday;
    DateStruct.Year = timeInfo.tm_year + 1900;
    M5.Rtc.SetData(&DateStruct);
    Serial.println("Time now matching NTP");
    if (display) {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(20, 15);
      M5.Lcd.println("S Y N C");
      delay(500);
    }
  }
}

void timeSync() {
  M5.Lcd.setTextSize(1);
  Serial.println("Syncing Time");
  Serial.printf("Connecting to %s ", ssid);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(20, 15);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      ++cnt;
      if (cnt > 5) {
        WiFi.reconnect();
        cnt = 0;
      }
  }
  Serial.println(" CONNECTED");
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(20, 15);
  M5.Lcd.println("Connected");
  ntpSyncAlreadyConnected(true);
  WiFi.disconnect();
}

#define PIN_CLK 0
#define PIN_DATA 34

static esp_pm_lock_handle_t rmt_freq_lock;
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_TX_GPIO_NUM GPIO_NUM_9
#define RMT_CLK_DIV (1) // 80000000 / 1(HZ)

void Displaybuff()
{
    Disbuff.pushSprite(0, 0);
}

void DisplayRTC()
{
    Disbuff.fillRect(0, 0, 160, 80, Disbuff.color565(0, 0, 0));
    Displaybuff();
    M5.Rtc.GetBm8563Time();
    RTC_TimeTypeDef time;
    M5.Rtc.GetTime(&time);

    Disbuff.setTextSize(3);
    Disbuff.setCursor(6, 25);
    Disbuff.setTextColor(WHITE);

    Wire.begin(0,26); //external
    if (!bmp.begin(BMP280_ADDR)) {
      Serial.println("Could not find a valid bmp280 sensor, check wiring!");
    }

    int shutdown_counter = 0;
    while ((!M5.BtnA.isPressed()) && (!M5.BtnB.isPressed()) && (shutdown_counter < 50)) {
      float vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
      float pressure = bmp.readPressure();
      Disbuff.fillRect(0, 0, 160, 80, Disbuff.color565(0, 0, 0));
      M5.Rtc.GetTime(&time);
      Disbuff.setTextSize(3);
      Disbuff.setTextColor(WHITE);
      Disbuff.setCursor(6, 40);
      Disbuff.printf("%02d:%02d:%02d", time.Hours, time.Minutes, time.Seconds);
      Disbuff.fillRect(0,0,160,36,Disbuff.color565(20,20,20));
      Disbuff.setTextSize(1);
      Disbuff.setCursor(32, 5);
      Disbuff.printf("vbat: %.3f", vbat);
      Disbuff.setCursor(32, 18);
      Disbuff.printf("pressure: %.2f", pressure);
      Displaybuff();
      M5.update();
      delay(100);
      ++shutdown_counter;
    }
    if (M5.BtnA.isPressed() || shutdown_counter >= 50) {
      delay(100);
      Wire.beginTransmission(BMP280_ADDR);
      Wire.write(BMP280_REGISTER_CONTROL);
      Wire.write(0x3C);
      while(Wire.endTransmission() != 0x00);
      M5.Axp.DeepSleep(WAKEUP_DELAY);
    }
    while (M5.BtnB.isPressed()) {
      timeSync();
      shutdown_counter = 0;
      M5.update();
    }
}

void setup()
{
  M5.begin();
  //Wire.begin(32,33,10000); //internal sensor
  Wire.begin(0,26); //external
  M5.update();
  bool button_wakeup = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);
  if (button_wakeup) {
    if (M5.BtnB.isPressed()) {
      TestMode = true;
    }
    
    M5.Lcd.setRotation(3);
    M5.Lcd.setSwapBytes(false);
    Disbuff.createSprite(160, 80);
    Disbuff.setSwapBytes(true);
    
    Disbuff.fillRect(0,0,160,80,BLACK);
    Displaybuff();
    M5.Axp.ScreenBreath(9);
    
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    
    timerSemaphore = xSemaphoreCreateBinary();
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 500000, true);
    timerAlarmEnable(timer);
  }
  uint8_t regdata;
  I2C_Read_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);
  regdata = regdata | 0b01000000;
  I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);
  //push air pressure data
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("Could not find a valid bmp280 sensor, check wiring!");
  }
  else {
    Serial.print("Connecting to ");
    Serial.println(ssid);
  
    WiFi.begin(ssid, password);

    int retry_count = 0;
    while (WiFi.status() != WL_CONNECTED && retry_count < 10) {
      delay(500);
      Serial.print(".");
      ++retry_count;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());

      ntpSyncAlreadyConnected(false);
      client.begin(mqtt_server, 1883, net);
      Serial.print("\nconnecting");
      retry_count = 0;
      client.disconnect();
      while (!client.connect("esp32adsf") && retry_count < 3) {
        Serial.print(".");
        delay(500);
        ++retry_count;
      }
      if (client.connected()) {
        RTC_TimeTypeDef TimeStruct;
        M5.Rtc.GetTime(&TimeStruct);
    
        RTC_DateTypeDef DateStruct;
        M5.Rtc.GetData(&DateStruct);
        
        float pressure = bmp.readPressure();
        char message[128];
        dtostrf(pressure, 1, 2, message);
        Serial.print("Pressure: ");
        Serial.println(message);
        snprintf(message, sizeof(message), "%04d-%02d-%02d %02d:%02d:%02d,%f", DateStruct.Year, DateStruct.Month, DateStruct.Date, TimeStruct.Hours, TimeStruct.Minutes, TimeStruct.Seconds, pressure);
        client.publish("/air_pressure", message);

        
        float vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
        snprintf(message, sizeof(message), "%04d-%02d-%02d %02d:%02d:%02d,%f", DateStruct.Year, DateStruct.Month, DateStruct.Date, TimeStruct.Hours, TimeStruct.Minutes, TimeStruct.Seconds, vbat);
        client.publish("/battery_voltage", message);
        client.disconnect();
        while (client.connected());
      }
      else {
        Serial.print("\nMQTT lastError: ");
        Serial.println(client.lastError());
        Serial.print("MQTT returnCode: ");
        Serial.println(client.returnCode());
      }
      WiFi.disconnect();
    }
    Wire.beginTransmission(BMP280_ADDR);
    Wire.write(BMP280_REGISTER_CONTROL);
    Wire.write(0x3C);
    while(Wire.endTransmission() != 0x00);
  }
  if (!button_wakeup) {
    M5.Axp.DeepSleep(WAKEUP_DELAY);
  }
}

void loop()
{
    DisplayRTC();
}
