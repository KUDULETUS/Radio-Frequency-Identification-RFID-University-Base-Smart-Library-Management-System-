#include <LiquidCrystal.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <HardwareSerial.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <base64.h>

// Wi-Fi Credentials
const char* ssid = "Prime";
const char* password = "12345678910";

// Server URLs
const char* serverUrl = "http://192.168.119.23/test/borrow_return3.php";
const char* getPhoneUrl = "http://192.168.119.23/test/get_phone.php"; // URL to get user phone number
const char* addUidUrl = "http://192.168.119.23/test/add_uid.php"; // URL to add UID to the database

// Twilio credentials and phone numbers
const String account_sid = "AC93e8019292b9e6e9222a4e12e5b223c7";
const String auth_token = "72b1e2b90782c43812d3d58c15b1a553";
const String from_number = "+12568297961";  // Twilio phone number

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 3600000); // Update every hour

// LCD Display
LiquidCrystal_I2C lcd(0x27, 20, 4);

// RFID Pin Definitions
#define SS_PIN 5
#define RST_PIN 4

// Servo Motor Pins
#define SECTION1_SERVO_PIN 13
#define SECTION2_SERVO_PIN 12

// Button Pin
#define BUTTON_PIN 14 // GPIO pin for the button

// Create an instance of the RFID reader
MFRC522 rfid(SS_PIN, RST_PIN);

// Create Servo instances
Servo section1Servo;
Servo section2Servo;

void setup() {
    // Initialize LCD
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Place your RFID card");
    lcd.setCursor(0, 1);
    lcd.print("to the reader...");

    // Start Serial communication for debugging
    Serial.begin(115200);

    // Initialize SPI bus and RFID reader
    SPI.begin();
    rfid.PCD_Init();
    delay(1000);
    Serial.println("Place your RFID card to the reader...");

    // Initialize servos
    section1Servo.attach(SECTION1_SERVO_PIN);
    section2Servo.attach(SECTION2_SERVO_PIN);
    delay(500); // Allow servos to stabilize
    section1Servo.write(0); // Close position
    section2Servo.write(0); // Close position

    // Configure button pin as input with internal pull-up resistor
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Connect to Wi-Fi
    connectToWiFi();

    // Initialize NTP Client
    timeClient.begin();
    updateNTPTime();
}

void loop() {
    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW) { // Button pressed
        Serial.println("Button pressed: Running Register UID Functionality");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Register UID Function");
        runRegisterUIDFunctionality();
    } else { // Button not pressed
        Serial.println("Button not pressed: Running Library Functionality");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" scan card to Borrow or Return");
        runLibraryFunctionality();
    }
}

void runLibraryFunctionality() {
    // Check if an RFID card is detected
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        delay(500);
        return; // If no card, continue looping
    }

    // Convert UID to string
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("Card UID: " + uid);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card UID: " + uid);

    // Assume first scan is user card, second scan is book
    String userUID = uid;
    String bookUID = "";

    // Wait for a second RFID scan (book)
    Serial.println("Please scan the book...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please scan the book...");
    unsigned long startMillis = millis();
    while (millis() - startMillis < 10000) { // Timeout after 10 seconds
        if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
            // Get book UID
            bookUID = "";
            for (byte i = 0; i < rfid.uid.size; i++) {
                bookUID += String(rfid.uid.uidByte[i], HEX);
            }
            bookUID.toUpperCase();
            Serial.println("Book UID: " + bookUID);
            break;
        }
        delay(500);
    }

    if (bookUID == "") {
        Serial.println("Book scan timed out.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Book scan timed out.");
        return;
    }

    // Send user and book UIDs to the server
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String httpRequestData = "user_uid=" + userUID + "&book_uid=" + bookUID;
        int httpResponseCode = http.POST(httpRequestData);
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Server Response: " + response);

            // Control the servos based on the server response
            if (response.indexOf("Return Success") != -1) {
                int section = response.substring(response.lastIndexOf(" ")+1).toInt();
                Serial.println("Section: " + String(section));
                if (section == 1) {
                    Serial.println("Returning book to Section 1");
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Returning to Sec 1");
                    section1Servo.write(90); // Open position
                    delay(5000);             // Wait 5 seconds for user to access
                    section1Servo.write(0);  // Close position
                    Serial.println("Section 1 Servo closed");
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Section 1 closed");
                } else if (section == 2) {
                    Serial.println("Returning book to Section 2");
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Returning to Sec 2");
                    section2Servo.write(90); // Open position
                    delay(5000);             // Wait 5 seconds for user to access
                    section2Servo.write(0);  // Close position
                    Serial.println("Section 2 Servo closed");
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Section 2 closed");
                }

                // Get user phone number from the server
                String userPhoneNumber = getUserPhoneNumber(userUID);
                if (userPhoneNumber != "") {
                    // Send SMS via Twilio for return confirmation
                    sendTwilioSMS(userPhoneNumber, "Book returned successfully to section " + String(section) + ". Thank you for using our library service.");
                }
            } else if (response.indexOf("Borrow Success") != -1) {
                Serial.println("Borrow Success");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Borrow Success");
                lcd.setCursor(0, 1);
                lcd.print("Return by: " + calculateReturnDate());

                // Get user phone number from the server
                String userPhoneNumber = getUserPhoneNumber(userUID);
                if (userPhoneNumber != "") {
                    // Send SMS via Twilio for borrow confirmation
                    sendTwilioSMS(userPhoneNumber, "Book borrowed successfully. Please return it by " + calculateReturnDate() + ". Thank you for using our library service.");
                }
            } else {
                Serial.println("Unexpected response: " + response);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(" " + response);
            }
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected, reconnecting...");
        connectToWiFi();
    }

    // Halt for a while before scanning the next card
    delay(5000);
}

void runRegisterUIDFunctionality() {
    // Check if an RFID card is detected
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        delay(500);
        return; // If no card, continue looping
    }

    // Convert UID to string
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("Registering UID: " + uid);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Registering UID...");
    lcd.setCursor(0, 1);
    lcd.print(uid);

    // Send UID to the server to add it to the uid_table
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(addUidUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String httpRequestData = "uid=" + uid;
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Server Response: " + response);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("UID Registered");
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Registration Error");
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected, reconnecting...");
        connectToWiFi();
    }

    delay(5000);
}

String calculateReturnDate() {
    // Calculate the return date (14 days from now)
    int borrowDays = 14;
    updateNTPTime();
    unsigned long now = timeClient.getEpochTime();
    now += borrowDays * 86400; // Add 14 days in seconds
    time_t futureTime = (time_t)now;
    struct tm *tm = gmtime(&futureTime);
    char returnDateBuffer[20];
    strftime(returnDateBuffer, sizeof(returnDateBuffer), "%Y-%m-%d", tm);
    return String(returnDateBuffer);
}

void sendTwilioSMS(String to, String message) {
    if ((WiFi.status() == WL_CONNECTED)) {
        HTTPClient http;
        String twilioUrl = "https://api.twilio.com/2010-04-01/Accounts/" + account_sid + "/Messages.json";
        http.begin(twilioUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("Authorization", "Basic " + base64::encode((account_sid + ":" + auth_token).c_str()));

        String postData = "To=" + to + "&From=" + from_number + "&Body=" + message;

        int httpResponseCode = http.POST(postData);
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Twilio Response code: " + String(httpResponseCode));
            Serial.println("Twilio Response: " + response);
        } else {
            Serial.print("Error on sending SMS via Twilio: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected, reconnecting...");
        connectToWiFi();
    }
}

String getUserPhoneNumber(String userUID) {
    if ((WiFi.status() == WL_CONNECTED)) {
        HTTPClient http;
        http.begin(getPhoneUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String httpRequestData = "user_uid=" + userUID;
        int httpResponseCode = http.POST(httpRequestData);

        if (httpResponseCode > 0) {
            String phoneNumber = http.getString();
            Serial.println("User Phone Number: " + phoneNumber);
            return phoneNumber;
        } else {
            Serial.print("Error on getting phone number: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected, reconnecting...");
        connectToWiFi();
    }
    return "";
}

void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
}

void updateNTPTime() {
    int retryCount = 0;
    while (!timeClient.update() && retryCount < 5) {
        Serial.println("NTP update failed, retrying...");
        timeClient.forceUpdate();
        delay(500);
        retryCount++;
    }
}
