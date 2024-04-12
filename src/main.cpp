#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans12pt7b.h>

#include "httpsCert.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SS_PIN 25
#define RST_PIN 2

#define SERVER_IP "rfid-server.vercel.app"

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


String printHex(byte* buffer, byte bufferSize);
void printDec(byte* buffer, byte bufferSize);
bool postGottenFingerprintId(String uid, String url);
void screenPrint(String text);

const char* ssid = "Okaeri";
const char* password = "cocktail";
const String check_url = "https://rfid-next-kappa.vercel.app/api/bikeParking";
//const String check_url = "https://rfid-next-kappa.vercel.app/api/attendance";
const String register_url = "https://rfid-next-kappa.vercel.app/api/updateRFID";

// const char* ssid = "FPTU_Student";
// const char* password = "12345678";

MFRC522 rfid(SS_PIN, RST_PIN);  // Instance of the class

MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[4];

void setup() {
  Serial.begin(115200);
  SPI.begin();      // Init SPI bus
  rfid.PCD_Init();  // Init MFRC522

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.setFont(&FreeSans12pt7b);

  //connect wifi
  WiFi.mode(WIFI_STA);  //Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting to wifi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    screenPrint("connecting");
    delay(500);
  }
  screenPrint("connected!");

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  Serial.print(F("Using the following key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);

  screenPrint("Scanning");
}

void loop() {

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || rfid.uid.uidByte[1] != nuidPICC[1] || rfid.uid.uidByte[2] != nuidPICC[2] || rfid.uid.uidByte[3] != nuidPICC[3]) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }

    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));

    String NUID = printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println(NUID);
    
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    
    if (!postGottenFingerprintId(NUID, check_url)) {
      postGottenFingerprintId(NUID, register_url);
    }
  } else {
    Serial.println(F("Card read previously."));
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    String NUID = printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println(NUID);
    
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();

    if (!postGottenFingerprintId(NUID, check_url)) {
      postGottenFingerprintId(NUID, register_url);
    }
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void screenPrint(String text) {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  // Display static text
  display.println(text);
  display.display(); 
}


/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
String printHex(byte* buffer, byte bufferSize) {
  String result = ""; // create an empty string
  for (byte i = 0; i < bufferSize; i++) {
    // append the characters to the string
    result += buffer[i] < 0x10 ? " 0" : " ";
    result += String(buffer[i], HEX);
  }
  return result; // return the string
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

bool postGottenFingerprintId(String uid, String url) {
  if ((WiFi.status() == WL_CONNECTED)) {

    WiFiClientSecure *client = new WiFiClientSecure;
    if (!client) {
      Serial.println("Unable to create wifi client.");
      return true;
    }
    client->setCACert(rootCACertificate);
    HTTPClient https;

    https.begin(*client, url);  // HTTP
    https.addHeader("Content-Type", "application/json");

    Serial.print("Sending to ");
    Serial.println(url);

    String json = "{\"uid\":\"" + uid + "\"}";
    Serial.println(json);
    int httpCode = https.POST(json);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == 201) {
        Serial.println("HTTP Success.");
        const String& payload = https.getString();
        Serial.println(payload);

        JsonDocument doc;
        deserializeJson(doc, payload);

        if (doc["message"] == "Not found") {
          Serial.println("Card not found");
          return false;
        } else {
          screenPrint(doc["insertData"]["studentId"]);
        }
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
    return true;
  }
  return true;
}

