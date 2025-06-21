#include "RTClib.h"
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <driver/dac.h>
#include <LiquidCrystal.h>

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

// Try multiple NTP servers for reliability
const char* ntpServer = "129.6.15.28"; // Direct IP for time.nist.gov
const long  gmtOffset_sec = 7 * 60 * 60; // GMT+7
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
RTC_DS3231 rtc;     // Add this line near your global declarations

//clock color
#define BACKGROUND_COLOR TFT_BLACK
#define DATE_COLOR TFT_WHITE
#define TEMP_COLOR TFT_ORANGE
#define HUM_COLOR TFT_GREEN
#define TFT_BACK_LIGHT 25 //deep sleep, for turning off the screen
#define BRIGHTNESS_DAC_PIN 25 // GPIO25 (DAC1)


// Pin mapping: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(14, 27, 26, 25, 33, 32);

sensors_event_t humidity, temp;
String ahtTempStr, humidityStr, SoilHumidityStr;

//clock
char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String yearStr, monthStr, dayStr, hourStr, minuteStr, dayOfWeek, dateStr, timeStr;

// Custom degree symbol for 1602 LCD
byte degreeChar[8] = {
  0b00111,
  0b00101,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

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
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(dayOfWeek + " ");
    lcd.print(temp.temperature, 1);
    lcd.write(byte(0)); // Print degree symbol
    lcd.print("C ");
    lcd.print(humidity.relative_humidity, 1);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print(timeStr + " ");
    lcd.print(dateStr);
}


void setRTCfromNTP() {
    Serial.print("Setting RTC from NTP server: ");
    Serial.println(ntpServer);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) { // 10s timeout
        Serial.println("NTP time received:");
        Serial.print("Year: "); Serial.println(timeinfo.tm_year + 1900);
        Serial.print("Month: "); Serial.println(timeinfo.tm_mon + 1);
        Serial.print("Day: "); Serial.println(timeinfo.tm_mday);
        Serial.print("Hour: "); Serial.println(timeinfo.tm_hour);
        Serial.print("Minute: "); Serial.println(timeinfo.tm_min);
        Serial.print("Second: "); Serial.println(timeinfo.tm_sec);
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

void setBrightness(uint8_t brightness) {
    // Map 0-255 to DAC output
    dac_output_enable(DAC_CHANNEL_1); // GPIO25
    dac_output_voltage(DAC_CHANNEL_1, brightness); // 0-255
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);

    // Initialize 1602 LCD
    lcd.begin(16, 2); // 16 columns, 2 rows
    lcd.clear();
    lcd.print("Booting up...");
    delay(1000);
    lcd.clear();

    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RTC Error!");
        Serial.flush();
        while (1) delay(10);
    }
    //Set current time (uncomment only if you want to set RTC!)
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Initialize temperature/humidity sensor
    if (!aht.begin()) {
        Serial.println("Could not find AHT sensor!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("AHT Error!");
        while (1) delay(10);
    }

    // Only set RTC from NTP if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        setRTCfromNTP();
    }

    // Setup DAC for brightness control
    setBrightness(59); // 70% brightness at startup

    lcd.begin(16, 2); // 16 columns, 2 rows
    lcd.print("Booting up...");
    lcd.createChar(0, degreeChar); // Create custom degree symbol
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
