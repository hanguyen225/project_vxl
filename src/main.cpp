#include "RTClib.h"
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "1727";
const char* password = "357?M4g2";

const char* mqtt_server = "192.168.1.184";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
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
    tft.drawString(ahtTempStr, 20, temp_pos_y + offset_y, tempfontsize);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("o",10 + 85 + offset_x, temp_pos_y-3 + offset_y,tempfontsize - 2); // Small "o" as degree symbol
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("C",10 + 95 + offset_x, temp_pos_y + offset_y, tempfontsize); // "C" for Celsius

    // Display humidity
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    //tft.drawString("Humidity", 20, 150, 3);
    dtostrf(roundf(humidity.relative_humidity * 100) / 100.0, 1, 2, buf); // 2 decimal places
    humidityStr = String(buf);
    int humid_pos_y = 140;
    tft.drawString(humidityStr, 160 + offset_x, humid_pos_y + offset_y, tempfontsize);
    tft.drawString("%", 95+ 130 + offset_x, humid_pos_y + offset_y, tempfontsize); // Percent symbol

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
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)).unixtime() + 24); // Add 24 seconds offset

    // Initialize temperature/humidity sensor
    if (!aht.begin()) {
        Serial.println("Could not find AHT sensor!");
        tft.setTextSize(2);
        tft.drawString("AHT Sensor Error!", 10, 40);
        while (1) delay(10);
    }

}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    
    getSensorData();
    setDisplayContent();

    String tempStr = String(temp.temperature, 2);
    String humStr = String(humidity.relative_humidity, 2);

    client.publish("home/esp32/temperature", tempStr.c_str());
    client.publish("home/esp32/humidity", humStr.c_str());

    // Print to serial for debugging
    Serial.println(dayOfWeek + ", " + dateStr + " " + timeStr);
    Serial.print("Temperature: ");
    Serial.print(temp.temperature);
    Serial.println("°C");
    Serial.print("Humidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println("% RH");
    Serial.println();
    
    delay(1000); // Small delay to avoid flooding
}
