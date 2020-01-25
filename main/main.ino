/*

  TBZ HF - IG1
  ============

  Version: 21.01.2020

  Autoren:
  - Senti Laurin <laurin.senti@edu.tbz.ch>
  - Severin Steiner <severin.steiner@edu.tbz.ch>
  - Markus Ritzmann <markus.ritzmann@edu.tbz.ch>

  Im Unterricht von Christoph Jäger.

  Abhängigkeiten
  ==============

  Installieren via "Tools -> Manage Libraries"

  Für den Sensor:
  - Adafruit BME680 Library by Adafruit
  - Adafruit Unified Sensor by Adafruit

  Für das WLAN:
  - WiFiNNA by Arduino

  Für den TFT Display:
  - TFT Built-in by Arduino, Adafruit
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library

*/

#include <Wire.h>
#include <SPI.h>

// TFT: Libraries
#include <Adafruit_GFX.h>    // Adafruit Grafik-Bibliothek wird benötigt "Adafruit GFX Library"
#include <Adafruit_ST7735.h> // Adafruit ST7735-Bibliothek wird benötigt "Adafruit ST7735 and ST7789 Library"

// Sensor: Libraries
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

//WiFi: Libraries
#include <WiFiNINA.h>
#include "arduino_secrets.h"

// TFT: Variablen
#define TFT_PIN_CS 0         // Arduino-Pin an Display CS
#define TFT_PIN_DC 6         // Arduino-Pin an
#define TFT_PIN_RST 1        // Arduino Reset-Pin

// Sensor: Variablen
#define BME_SCK 9
#define BME_MISO 10
#define BME_MOSI 8
#define BME_CS 7

// Standard Wert aus Dokumentation war 1013.25. Anhand https://kachelmannwetter.com/ch/messwerte/zuerich/luftdruck-qnh/20200119-1500z.html
// and die Schweiz (Zürich) angepasst. Muss anhand Wetter und Ort angepasst werden.
#define SEALEVELPRESSURE_HPA (1036.5)

// TFT: Einstellungen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

// Sensor: Einstellugen
Adafruit_BME680 bme(BME_CS); // hardware SPI

//WiFi Einstellungen
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the Wifi radio's status

WiFiClient client; //For HTTP requests to send data to influxDB
char buf[influxdbBufferSize] = {'\0'};
const String strInfluxDbServer = influxdbServer; //Convert to String
const String strInfluxDbPort = String(influxdbPort); //Convert to String

//Define globally used variables outside of scope
float floatTemperature;
float floatPressure;
float floatHumidity;
float floatGas;
float floatAltitude;

void setup() {

  // Sets the Digital Pin as output
  pinMode(2, OUTPUT); // Tongeber
  pinMode(3, OUTPUT); // LED Rot
  pinMode(4, OUTPUT); // LED Gelb
  pinMode(5, OUTPUT); // LED Grün

  // TFT
  tft.initR(INITR_GREENTAB); // Initialise TFT library
  tft.fillScreen(ST7735_BLACK); // Fill display with black

  // Sensor
  Serial.begin(9600);

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  setWifi(); //Call to function to set up WiFi connection
}

//Variables outside of loop
String line;

void loop() {
  setSensorData();
  setDisplay();

  // =========
  // Sensor
  // =========



  Serial.println();
  delay(3000);
  //printCurrentNet();
  setData();
}

// =========
// Void functions
// =========

// Status OK
float statusOk() {
  digitalWrite(5, HIGH);
}

// Status Warnung
float statusWarning() {
  digitalWrite(4, HIGH);
}

// Status Kritisch
float statusCritical() {
  digitalWrite(3, HIGH); // LED
  digitalWrite(2, HIGH); // Tongeber
}

// Setzt Ausgaben zurück
float statusReset() {
  digitalWrite(5, LOW); // LED Grün
  digitalWrite(4, LOW); // LED Gelb
  digitalWrite(3, LOW); // LED Rot
  digitalWrite(2, LOW); // Tongeber
}

void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void sendData(char* data, int influxdbBufferSize) {
  if (client.connect(influxdbServer, influxdbPort)) { //if connected to server
    //Print the buffer on the serial line to see how it looks
    Serial.print("Sending following dataset to InfluxDB: ");
    Serial.println(buf);
    client.println("POST /api/v2/write?org=" + influxdbOrgId + "&bucket=" + influxdbBucketName + "&precision=s HTTP/1.1");
    client.println("Host: " + strInfluxDbServer + ":" + strInfluxDbPort + "");
    client.println("Authorization: Token " + influxdbAuthToken);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(influxdbBufferSize);
    client.println();
    client.println(data);
    client.stop();
  }
  else {
    Serial.println("Connection to server failed");
  }
}

void setData() {
  //this will be number of chars written to buffer, return value of sprintf
  int numChars = 0;
  //First of all we need to add the name of measurement to beginning of the buffer
  numChars = sprintf(buf, "sensor,");

  //tag should have an space at the end
  numChars += sprintf(&buf[numChars], "sensor-id=1 ");

  numChars += sprintf(&buf[numChars], "temperature=%.2f,", getTemperature());
  numChars += sprintf(&buf[numChars], "pressure=%.2f,", getPressure());
  numChars += sprintf(&buf[numChars], "humidity=%.2f,", getHumidity());
  numChars += sprintf(&buf[numChars], "gas=%.2f,", getGas());
  numChars += sprintf(&buf[numChars], "altitude=%.2f", getAltitude());

  //send to InfluxDB
  sendData(buf, numChars);

  //we have to reset the buffer at the end of loop
  memset(buf, '\0', influxdbBufferSize);
}

void setWifi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // retry to connect every 5 seconds:
    delay(5000);
  }
  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  //printCurrentNet();
  //printWifiData();
}

void setDisplay() {
  // =========
  // TFT
  // =========


  tft.fillScreen(ST7735_BLACK); // Fill display with black to not have strange behaviour when overwriting displayed text

  String Line1  = "Temperatur:";
  String Line2  = String(getTemperature()) + "*C";

  String Line3  = "Luftdruck:";
  String Line4  = String(getPressure()) + "hPa";

  String Line5  = "Luftfeuchtigkeit:";
  String Line6  = String(getHumidity()) + "%";

  String Line7  = "Gas:";
  String Line8  = String(getGas()) + "KOhm";

  String Line9  = "Hoehe:";
  String Line10 = String(getAltitude()) + "m";

  // Gibt Text aus
  tft.setCursor(7, 0);            // Setze Position
  tft.setTextSize(1);             // Schriftgröße einstellen
  tft.print(Line1);               // Text ausgeben

  tft.setCursor(7, 10);
  tft.setTextSize(2);
  tft.print(Line2);


  tft.setCursor(7, 30);
  tft.setTextSize(1);
  tft.print(Line3);

  tft.setCursor(7, 40);
  tft.setTextSize(2);
  tft.print(Line4);


  tft.setCursor(7, 60);
  tft.setTextSize(1);
  tft.print(Line5);

  tft.setCursor(7, 70);
  tft.setTextSize(2);
  tft.print(Line6);


  tft.setCursor(7, 90);
  tft.setTextSize(1);
  tft.print(Line7);

  tft.setCursor(7, 100);
  tft.setTextSize(2);
  tft.print(Line8);


  tft.setCursor(7, 120);
  tft.setTextSize(1);
  tft.print(Line9);

  tft.setCursor(7, 130);
  tft.setTextSize(2);
  tft.print(Line10);
}

void setSensorData() {
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading on sensor:(");
    return;
  }
  setTemperature();
  setPressure();
  setHumidity();
  setGas();
  setAltitude();
}

void setTemperature() {
  floatTemperature = bme.temperature;
}

void setPressure() {
  floatPressure = bme.pressure / 100.0; // in hPa
}

void setHumidity() {
  floatHumidity = bme.humidity;
}

void setGas() {
  floatGas = bme.gas_resistance / 1000.0; // in kOhm
}

void setAltitude() {
  floatAltitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

// =========
// Float functions
// =========

float getTemperature() {
  return floatTemperature;
}

float getPressure() {
  return floatPressure;
}

float getHumidity() {
  return floatHumidity;
}

float getGas() {
  return floatGas;
}

float getAltitude() {
  return floatAltitude;
}
