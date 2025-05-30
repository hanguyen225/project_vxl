#define BLYNK_TEMPLATE_ID "TMPL6k8skf8vt"
#define BLYNK_TEMPLATE_NAME "Clock with Temp and Humidity"
#define BLYNK_AUTH_TOKEN "qUNfMaqJMIjAw_tzUFLDnG_x6SaEP9EB"

//Include necessary libraries
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiClient.h>

//Relay pin (Water Pump)
#define RELAY_PIN 26

//ADC pin (33-36)
#define SoilADCpin 33

Adafruit_AHTX0 aht;
TFT_eSPI tft = TFT_eSPI();

//Blynk
char ssid[] = "Nguyen Hieu";
char pass[] = "0123456789";
char auth[] = "qUNfMaqJMIjAw_tzUFLDnG_x6SaEP9EB";
SimpleTimer timer;

sensors_event_t humidity, temp;
String ahtTempStr, humidityStr, SoilHumidityStr;
int toggleAutoWatering = 1;
int toggleManualWatering = 0;
int relayState = 0; // Relay state
double humidityThreshold = 60; // Humidity threshold for the pump
double tempThreshold = 40; // Temperature threshold for the 

int soilHumidityThreshold = 40; // Soil's humidity threshold for the pump

// Link variable to data from Blynk
BLYNK_WRITE(V2) { toggleAutoWatering = param.asInt(); } // 1 = ON, 0 = OFF
BLYNK_WRITE(V3) { toggleManualWatering = param.asInt(); }
BLYNK_WRITE(V4) { humidityThreshold = param.asDouble(); }
BLYNK_WRITE(V5) { tempThreshold = param.asDouble(); }

BLYNK_WRITE(V6) { soilHumidityThreshold = param.asDouble(); }
double soilHumidity;


void getSensorData() {
    // Get temperature and humidity
    aht.getEvent(&humidity, &temp);

    soilHumidity = map(analogRead(SoilADCpin), 4095, 0, 0, 100);
}

void sendToBlynk() { // Send data to cloud
    Blynk.virtualWrite(V0, temp.temperature);
    Blynk.virtualWrite(V1, humidity.relative_humidity);

    Blynk.virtualWrite(V7, soilHumidity);
}

void setDisplayContent() {
    char buf[8];
    // Display temperature
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Temperature", 10, 60, 4);
    dtostrf(roundf(temp.temperature * 100) / 100.0, 1, 2, buf); //Round to 2 decimal places
    ahtTempStr = String(buf);
    tft.drawString(ahtTempStr, 20, 100, 6);

    // Display humidity
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("Humidity", 190, 60, 4);
    dtostrf(roundf(humidity.relative_humidity * 100) / 100.0, 1, 2, buf); //Round to 2 decimal places
    humidityStr = String(buf);
    tft.drawString(humidityStr, 180, 100, 6);

    /*
    // Display soil humidity
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("Soil humidity", , , ); //lam sau idk 
    dtostrf(roundf(humidity.relative_humidity * 100) / 100.0, 1, 2, buf); //Round to 2 decimal places
    SoilHumidityStr = String(buf);
    tft.drawString(SoilHumidityStr, , , );
*/

    // Display water pump status
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Water Pump", 10, 160, 4);
    if (relayState == 1) {
        tft.drawString("ON*", 180, 160, 4);
    } else {
        tft.drawString("OFF", 180, 160, 4);
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


    /* code goc
    if (humidity.relative_humidity < humidityThreshold || temp.temperature > tempThreshold) {
        digitalWrite(RELAY_PIN, HIGH); // Turn ON relay (pump ON)
        relayState = 1;
    } else {
        digitalWrite(RELAY_PIN, LOW);  // Turn OFF relay (pump OFF)
        relayState = 0;
    }
    */
}

void setup() {
    Serial.begin(115200);

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

    // Set up a timer to send data to Blynk every 1 second
    timer.setInterval(1000L, sendToBlynk);

    // Initialize TFT
    delay(200);
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);

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
    getSensorData();
    setDisplayContent();
    controlWaterPump();

    Blynk.syncVirtual(V2, V3, V4, V5, V7); // Sync virtual pins to get latest values from the server
    Blynk.run();
    timer.run();

    delay(500); // Small delay to avoid flooding
}