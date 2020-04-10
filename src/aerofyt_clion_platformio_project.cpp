// Aéropot Algorithm (WIP)

#define BLYNK_PRINT Serial // defines the object that is used for printing (Mega <> USB)
// #define BLYNK_DEBUG // Optional, this enables more detailed prints
// #define BLYNK_MAX_READBYTES 1024

/******************************** Libraries ********************************************/
/***************************************************************************************/

// - Libraries list
#include "ESP8266_Lib.h"
#include "BlynkSimpleShieldEsp8266.h"
#include "WiFiEsp.h"

#include "ArduinoJson.h"
#include "TimeLib.h"
#include "WidgetRTC.h"

#include "SPI.h"
#include "Wire.h"
#include "OneWire.h"

#include "SimpleTimer.h"

// -- Sensor libraries
#include "BH1750.h"
#include "DallasTemperature.h"
#include "GravityTDS.h"
#include "DHT.h"
#include "LedControl.h"
#include "Adafruit_SI1145.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "MQ135.h"
#include "HX711.h"

// -- C++ functions
#include "math.h"
#include "stdarg.h"
#include "stdio.h"


// -- Definitions and initializations
#define EspSerial Serial3 // Hardware Serial communication between ATMega2560 <> ESP8266
#define ESP8266_BAUD 115200 // ESP8266 baud rate, do not change!
#define ONE_WIRE_BUS 13 // DS18B20 Pin
#define TdsSensorPin A3
#define gas A4 // Analog pin for the MQ135
#define BME280_ADDRESS (0x76) // BME280 I2C address

// #define DHTTYPE DHT22
// #define DHTPIN 51 // Digital pin connected to the DHT sensor

ESP8266 wifi(&EspSerial);
BH1750 lightMeter;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempMeter(&oneWire);
GravityTDS gravityTds;
// BlynkTimer timer; // MAX_TIMERS set to = 24 in the library BlynkTimer.h
SimpleTimer timer;
WidgetRTC rtc;
Adafruit_SI1145 uv = Adafruit_SI1145(); // Initialize GY1145
MQ135 mq135 = MQ135(gas); // Initialize MQ135
Adafruit_BME280 bme;
HX711 scale;
// DHT dht(DHTPIN, DHTTYPE); // Initialize DHT11


/******************************** Constants and variables ******************************/
/***************************************************************************************/

// - OUTPUT (PWM PINS 2-13 [=11])
int mister = 3;
int fan = 4;
int indicationOne = 5;
int indicationTwo = 6;
int lightOne = 8;
int lightTwo = 9;
int levelTrg = 22;

// - INPUT (PINS 22-53 [=31] + A0-A15 [=15])
// int levelEcho = 53;
int tds = A3;
int ph = A2;

// -- MAX471
int current = A0;
int voltage = A1;

// -- HX711
const int LOADCELL_DOUT_PIN = A5;
const int LOADCELL_SCK_PIN = A6;

// -- MAX7219
int dIn = 28;
int clk = 24;
int cs = 26;

int const screens = 4;
int const dots = 8;
float totalDots = screens * dots;
float segmentsPercentage = 1 / totalDots;

LedControl lc = LedControl(dIn, clk, cs, screens); // Initialize MAX7219

// - Variables
float waterLevel;
float tdsValue;

float temp = 25;

int lifeCountdown = 5;
int timerLoopLifeCountdown;
double cyclePumpMister = 20;

// -- PH
float calibration = 11; // 21.28
int sensorValue = 0;
unsigned long int avgValue;
float b;
int buf[10];

// WRITE RUNTIME FUNCTION + DISPLAY
long cas = (millis() / 1000) / 60;


// Toggle GLOBAL functions
bool isFirstConnect = true; // Sync all with Blynk app when it is connected for the first time
bool isblynkAllowed = true;
bool loopActive = false;
bool i2cActive = false;
bool lightActive = false;
bool loopPumpActive = true;
bool manualControl = false;
const bool serialActive = false; // Enable or disable Serial print

/*********************************** Blynk settings ************************************/
/***************************************************************************************/

char auth[] = "2f8ac3fa803d414d9bd9edfbdbef5d86"; // 42ef99a79dfe4eea8876c7931585ce9a
char ssid[] = "B-LINK";
char pass[] = "04061957";
char server[] = "blynk-cloud.com";
int port = 8442;

WidgetTerminal terminal(V6);

#ifdef BLYNK_PRINT

BLYNK_CONNECTED() {
    Blynk.syncAll();

    Blynk.syncVirtual(V17, V18, V19); // Sync mist toggles

    Blynk.syncVirtual(V22, V23, V24); // Sync control toggles

    rtc.begin();
    Serial.println("Blynk data synced.");
}

/*
// This is called when Blynk App is opened
BLYNK_APP_CONNECTED() {
    Serial.println("App connected.");
}

// This is called when Blynk App is closed
BLYNK_APP_DISCONNECTED() {
    Serial.println("App disconnected.");
}
*/

#endif


/********************************** Blynk functions ************************************/
/***************************************************************************************/

void reconnectBlynk() { // reconnect to server if disconnected, timer checks every 10 seconds
    // Blynk.connect();
    // Blynk.run();

    if (!Blynk.connected()) {
        Serial.println("Lost connection.");
        terminal.println("Lost connection.");

        // Blynk.connect();
        // Blynk.run();

        if (Blynk.connect()) {
            Serial.println("Reconnected.");
            terminal.println("Reconnected.");

            // Blynk.run();
            Blynk.syncAll();

        } else {
            Serial.println("Not reconnected.");
            // Serial.println("Restarting Blynk.begin ...");

            // Blynk.begin(auth, wifi, ssid, pass);
            // Blynk.connect();
            // Blynk.run();
            // reconnectBlynk();
        }
    }
}

void tryConnecting() {
    if (!Blynk.connected()) {
        Blynk.connect();  // try to connect for 15 seconds, every time, as set  in: timer.setInterval(60000, tryConnecting) in void setup
        // but let the main loop run

        if (Blynk.connected()) {
            if (isFirstConnect) {
                // do some none relevant stuff here, and
                isFirstConnect = false;
            }

            //Serial.println("Connected to the Blynk server.");
        }
    }
}

void checkBlynk() {
    if (isblynkAllowed) {

        if (isFirstConnect) { tryConnecting(); };

        if (Blynk.connected()) { Blynk.run(); }
        else {
            Blynk.run();
            reconnectBlynk();
            Blynk.run();
        };
    }
}

void wifiSignal() {
    long rssi = WiFi.RSSI();
    Serial.println("===================");
    Serial.println("RSSI: " + (String) rssi + " dBm");

    // Blynk.virtualWrite(V_DBM, WiFi.RSSI());

    /*
    -30 dBm: amazing  (unlikely in real life)
    -67 dBm: strong   (minimum for realtime hd streaming)
    -70 dBm: okay     (minimum for reliable packet delivery)
    -80 dBm: weak     (minimum signal strength for basic connectivity. packet delivery may be unreliable)
    -90 dBm: unusable (any functionality is highly unlikely)
    */
}

BLYNK_WRITE(V24) {
    // Light active
    lightActive = param.asInt();
}

BLYNK_WRITE(V23) {
    // Loop active
    loopActive = param.asInt();
}

BLYNK_WRITE(V22) {
    // Manual light control
    manualControl = param.asInt();
}

BLYNK_WRITE(V15) {
    // BLUE/RED LED
    analogWrite(2, param.asInt());
}

BLYNK_WRITE(V13) {
    // FULL SPECTRUM LED
    analogWrite(3, param.asInt());
}

BLYNK_WRITE(V16) {
    // GREEN LED
    analogWrite(4, param.asInt());
}

BLYNK_WRITE(V14) {
    // WARM WHITE LED
    analogWrite(5, param.asInt());
    // analogWrite(5, map(param.asInt(), 0, 50, 0, 255));
}

BLYNK_WRITE(V20) {
    // FAN
    // analogWrite(7, param.asInt());
    analogWrite(7, map(param.asInt(), 0, 5, 0, 200));
}

BLYNK_WRITE(V17) {
    digitalWrite(37, param.asInt());
}

BLYNK_WRITE(V18) {
    // MISTER TOP
    digitalWrite(8, param.asInt());
}

BLYNK_WRITE(V19) {
    // MISTER BOTTOM
    digitalWrite(9, param.asInt());
}


BLYNK_WRITE(V21) {
    TimeInputParam t(param);

    // Process start time

    if (t.hasStartTime()) {
        Serial.println(String("Start: ") +
                       t.getStartHour() + ":" +
                       t.getStartMinute() + ":" +
                       t.getStartSecond());
    } else if (t.isStartSunrise()) {
        Serial.println("Start at sunrise");
    } else if (t.isStartSunset()) {
        Serial.println("Start at sunset");
    } else {
        // Do nothing
    }

    // Process stop time

    if (t.hasStopTime()) {
        Serial.println(String("Stop: ") +
                       t.getStopHour() + ":" +
                       t.getStopMinute() + ":" +
                       t.getStopSecond());
    } else if (t.isStopSunrise()) {
        Serial.println("Stop at sunrise");
    } else if (t.isStopSunset()) {
        Serial.println("Stop at sunset");
    } else {
        // Do nothing: no stop time was set
    }

    // Process timezone
    // Timezone is already added to start/stop time

    Serial.println(String("Time zone: ") + t.getTZ());

    // Get timezone offset (in seconds)
    Serial.println(String("Time zone offset: ") + t.getTZ_Offset());

    // Process weekdays (1. Mon, 2. Tue, 3. Wed, ...)

    for (int i = 1; i <= 7; i++) {
        if (t.isWeekdaySelected(i)) {
            Serial.println(String("Day ") + i + " is selected");
        }
    }

    Serial.println();
}


/********************************** Custom functions ***********************************/
/***************************************************************************************/

// An algorithm to reduce measurement error from a sensor input
#define SAMPLES 25 // number of samples to take mean of
#define TOLERANCE 0.05 // % range of allowable deviation between current mean and new values, expressed as decimal

#define BUFFER 10 // Buffer size (number of readings to use in rolling Mean)
#define SENSORPIN 3 // Analogue input pin from sensor

float meanValue(float data[]) {

/*    // Fill buffer with initial data
    int n = 0;
    while (n < BUFFER) {
        data[n] = analogRead(SENSORPIN);
        n++;

        Serial.println(data[n]); // DELETE
    }*/

    // Calculate Raw Mean & SD
    float datasum = 0;
    float diffsum = 0;
    for (int n = 0; n < BUFFER; n++) {
        datasum += data[n];

        // DEBUG
        /*Serial.print("Input meanValue[");
        Serial.print(n);
        Serial.print("]: ");
        Serial.println(data[n]);*/
    }
    float mean = float(datasum / BUFFER);
    for (int n = 0; n < BUFFER; n++) {
        diffsum += ((data[n] - mean) * (data[n] - mean));
    }
    float sd = sqrt(diffsum / (BUFFER - 1));

    // Recalculate Corrected Mean only using data within range +/- 1SD of Raw Mean
    if (sd != 0) {      // Avoid divide by zero error if sd = 0
        float newSum = 0; // Sum of readings within acceptable range
        float newLen = 0; // Number of readings within acceptable range
        int n = 0;
        while (n < BUFFER) {
            if ((data[n] < (mean + sd)) and (data[n] > (mean - sd))) {
                newSum += data[n];
                newLen++;
            }
            n++;
        }
        mean = newSum / newLen; // Corrected Mean

        // DEBUG
        // Serial.print("Corrected Mean: ");
        // Serial.println(mean);
    }

/*    if ((sample < (1 + TOLERANCE) * mean) and (sample > (1 - TOLERANCE) * mean)) {
        total = total + sample; // if new reading is within range, calculate new mean
        n = n + 1;
        mean = total / n;
    }*/

    // Add your code here to do stuff with variable float 'mean' = the (Corrected) Mean

    // Read new sensor reading, append to buffer and delete oldest reading
    for (int n = (BUFFER - 1); n > 0; n--) {
        data[n] = data[n - 1];
    }
    data[0] = analogRead(SENSORPIN);

    // Return Corrected Mean
    return mean;
}

void setLc() {
    for (int i = 0, j = 1; i < screens; i++) { // j = j + floor(14 / screens)
        lc.shutdown(i, false); // turn off power saving, enables display
        lc.setIntensity(i,
                        15); // sets brightness (0~15 possible values) + in case there are more than one MAX7219 daisy chained, the first of the two digits indicates of which module the intensity is to be set.
        lc.clearDisplay(i); // clear screen

        if (serialActive) {
            Serial.println("SetLc Intensity:" + (String) j); // DEBUG
        }
    }
}


/*// Functions WIP
void functions() {

// ALTERNATIVE
    // HC-SR04 Distance Sensor / Water Level

        long diffTrgEcho;

    digitalWrite(levelTrg, LOW);
    delayMicroseconds(2);
    digitalWrite(levelTrg, HIGH);
    delayMicroseconds(10);
    digitalWrite(levelTrg, LOW);

    diffTrgEcho = pulseIn(levelEcho, HIGH);

    waterLevel = diffTrgEcho * 0.034 / 2;

    Serial.println();
    Serial.print("Distance: ");
    Serial.print(waterLevel);
    Serial.print(" cm");
    Serial.println();
    Serial.println();

    // Delay
    delay(2500);

        // Blynk.virtualWrite(V6, waterLevel);

    // terminal.print("1: ");
    // terminal.println(waterLevel);
    // terminal.print("2: ");
    // terminal.println(meanDistance);

        // EDIT !!
    while ((n < SAMPLES) and (total != 0)) {

        distance = sonicDistance();

        //diffTrgEcho = pulseIn(levelEcho, HIGH);

        // waterLevel = diffTrgEcho*(0.034/2);
        if ((distance < ((1 + TOLERANCE) * meanDistance)) and (distance > ((1 - TOLERANCE) * meanDistance))) {
            total = total + distance;
            n = n + 1;
            meanDistance = total / n;
        }
    }

}*/

String returnClock() {
    // RTC Time/Date

    char TimeStamp[32];

    sprintf(TimeStamp, "%02d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());

    return TimeStamp;
}


void i2cScanner() {
    Serial.println("===================");
    Serial.println("I2C scanner. Scanning ...");
    byte count = 0;

    for (byte i = 1; i < 120; i++) {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0) {
            Serial.print("Found address: ");
            Serial.print(i, DEC);
            Serial.print(" (0x");
            Serial.print(i, HEX);
            Serial.println(")");
            count++;
        }
    }
    Serial.println("Done.");
    Serial.print("Found ");
    Serial.print(count, DEC);
    Serial.println(" device(s).");
}


/*********************************** Loop Functions ************************************/
/***************************************************************************************/

void displayClock() {
    // RTC Time/Date for the Blynk time widget

    char Date[16];
    char Time[16];

    sprintf(Date, "%04d/%02d/%02d", year(), month(), day());
    sprintf(Time, "%02d:%02d:%02d", hour(), minute(), second());

    Blynk.virtualWrite(V0, Date);
    Blynk.virtualWrite(V2, Time);

    /*Serial.println("===================");
    Serial.print("Date: ");
    Serial.print(Date);
    Serial.print("  |  Time: ");
    Serial.println(Time);*/
}


/*void loopHCSR04() {
    // HC-SR04 Distance Sensor / Water Level

    long duration;
    float distance;

    float dataArray[BUFFER]; // Make an array of appropriate size for Buffer

    // Fill buffer with initial data
    int n = 0;
    while (n < BUFFER) {

        delayMicroseconds(25);
        digitalWrite(levelTrg, LOW);
        delayMicroseconds(2);

        digitalWrite(levelTrg, HIGH);
        delayMicroseconds(10);

        digitalWrite(levelTrg, LOW);

        duration = pulseIn(levelEcho, HIGH);

        distance = duration / 58.2;

        dataArray[n] = distance;
        n++;
    }

    waterLevel = meanValue(dataArray);
    waterLevel = 11.5 - waterLevel; // the number is a distance of the sensor from the bottom of the planter

    Serial.println("===================");
    Serial.println("HC-SR04 Distance Sensor / Water Level");

    Serial.print("Water level: ");
    Serial.print(waterLevel);
    Serial.println(" cm");

    Blynk.virtualWrite(V5, waterLevel);


    // Blynk Webhook V0 (string document, int waterLevel) 1 + 1

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("waterLevel");
    field1["doubleValue"] = waterLevel;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    *//*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*//*

    // Blynk.virtualWrite(V1, "HCSR04", jsonMessage);

    jsonBuffer.clear();
}*/

void loopMAX471() {
    // MAX471 Voltage/Current Sensor

    int vt_read = analogRead(voltage);
    int at_read = analogRead(current);

    float voltage = vt_read * (5.0 / 1024.0) * 5.0;
    float current = at_read * (5.0 / 1024.0);
    float power = voltage * current;

    if (serialActive) {
        Serial.println("===================");
        Serial.println("MAX471 Voltage/Current Sensor");

        Serial.print("Voltage: ");
        Serial.print(voltage, 1);
        Serial.print(" V  |  Current: ");
        Serial.print(current, 3);
        Serial.print(" A  |  Power: ");
        Serial.print(power, 3);
        Serial.println(" W");
    }


    // Blynk Webhook V0 (string document, int voltage, int current, int power) 1 + 3

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 5 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("voltage");
    field1["doubleValue"] = voltage;

    JsonObject &field2 = fields.createNestedObject("current");
    field2["doubleValue"] = current;

    JsonObject &field3 = fields.createNestedObject("power");
    field3["doubleValue"] = power;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    Blynk.virtualWrite(V1, "MAX471", jsonMessage);

    jsonBuffer.clear();
}


void loopDS18B20() {
    // DS18B20 Temperature Sensor

    do {
        tempMeter.requestTemperatures();
        temp = tempMeter.getTempCByIndex(0);
        Blynk.virtualWrite(V11, temp);
    } while (temp == 85.0 || temp == (-127.0));

    if (serialActive) {
        Serial.println("===================");
        Serial.println("DS18B20 Temperature Sensor");

        Serial.print("Inside temperature: ");
        Serial.print(temp);
        Serial.println(" °C");
    }


    // Blynk Webhook V0 (string document, int tdsValue ) 1 + 1

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("temperature");
    field1["doubleValue"] = temp;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "DS18B20", jsonMessage);

    jsonBuffer.clear();
}


void loopTDS() {
    // DFRobot Gravity Analog TDS Sensor

    gravityTds.setTemperature(temp);
    gravityTds.update();
    tdsValue = gravityTds.getTdsValue() + 500; // CHEAT - MUST EDIT!!!
    Blynk.virtualWrite(V9, tdsValue);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("DFRobot Gravity Analog TDS Sensor");

        Serial.print("Temperature: ");
        Serial.print(temp);
        Serial.print("  |  TDS: ");
        Serial.println(tdsValue);
    }


    // Blynk Webhook V0 (string document, int tdsValue ) 1 + 1

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("tds");
    field1["doubleValue"] = tdsValue;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "TDS", jsonMessage);

    jsonBuffer.clear();
}


void loopPH() {
    // PH 0-14 Sensor

    float dataArray[BUFFER]; // Make an array of appropriate size for Buffer

    // Fill buffer with initial data
    int n = 0;
    while (n < BUFFER) {

        dataArray[n] = analogRead(ph);
        n++;
    }

    float phMean = meanValue(dataArray);

    float pHVol = (float) phMean * 5.0 / 1024 / 6;
    float phValue = -5.70 * pHVol + calibration; // -5.70 * pHVol + calibration;

    if (serialActive) {
        Serial.println("===================");
        Serial.println("PH 0-14 Sensor");

        Serial.print("PH: ");
        Serial.println(phValue);
    }


    Blynk.virtualWrite(V8, phValue);
    temp = 25; // ????


    // Blynk Webhook V0 (string document, phValue) 1 + 1

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("ph");
    field1["doubleValue"] = phValue;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "PH", jsonMessage);

    jsonBuffer.clear();
}

/*void loopDHT() {
    // DHT11/22 Temperature and Humidity Sensor

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.readTemperature(true);

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }

    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(f, h);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);

    Serial.println("===================");
    Serial.println("DHT11 Temperature and Humidity Sensor");

    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F(" %  |  Temperature: "));
    Serial.print(t);
    Serial.print(F(" °C "));
    Serial.print(f);
    Serial.print(F(" °F  |  Heat index: "));
    Serial.print(hic);
    Serial.print(F(" °C "));
    Serial.print(hif);
    Serial.println(F(" °F"));


    // Blynk Webhook V1 (string document, int humidity, int temperature) 1 + 2

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 5 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("humidity");
    field1["doubleValue"] = h;

    JsonObject &field2 = fields.createNestedObject("temperature");
    field2["doubleValue"] = t;

    JsonObject &field3 = fields.createNestedObject("heatIndex");
    field3["doubleValue"] = hic;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();

    // Blynk.virtualWrite(V1, "DHT", jsonMessage);

    jsonBuffer.clear();
}*/

void loopGY1145() {
    // SI/GY1145 UV/IR/Visible Light Sensor

    float UVindex = uv.readUV(); // the index is multiplied by 100 so to get the integer index, divide by 100
    UVindex /= 100.0;
    float visibleLight = uv.readVisible();
    float IRLight = uv.readIR();

    Blynk.virtualWrite(V3, visibleLight);
    Blynk.virtualWrite(V4, IRLight);
    Blynk.virtualWrite(V5, UVindex);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("SI/GY1145 UV/IR/Visible Light Sensor");

        Serial.print("Visible light: ");
        Serial.print(visibleLight);
        Serial.print(" lux  |  Infrared light: ");
        Serial.print(IRLight);
        Serial.print(" m2/W  |  UV index: ");
        Serial.println(UVindex);
    }

    // Uncomment if you have an IR LED attached to LED pin
    // Serial.print("Prox: "); Serial.println(uv.readProx());


    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 5 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("visibleLight");
    field1["doubleValue"] = visibleLight;

    JsonObject &field2 = fields.createNestedObject("IRLight");
    field2["doubleValue"] = IRLight;

    JsonObject &field3 = fields.createNestedObject("UVindex");
    field3["doubleValue"] = UVindex;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "GY1145", jsonMessage);

    jsonBuffer.clear();

}

void loopBME280() {
    // BME280 Humidity, Temperature & Pressure sensor

    float bmeTemp = bme.readTemperature();
    float bmeHumidity = bme.readHumidity();

    Blynk.virtualWrite(V12, bmeTemp);
    Blynk.virtualWrite(V10, bmeHumidity);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("BME280 Humidity, Temperature & Pressure sensor");

        Serial.print("Temperature: ");
        Serial.print(bmeTemp);
        Serial.print(" °C  |  Humidity: ");
        Serial.print(bmeHumidity);
        Serial.print(" %  |  Pressure: ");
        Serial.print(bme.readPressure());
        Serial.print(" Pa  |  Approx altitude: ");
        Serial.print(bme.readAltitude(1013.25)); // this should be adjusted to your local forecast
        Serial.println(" m");
    }

}

void loopGY302() {
    // GY-302 BH1750 Light Intensity Sensor

    float lightIntensity = lightMeter.readLightLevel();

    Blynk.virtualWrite(V7, lightIntensity);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("GY-302 BH1750 Light Intensity Sensor");


        Serial.print("Light Intensity: ");
        Serial.print(lightIntensity);
        Serial.println(" lux");
    }


    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("lightIntensity");
    field1["doubleValue"] = lightIntensity;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "GY302", jsonMessage);

    jsonBuffer.clear();

}


void loopMQ135() {
    // MQ-135 Gas/Air quality Sensor

    float temperature = 28.0; // assume current temperature. Recommended to measure with DHT22
    float humidity = 25.0; // assume current humidity. Recommended to measure with DHT22

    /*
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }*/

    float rzero = mq135.getRZero();
    float correctedRZero = mq135.getCorrectedRZero(temperature, humidity);
    float resistance = mq135.getResistance();
    float ppm = mq135.getPPM();
    float correctedPPM = mq135.getCorrectedPPM(temperature, humidity);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("MQ-135 Gas/Air quality Sensor");

        Serial.print("RZero: ");
        Serial.print(rzero);
        Serial.print("  |  Corrected RZero: ");
        Serial.print(correctedRZero);
        Serial.print("  |  Resistance: ");
        Serial.println(resistance);
        Serial.print("PPM: ");
        Serial.print(ppm);
        Serial.print("  |  Corrected PPM: ");
        Serial.print(correctedPPM);
        Serial.print(" ppm @ temp/hum: ");
        Serial.print(temperature);
        Serial.print("/");
        Serial.print(humidity);
        Serial.println("%");
    }


    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("correctedPPM");
    field1["doubleValue"] = correctedPPM;

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    /*Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();*/

    // Blynk.virtualWrite(V1, "MQ135", jsonMessage);

    jsonBuffer.clear();


/*    float dataArray[BUFFER]; // Make an array of appropriate size for Buffer

    // Fill buffer with initial data
    int n = 0;
    while (n < BUFFER) {
        dataArray[n] = mq135.getPPM();
        Serial.println(dataArray[n]); // DELETE
        n++;
    }

    float phMean = meanValue(dataArray);*/


}


void loopMAX7219() {
    // LED Matrix Indicator MAX7219 (TDS/Water Level)

    if (serialActive) {
        Serial.println("===================");
        Serial.println("Matrix Indicator MAX7219 – Looping through!"); // DEBUG
    }

    // float waterLevelDisplay = waterLevel / 11.5;
    float currentLevel1 = (waterLevel / 30) / segmentsPercentage;
    int levelNumber1 = roundf(currentLevel1);

    float currentLevel2 = (tdsValue / 1000) / segmentsPercentage;
    int levelNumber2 = roundf(currentLevel2);

    int progress1 = 1;
    int progress2 = 1;

    byte display1[screens];
    byte display2[screens];

    for (int i = 0; screens > i; i++) {

        display1[i] = 0;
        display2[i] = 0;

        for (int j = 1; j <= dots; j++) {

            if (progress1 <= levelNumber1) {
                display1[i] += (unsigned(1) << (j - 1));
                progress1++;
            } else {
                display1[i] += (unsigned(0) << (j - 1));
            }

            if (progress2 <= levelNumber2) {
                display2[i] += (unsigned(1) << (j - 1));
                progress2++;
            } else {
                display2[i] += (unsigned(0) << (j - 1));
            }

        }
    }

    int dataLc[screens][dots];

    for (int i = 0; i < screens; i++) {

        for (int j = 0; j < dots; j++) {

            if (j == 1 || j == 2) {
                dataLc[i][j] = {display1[i]};
            } else if (j == 5 || j == 6) {
                dataLc[i][j] = {display2[i]};
            } else {
                dataLc[i][j] = {0};
            }
            lc.setRow(i, j, dataLc[i][j]);
        }
    }
}


void loopMPS20N0040D() {
    // MPS20N0040D-D Pressure Sensor 0-40kPa

    long executionStart = millis();

    float base = 5444444;
    float raw = scale.read_average(10);

    waterLevel = (raw - base) / 10000;

    Blynk.virtualWrite(V25, waterLevel);

    // Serial.print("Units:\t");
    // Serial.print(scale.get_units(10), 1);

    if (serialActive) {
        Serial.println("===================");
        Serial.println("MPS20N0040D-D Pressure Sensor 0-40kPa");

        Serial.print("Raw: ");
        Serial.print(raw, 1);
        Serial.print("  |  Final: ");
        Serial.print(waterLevel, 1);
        Serial.print("  |  Execution: ");
        Serial.print(millis() - executionStart);
        Serial.println(" ms");

        terminal.print("MPS20N0040D-D: ");
        terminal.print(millis() - executionStart);
        terminal.println(" ms");
        terminal.flush();
    }


    // Serial.print("SCK: ");
    // Serial.println(analogRead(A0));

    // Serial.print("OUT: ");
    // Serial.println(analogRead(A1));

    // Arduino JSON
    String timeStamp = returnClock();

    // Allocate JsonBuffer
    const size_t capacity = 6 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 100;
    // DynamicJsonBuffer jsonBuffer(capacity);
    StaticJsonBuffer<capacity> jsonBuffer;

    // Create JsonObject
    JsonObject &jb = jsonBuffer.createObject();

    // Construct JSON body
    JsonObject &fields = jb.createNestedObject("fields");

    JsonObject &field1 = fields.createNestedObject("base");
    field1["doubleValue"] = base;

    JsonObject &field2 = fields.createNestedObject("raw");
    field2["doubleValue"] = raw;

    JsonObject &field3 = fields.createNestedObject("waterLevel");
    field3["doubleValue"] = waterLevel;

    JsonObject &field4 = fields.createNestedObject("executionTime");
    field4["doubleValue"] = (millis() - executionStart);

    JsonObject &field_timeCreated = fields.createNestedObject("timeCreated");
    field_timeCreated["stringValue"] = timeStamp;

    String jsonMessage;
    jb.printTo(jsonMessage);

    // DEBUG
    Serial.println();
    jb.prettyPrintTo(Serial);
    Serial.println();

    Blynk.virtualWrite(V1, "MPS20N0040D", jsonMessage);

    jsonBuffer.clear();
}

void loopLight() {

    if (lightActive) {

        int currentHour = hour();

        Blynk.syncVirtual(V22);

        if (manualControl == 0) {

            if (serialActive) {
                Serial.println("===================");
                Serial.println("Manual control is OFF.");
                Serial.print("24-hour time is: ");
                Serial.println(currentHour);
            }

            if (currentHour > 12) {
                currentHour = currentHour - 12;

                if (serialActive) {
                    Serial.print("12-hour time is: ");
                    Serial.println(currentHour);
                }


                if (currentHour >= 7) {
                    analogWrite(2, 100);    // BLUE/RED LED
                    analogWrite(3, 75);     // FULL SPECTRUM LED
                    analogWrite(4, 30);     // GREEN LED
                    analogWrite(5, 75);     // WARM WHITE LED
                }
            }

            if (hour() <= 6) {
                analogWrite(2, 2);
                analogWrite(3, 1);
                analogWrite(4, 1);
                analogWrite(5, 2);
            }
        } else {

            Blynk.syncVirtual(V13, V14, V15, V16);

        }
    } else {
        digitalWrite(2, LOW);
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(5, LOW);
    }
}

void(* resetFunc) (void) = 0; // Reset Arduino


void loopLifeCountdown() {
    Serial.print(lifeCountdown);
    Serial.println(" minute(s) left until the next LIFE cycle.");

    terminal.print(lifeCountdown);
    terminal.println(" minute(s) left until the next LIFE cycle.");
    terminal.flush();
    lifeCountdown = --lifeCountdown;
};

void loopFinishLife() {
    Serial.println("Ending LIFE cycle.");
    terminal.println("Ending LIFE cycle.");
    terminal.flush();

    lifeCountdown = 5;
    timerLoopLifeCountdown = timer.setInterval(1 * 60 * 1000L, loopLifeCountdown);
    loopLifeCountdown();

    digitalWrite(33, LOW); // JUST FOR FUN = to make the relay clicking sound
    digitalWrite(37, LOW); // --- //
    digitalWrite(8, LOW);
    // digitalWrite(45, LOW); // TOP MISTER
    digitalWrite(7, LOW);
}

void loopStartLife() {
    Serial.println("Starting LIFE cycle.");
    terminal.println("Starting LIFE cycle.");
    terminal.flush();

    timer.deleteTimer(timerLoopLifeCountdown);

    digitalWrite(33, HIGH);
    digitalWrite(37, HIGH);
    digitalWrite(8, HIGH);
    // digitalWrite(45, HIGH); // TOP MISTER
    analogWrite(7, 30);
}

void loopLife() {
    timer.setTimeout(5 * 1000L, loopStartLife);
    timer.setTimeout(20 * 1000L, loopFinishLife);
}



void finishLoopPump() {
    analogWrite(9, 0);
}

void loopPump() {
    // if (loopPumpActive) {
    analogWrite(9, map(20, 0, 50, 0, 255));
    timer.setTimeout(3 * 1000L, finishLoopPump);
    timer.setTimeout(2 * 60 * 1000L, resetFunc);
}

void finishLoopMister() {
    digitalWrite(45, LOW);

    /*Serial.println("Ending PUMP-MISTER cycle.");
    terminal.println("Ending PUMP-MISTER cycle.");
    terminal.flush();*/

    if (cyclePumpMister == 20) {
        analogWrite(7, 0); // FAN
    } else {
        analogWrite(7, 40); // FAN
    }
}

void loopMister() {
    analogWrite(7, 0); // FAN
    digitalWrite(45, HIGH);
    timer.setTimeout(4 * 1000L, finishLoopMister);

    // Serial.println(cyclePumpMister);

    double pumpOn = cyclePumpMister / 10;

    /*if (pumpOn == 3.0) { // || pumpOn == 2.0 || pumpOn == 1.0
        timer.setTimeout(1 * 1000L, loopPump);
    }*/

    cyclePumpMister--;

    if (cyclePumpMister == 0) {
        cyclePumpMister = 20;
    }
}

void loopPumpMister() {
    int currentHour = hour();
    // Serial.println(currentHour);

    if (currentHour < 22 && currentHour > 1) {
        timer.setTimer(10 * 1000L, loopMister, cyclePumpMister);
    }

    // timer.setTimer(6 * 1000L, finishLoopPumpMister, cyclePumpMister);

    /*Serial.println("Starting PUMP-MISTER cycle.");
    terminal.println("Starting PUMP-MISTER cycle.");
    terminal.flush();*/
}

int currentHour = hour();


void loops() {
    // Blynk timers

    timer.setTimeout(0, loopMPS20N0040D); // Atmospheric pressure level sensor
    timer.setTimeout(5000L, loopMAX471); // Voltage/Current Sensor

    if (loopActive) {

        // DETECTIVE CASE – I suspect that having I2C components which are not properly identified stops the Arduino ???
        // AND I2C components are not identified – might be due to improper physical contacts

        // Lower part
        /* 1. */  timer.setTimeout(0, loopDS18B20);
        /* 2. */  timer.setTimeout(1000L, loopTDS);
        /* 3. */  timer.setTimeout(2000L, loopPH);
        /* 4. */  timer.setTimeout(3000L, loopMAX471);
        /* 5. */  timer.setTimeout(4000L, loopMQ135);
        /* 6. */  timer.setTimeout(5000L, loopMPS20N0040D);

        // Upper part
        /* 7. */ timer.setTimeout(6000L, loopMAX7219); // LED Matrix

        if (i2cActive) {
            /* 8. */ timer.setTimeout(7000L, loopBME280);
            /* 9. */ timer.setTimeout(8000L, loopGY302);
            /* 10. */ timer.setTimeout(9000L, loopGY1145);
        }

    }

    // Unused
    // timer.setInterval(5000L, loopDHT);
    // timer.setInterval(5000L, loopHCSR04);
    // timer.setInterval(2500L, wifiSignal);
};


/********************************* Arduino Setup ***************************************/
/***************************************************************************************/

void setup() {

    // Initialize communication
    Serial.begin(115200); // Debug console
    Serial.println("Hello, serial began.");
    delay(10);
    EspSerial.begin(ESP8266_BAUD);  // Set ESP8266 baud rate
    delay(10);


    // Initialize peripherals
    if (i2cActive) {
        Wire.begin();
        i2cScanner();

        if (!bme.begin(BME280_ADDRESS)) {
            Serial.println("BME280 sensor was not found.");
        }

        if (!lightMeter.begin()) {
            Serial.println("GY302 sensor was not found.");
        }

        if (!uv.begin()) {
            Serial.println("GY/SI1145 sensor was not found.");
        }
    }

   /* tempMeter.begin();
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); // MPS20N0040D
    // dht.begin();

    setLc(); // MAX7219

    // TDS
    gravityTds.setPin(TdsSensorPin);
    gravityTds.setAref(5.0);
    gravityTds.setAdcRange(1024);
    gravityTds.begin();*/

    setSyncInterval(5 * 60); // Syncing interval for the clock = 5 minutes

    // Initialize pins
    // pinMode(lightOne, OUTPUT);
    // pinMode(lightTwo, OUTPUT);
    // pinMode(levelTrg, OUTPUT);
    // pinMode(levelEcho, INPUT);
    pinMode(7, OUTPUT);
    pinMode(8, OUTPUT);
    pinMode(9, OUTPUT);
    pinMode(33, OUTPUT);
    pinMode(37, OUTPUT);
    pinMode(45, OUTPUT);

    if (lightActive) {
        loopLight();
    }


    // Initialize Blynk
    if (isblynkAllowed) {
        Blynk.begin(auth, wifi, ssid, pass);

        // Blynk.config(auth, server, port);
        // while (!Blynk.connect());
        // Blynk.connect();

        // while (Blynk.connect() == false) {  }   // Wait until connected

        // Blynk.syncAll();

        /*if (millis() < 50000) {  // if Blynk.begin(auth) finished under 50 seconds == connected to network
            //Serial.println("Connected to the local network.");

            tryConnecting();                          // try to connect to blynk server
            tryConnecting();                          // I put this twice, because very rarely it fails the first connection

            timer.setInterval(60000, tryConnecting);  // if lost connection, try to connect time by time

            //Serial.println("Reconnect attempts to the Blynk server allowed in the main loop.");
        }
        else {   // if Blynk.begin(auth) took more than 50 seconds == no network connection
            // isblynkAllowed = false;
            //Serial.println("No network access. No reconnect attempts will be allowed in main loop.");
        }*/

        Serial.print("Blynk v.");
        Serial.print(BLYNK_VERSION);
        Serial.println(": Device started");

        timer.setInterval(20 * 1000L, reconnectBlynk);  // Check every 10 seconds if still connected to server
        timer.setInterval(5 * 1000L, displayClock);
    }


    terminal.clear();
    terminal.println("Hello, serial terminal began.");


    // timer.setInterval(10 * 60 * 1000L, loops);

    // timer.setInterval(5 * 1000L, loopLight);

    // timer.setInterval(5 * 60 * 1000L, loopLife); // Every 5 minutes inject life ;-)

    loopPumpMister();

    timer.setInterval(30 * 60 * 1000L, loopPumpMister);

    if (loopPumpActive) {
        timer.setInterval(720 * 60 * 1000L, loopPump);
    }

    // timerLoopLifeCountdown = timer.setInterval(1 * 60 * 1000L, loopLifeCountdown);
    // loopLifeCountdown();
    // loopMAX7219();

    // TODO Change communication baud rate between Arduino <--> ESP8266 to 9600 instead of 115200 ! i.e. firmware AT command 'AT+CIOBAUD=9600'
    // TODO Wifi settings through EEPROM

}


/******************************** Arduino loop *****************************************/
/***************************************************************************************/

void loop() {
    /*if (WiFi.status() == WL_CONNECTED) {
        Blynk.run();
    } else {
        reconnectBlynk();
    }*/

    checkBlynk();
    timer.run();

    /*if (Blynk.connected()) {
        Blynk.run();
    } else {
        reconnectBlynk();
    }*/

    // Blynk.run();
}


float meanValueOld(int pinNumber) {
    int n = 1; // loop variable
    float total = analogRead(pinNumber); // Running total for calculation of mean & baseline reading
    float mean = total; // mean of all current data

    while (n < SAMPLES) {
        float sample = analogRead(pinNumber);
        if ((sample < (1 + TOLERANCE) * mean) and (sample > (1 - TOLERANCE) * mean)) {
            total = total + sample; // if new reading is within range, calculate new mean
            n = n + 1;
            mean = total / n;
        }
        // (else ignore new reading)
    }
    return mean;
}