#include "RTClib.h"
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#include <WiFi.h>
#include "time.h"
#include <PubSubClient.h>

// Global WiFi status flag
bool wifiConnected = false;

const char* ssid = "1727";
const char* password = "357?M4g2";

const char* mqtt_server = "192.168.1.184";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 60 * 60; // Set your timezone offset
const int   daylightOffset_sec = 0;

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
    } else {
        Serial.println("");
        Serial.println("WiFi connect timeout, continuing without WiFi.");
        wifiConnected = false;
    }
}

void reconnect() {
    if (!wifiConnected) return; // Don't try MQTT if WiFi failed
    unsigned long startAttemptTime = millis();
    while (!client.connected() && millis() - startAttemptTime < 10000) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 2 seconds");
            delay(2000);
        }
    }
    if (!client.connected()) {
        Serial.println("MQTT connect timeout, continuing without MQTT.");
    }
}


Adafruit_AHTX0 aht;
TFT_eSPI tft = TFT_eSPI();
RTC_DS3231 rtc;     // Add this line near your global declarations

//clock color
#define BACKGROUND_COLOR TFT_BLACK
#define DATE_COLOR TFT_WHITE
#define TEMP_COLOR TFT_ORANGE
#define HUM_COLOR TFT_GREEN
#define TFT_BACK_LIGHT 25 //deep sleep, for turning off the screen


sensors_event_t humidity, temp;
String ahtTempStr, humidityStr, SoilHumidityStr;

//clock
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String yearStr, monthStr, dayStr, hourStr, minuteStr, dayOfWeek, dateStr, timeStr;


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


}



void setDisplayContent() {
    char buf[8];

    int offset_x = 20;
    int offset_y = 20;
    int tempfontsize = 6;
    
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
    tft.drawString(ahtTempStr, 10, temp_pos_y + offset_y, tempfontsize);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("o",30 + 85 + offset_x, temp_pos_y-3 + offset_y,tempfontsize - 2 -2); // Small "o" as degree symbol
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("C",30 + 95 + offset_x, temp_pos_y + offset_y, tempfontsize -2); // "C" for Celsius

    // Display humidity
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    //tft.drawString("Humidity", 20, 150, 3);
    dtostrf(roundf(humidity.relative_humidity * 100) / 100.0, 1, 2, buf); // 2 decimal places
    humidityStr = String(buf);
    int humid_pos_y = 140;
    tft.drawString(humidityStr, 150 + offset_x, humid_pos_y + offset_y, tempfontsize);
    tft.drawString("%", 95+ 130 + offset_x + 50, humid_pos_y + offset_y, tempfontsize - 2); // Percent symbol

}


void setRTCfromNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        rtc.adjust(DateTime(
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec
        ));
        Serial.println("RTC set from NTP!");
    } else {
        Serial.println("Failed to get time from NTP");
    }
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);

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
    //Set current time (uncomment only if you want to set RTC!)
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Initialize temperature/humidity sensor
    if (!aht.begin()) {
        Serial.println("Could not find AHT sensor!");
        tft.setTextSize(2);
        tft.drawString("AHT Sensor Error!", 10, 40);
        while (1) delay(10);
    }

    // Only set RTC from NTP if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        setRTCfromNTP();
    }

}

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000; // Try every 10 seconds

#define TIME_TO_SLEEP 50 //In seconds
#define TIME_TO_STAY_AWAKE 10 //In seconds

void loop() {
    static int lastUpdateMinute = -1;
    static float tempSum = 0.0;
    static float humSum = 0.0;
    static int sampleCount = 0;

    DateTime now = rtc.now();
    int currentMinute = now.minute();
    int currentSecond = now.second();

    // Take a reading and accumulate
    getSensorData();
    tempSum += temp.temperature;
    humSum += humidity.relative_humidity;
    sampleCount++;

    // Ensure MQTT connection before publishing
    if (wifiConnected) {
        if (!client.connected()) {
            reconnect();
        }
        client.loop();
        if (!isnan(temp.temperature) && !isnan(humidity.relative_humidity)) {
            String tempStr = String(temp.temperature, 2);
            String humStr = String(humidity.relative_humidity, 2);
            client.publish("home/esp32/temperature", tempStr.c_str(), true); // retain flag set
            client.publish("home/esp32/humidity", humStr.c_str(), true);
            Serial.println("Published current data to MQTT (with retain)");
        } else {
            Serial.println("Skipped MQTT publish: invalid sensor data");
        }
    }

    // If the minute has changed, update display with average
    if (lastUpdateMinute != currentMinute) {
        float avgTemp = tempSum / sampleCount;
        float avgHum = humSum / sampleCount;
        temp.temperature = avgTemp;
        humidity.relative_humidity = avgHum;
        setDisplayContent();
        // Reset for next minute
        tempSum = 0.0;
        humSum = 0.0;
        sampleCount = 0;
        lastUpdateMinute = currentMinute;
    }
    // Sleep for 5 seconds to save power
    Serial.println("Sleeping for 5 seconds...");
    delay(100);
    esp_sleep_enable_timer_wakeup(5 * 1000000ULL);
    esp_light_sleep_start();
}
