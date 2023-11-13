// Necessary libraries.
#include <Wire.h>
// ESP8266.h library provides ESP8266 a specific Wi-Fi routine that we are calling to connect to the network.
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define API_KEY "AIzaSyBJckN6aQCxJCzwhFTeEdCL3NiH3HK2Ays"
// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://falldetection-9430d-default-rtdb.asia-southeast1.firebasedatabase.app"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Define NTP Client to get time
// The WiFi UDP Send block sends data to a UDP host over a wireless network. 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const int MPU_addr = 0x68; // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean fall = false;     // stores if a fall has occurred
boolean trigger1 = false; // stores if first trigger (lower threshold) has occurred
boolean trigger2 = false; // stores if second trigger (upper threshold) has occurred
boolean trigger3 = false; // stores if third trigger (orientation change) has occurred
byte trigger1count = 0;   // stores the counts past since trigger 1 was set true
byte trigger2count = 0;   // stores the counts past since trigger 2 was set true
byte trigger3count = 0;   // stores the counts past since trigger 3 was set true
int angleChange = 0;

// WiFi network info.
const char *ssid = "realme 8";        // Enter your Wi-Fi Name
const char *pass = "johnwick"; // Enter your Wi-Fi Password

unsigned long sendDataPrevMillis = 0;

// Board details.
String phoneNumber = "9587800611";  

String customerId = "001";

bool signupOK = false;
int status = WL_IDLE_STATUS;
String currentDate="";
String formattedTime ="";

void setup()
{
    Serial.begin(115200);
    Wire.begin(); 
//   This function initializes the Wire library and join the I2C bus as a controller or a peripheral. 
//   This function should normally be called only once.
//   address: the 7-bit slave address (optional); if not specified, join the bus as a controller device.
    Wire.beginTransmission(MPU_addr);
//   This function begins a transmission to the I2C peripheral device with the given address. Subsequently,
//   queue bytes for transmission with the write() function and transmit them by calling endTransmission().
//   address: the 7-bit address of the device to transmit to.
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0);    // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
//    If true, endTransmission() sends a stop message after transmission, releasing the I2C bus.
//    If false, endTransmission() sends a restart message after transmission. 
//    stop: true or false. True will send a stop message, releasing the bus after transmission. 
//    False will send a restart, keeping the connection active.
    
    Serial.println("Wrote to IMU");
    Serial.println("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print("."); // print ... till not connected
    }
    Serial.println("");
    Serial.println("WiFi connected");

    // Initialize a NTPClient to get time
    timeClient.begin();
    // Set offset time in seconds to adjust for your timezone, for example:
    // GMT +5:30 = 19800
    timeClient.setTimeOffset(19800);
    
    //   part of firebase
    config.api_key = API_KEY;

    /* Assign the RTDB URL (required) */
    config.database_url = DATABASE_URL;
    /* Sign up */
    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("ok");
        signupOK = true;
    }
    else
    {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

void loop()
{
    mpu_read();
    ax = (AcX - 2050) / 16384.00;
    ay = (AcY - 77) / 16384.00;
    az = (AcZ - 1947) / 16384.00;
    gx = (GyX + 270) / 131.07;
    gy = (GyY - 351) / 131.07;
    gz = (GyZ + 136) / 131.07;
    // calculating Amplitute vactor for 3 axis
    float Raw_Amp = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
    int Amp = Raw_Amp * 10; // Mulitiplied by 10 bcz values are between 0 to 1
    Serial.println(Amp);
    
//    This program first checks if the accelerometer values exceed the lower threshold, 
//    if yes, then it waits for 0.5 seconds and checks for the higher threshold. 
//    If the accelerometer values exceed the higher threshold, 
//    then it checks for the gyroscope values to calculate the change in orientation. 
//    Now, If there is a sudden change in orientation, 
//    then it waits for 10 seconds and checks if the orientation remains the same. 
//    If yes, then it activates the Fall Detector alarm.
    if (Amp <= 2 && trigger2 == false)
    {
        // if AM breaks lower threshold (0.4g)
        trigger1 = true;

        Serial.println("TRIGGER 1 ACTIVATED");
    }
    if (trigger1 == true)
    {
        trigger1count++;
        if (Amp >= 12)
        {
            // if AM breaks upper threshold (3g)
            trigger2 = true;
            Serial.println("TRIGGER 2 ACTIVATED");
            trigger1 = false;
            trigger1count = 0;
        }
    }
    if (trigger2 == true)
    {
        
        trigger2count++;
        angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
        Serial.println(angleChange);
        if (angleChange >= 30 && angleChange <= 400)
        {
          fall = true;
            // if orientation changes by between 80-100 degrees
            trigger3 = true;
            trigger2 = false;
            trigger2count = 0;
            Serial.println(angleChange);
            Serial.println("TRIGGER 3 ACTIVATED");
        }
    }
    if (trigger3 == true)
    {
        
        trigger3count++;
        if (trigger3count >= 10)
        {
            angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
            delay(10);
            Serial.println(angleChange);
            if ((angleChange >= 0) && (angleChange <= 10))
            {
                // if orientation changes remains between 0-10 degrees

                trigger3 = false;
                trigger3count = 0;
                Serial.println(angleChange);
            }
            else
            {
                // user regained normal orientation
                trigger3 = false;
                trigger3count = 0;
                Serial.println("TRIGGER 3 DEACTIVATED");
            }
        }
    }
    if (fall == true)
    {
        // in event of a fall detection
        Serial.println("FALL DETECTED");

        timeClient.update();

        time_t epochTime = timeClient.getEpochTime();
        
        formattedTime = timeClient.getFormattedTime();
        Serial.print("Time: ");
        Serial.println(formattedTime);  
      
        //Get a time structure
        struct tm *ptm = gmtime ((time_t *)&epochTime); 
      
        int monthDay = ptm->tm_mday;
        int currentMonth = ptm->tm_mon+1;
        int currentYear = ptm->tm_year+1900;
      
        //Print complete date:
        currentDate = String(monthDay) + "-" + String(currentMonth) + "-"+ String(currentYear)  ;
        Serial.print("Date: ");
        Serial.println(currentDate);
      
        Serial.println("");
      
//        delay(2000);
          
        char bssid[6];
        Serial.println("scan start");
        // WiFi.scanNetworks will return the number of networks found
        int n = WiFi.scanNetworks();
        Serial.println("scan done");
        if (n == 0)
        {
            Serial.println("no networks found");
        }
        else
        {
          String uniq;
            Serial.print(n);
            Serial.println(" networks found...");
            if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
            {
                Serial.printf("Set phoneNumber... %s\n", Firebase.RTDB.setString(&fbdo,
                "/test/phoneNumber/", phoneNumber) ? "ok" : fbdo.errorReason().c_str());
                Serial.printf("Set customerId... %s\n", Firebase.RTDB.setString(&fbdo, 
                "/test/customerId/", customerId) ? "ok" : fbdo.errorReason().c_str());
                Serial.printf("Set Date... %s\n", Firebase.RTDB.setString(&fbdo,
                "/test/date/", currentDate) ? "ok" : fbdo.errorReason().c_str());
                Serial.printf("Set Time... %s\n", Firebase.RTDB.setString(&fbdo,
                "/test/time/", formattedTime) ? "ok" : fbdo.errorReason().c_str());
            for (int j = 0; j < n; ++j)
            {
              uniq = String(j);
              Serial.println(uniq);
              Serial.printf("Set wifiMACAddress... %s\n", Firebase.RTDB.setString(&fbdo, 
              "/test/wifiAccessPoints/" + uniq + "/macAddress", WiFi.BSSIDstr(j)) ? "ok" : fbdo.errorReason().c_str());
              Serial.printf("Set wifiMACAddress... %s\n", Firebase.RTDB.setString(&fbdo, 
              "/test/wifiAccessPoints/" + uniq + "/signalStrength", WiFi.RSSI(j)) ? "ok" : fbdo.errorReason().c_str());
//               sendDataPrevMillis = millis();
            }   

                           
                fall = false;
            }
            if (trigger2count >= 6)
            {
                // allow 0.5s for orientation change
                trigger2 = false;
                trigger2count = 0;
                Serial.println("TRIGGER 2 DECACTIVATED");
            }
            if (trigger1count >= 6)
            { // allow 0.5s for AM to break upper threshold
                trigger1 = false;
                trigger1count = 0;
                Serial.println("TRIGGER 1 DECACTIVATED");
            }
            delay(100);
        }
    }
}

void mpu_read()
{
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
//    This function is used by the controller device to request bytes from a peripheral device.
//    The bytes may then be retrieved with the available() and read() functions. As of Arduino 1.0.1,
//    requestFrom() accepts a boolean argument changing its behavior for compatibility with certain I2C devices.
//    If true, requestFrom() sends a stop message after the request, releasing the I2C bus.
//    If false, requestFrom() sends a restart message after the request. 
    AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
//    This function reads a byte that was transmitted from a peripheral device to a controller device after 
//    a call to requestFrom() or was transmitted from a controller device to a peripheral device. read() inherits
//    from the Stream utility class.
    AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}
