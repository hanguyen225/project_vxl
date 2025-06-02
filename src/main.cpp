#define BLYNK_TEMPLATE_ID "TMPL6FX9oQfNy"
#define BLYNK_TEMPLATE_NAME "Esp32 soil sens"
#define BLYNK_AUTH_TOKEN "WMCS5TZ8XGOgUFj518HxKMc6WCkUS7Fj"

#include "RTClib.h"
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define RELAY_PIN 26 //Relay pin (Water Pump)
#define SoilADCpin 33 //ADC pin (33-36) 

//light sleep
#define TIME_TO_SLEEP 60 //In seconds
#define TIME_TO_STAY_AWAKE 5 //In seconds


RTC_DS3231 rtc;
Adafruit_AHTX0 aht;
TFT_eSPI tft = TFT_eSPI();

//clock color
#define BACKGROUND_COLOR TFT_BLACK
#define DATE_COLOR TFT_WHITE
#define TEMP_COLOR TFT_ORANGE
#define HUM_COLOR TFT_GREEN

//Blynk
char ssid[] = "TheonLyK";
char pass[] = "Kien1812@@@";
char auth[] = BLYNK_AUTH_TOKEN;
SimpleTimer timer;

sensors_event_t humidity, temp;
String ahtTempStr, humidityStr, SoilHumidityStr;
int toggleAutoWatering = 1;
int toggleManualWatering = 0;
int relayState = 0; // Relay state
double humidityThreshold = 60; // Humidity threshold for the pump
double tempThreshold = 40;     // Temperature threshold for the pump
double soilHumidity = 0;       // Soil humidity value
double soilHumidityThreshold = 40; // Soil's humidity threshold for the pump

//clock
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String yearStr, monthStr, dayStr, hourStr, minuteStr, dayOfWeek, dateStr, timeStr;


// Link variable to data from Blynk
BLYNK_WRITE(V2) { toggleAutoWatering = param.asInt(); } // 1 = ON, 0 = OFF
BLYNK_WRITE(V3) { toggleManualWatering = param.asInt(); }
BLYNK_WRITE(V4) { humidityThreshold = param.asDouble(); }
BLYNK_WRITE(V5) { tempThreshold = param.asDouble(); }
BLYNK_WRITE(V7) { soilHumidityThreshold = param.asDouble(); } // Now V7 is for soilHumidityThreshold


void getSensorData() {
    // Get temperature and humidity
    aht.getEvent(&humidity, &temp);

    // Get current time from RTC
    DateTime now = rtc.now();
    yearStr = String(now.year(), DEC);
    monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
    dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
    hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC);
    minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
    dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];
    dateStr = dayStr + "-" + monthStr + "-" + yearStr;
    timeStr = hourStr + ":" + minuteStr;

    // Soil sensor
    int soilRaw = analogRead(SoilADCpin);
    soilHumidity = map(soilRaw, 4095, 0, 0, 100);

    
    // Serial output for monitoring soil sensor
    Serial.print("Soil sensor raw value: ");
    Serial.print(soilRaw);
    Serial.print(" | Mapped soil humidity: ");
    Serial.println(soilHumidity);
    
}

double get_soil_sensor_data(){
    int soilRaw = analogRead(SoilADCpin);
    soilHumidity = map(soilRaw, 4095, 0, 0, 100);
    return soilHumidity;
}

void sendToBlynk() { // Send data to cloud
    Blynk.virtualWrite(V0, temp.temperature);
    Blynk.virtualWrite(V1, humidity.relative_humidity);
    Blynk.virtualWrite(V6, soilHumidity); // Now send soilHumidity to V6
}

void setDisplayContent() {
    char buf[8];

    int offset_x = 20;

    // Display day of week, date, and time
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(dayOfWeek, 10, 10, 4);
    tft.drawString(dateStr, 180, 10, 4);
    tft.drawString(timeStr, 30, 50, 8);

    // Display temperature
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    //tft.drawString("Temperature", 10, 130, 3);
    dtostrf(roundf(temp.temperature * 100) / 100.0, 1, 2, buf); // 2 decimal places
    ahtTempStr = String(buf);
    int temp_pos_y = 140;
    tft.drawString(ahtTempStr, 20 + offset_x, temp_pos_y, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("o", 85 + offset_x, temp_pos_y-3, 2); // Small "o" as degree symbol
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("C", 95 + offset_x, temp_pos_y, 4); // "C" for Celsius

    // Display humidity
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    //tft.drawString("Humidity", 20, 150, 3);
    dtostrf(roundf(humidity.relative_humidity * 100) / 100.0, 1, 2, buf); // 2 decimal places
    humidityStr = String(buf);
    int humid_pos_y = 170;
    tft.drawString(humidityStr, 20 + offset_x, humid_pos_y, 4);
    tft.drawString("%", 95 + offset_x, humid_pos_y, 4); // Percent symbol

    /*

    // Display soil humidity (as integer, right-aligned before the % symbol)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Soil humidity", 180, 140, 3);

    int soilInt = (int)get_soil_sensor_data();
    SoilHumidityStr = String(soilInt);

    // Calculate the width of the number string in pixels (font size 6)
    int soilNumWidth = tft.textWidth(SoilHumidityStr, 4);
    // Set the right edge position (where you want the % symbol)
    int rightEdge = 160;
    // Calculate the x position so the number is right-aligned before the percent symbol
    int soilNumX = rightEdge - soilNumWidth;

    // Draw the number right-aligned
    tft.drawString(SoilHumidityStr, soilNumX, 200, 6);
    tft.drawString("%", 180, 150, 3); // Percent symbol stays at x=70
*/

    // Display soil humidity
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    //tft.drawString("Soil Humidity", 20, 150, 3);

    int soilInt = (int)get_soil_sensor_data();
    SoilHumidityStr = String(soilInt); 
    int soil_humid_pos_y = 140;
    tft.drawString(SoilHumidityStr, 170 + offset_x, soil_humid_pos_y, 4);
    tft.drawString("%", 200 + offset_x, soil_humid_pos_y, 4); // Percent symbol

    // Display water pump status
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Water Pump", 20 + offset_x, 200, 2);//kbiet why k dung dc font to hon 2
    if (relayState == 1) {
        tft.drawString("ON*", 180 + offset_x, 200, 2);// dung xong la k hien
    } else {
        tft.drawString("OFF", 180 + offset_x, 200, 2);
    }
}

void controlWaterPump() {
    if (toggleManualWatering == 1) {
        digitalWrite(RELAY_PIN, HIGH); // Turn ON relay (pump ON)
        relayState = 1;
        return;
    }

    if (toggleAutoWatering == 0) {
        digitalWrite(RELAY_PIN, LOW);  // Turn OFF relay (pump OFF)
        relayState = 0;
        return;
    }

    if (soilHumidity < soilHumidityThreshold || temp.temperature > tempThreshold) {
        digitalWrite(RELAY_PIN, HIGH); // Turn ON relay (pump ON)
        relayState = 1;
    } else {
        digitalWrite(RELAY_PIN, LOW);  // Turn OFF relay (pump OFF)
        relayState = 0;
    }
}

void setup() {
    Serial.begin(115200);


/*
    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected!");

    // Initialize Blynk
    Blynk.begin(auth, ssid, pass);

    // Set up a timer to send data to Blynk every 2 seconds
    timer.setInterval(2000L, sendToBlynk);
*/

    // Initialize TFT
    delay(200);
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);

     // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        tft.setTextSize(2);
        tft.drawString("RTC Error!", 10, 10);
        Serial.flush();
        while (1) delay(10);
    }
    // Set current time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));


    // Initialize temperature/humidity sensor
    if (!aht.begin()) {
        Serial.println("Could not find AHT sensor!");
        tft.setTextSize(2);
        tft.drawString("AHT Sensor Error!", 10, 40);
        while (1) delay(10);
    }

    // Initialize relay pin
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Start with pump OFF
}

void loop() {
    unsigned long start = millis();
    while (millis() - start < TIME_TO_STAY_AWAKE * 1000) { // Stay awake for some seconds to allow the code to run before sleeping again


    getSensorData();
    setDisplayContent();
    controlWaterPump();

    //Blynk.syncVirtual(V2, V3, V4, V5, V6, V7); // Sync both V6 and V7
    //Blynk.run();
    //timer.run();


    
        // Print to serial for debugging
    Serial.println(dayOfWeek + ", " + dateStr + " " + timeStr);
    Serial.print("Temperature: ");
    Serial.print(temp.temperature);
    Serial.println("°C");
    Serial.print("Humidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println("% RH");
    Serial.println();
    



    delay(1750); // Small delay to avoid flooding
    }
    // Enter light sleep
    Serial.println("Going to light sleep...");
    delay(100);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000ULL); // 60 seconds in microseconds
    esp_light_sleep_start();
}