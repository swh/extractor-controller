/**
 * RTC_NTPSync
 * 
 * This example shows how to set the RTC (Real Time Clock) on the Portenta C33 / UNO R4 WiFi
 * to the current date and time retrieved from an NTP server on the Internet (pool.ntp.org).
 * Then the current time from the RTC is printed to the Serial port.
 * 
 * Instructions:
 * 1. Download the NTPClient library (https://github.com/arduino-libraries/NTPClient) through the Library Manager
 * 2. Change the WiFi credentials in the arduino_secrets.h file to match your WiFi network.
 * 3. Upload this sketch to Portenta C33 / UNO R4 WiFi.
 * 4. Open the Serial Monitor.
 * 
 * Initial author: Sebastian Romero @sebromero
 * 
 * Find the full UNO R4 WiFi RTC documentation here:
 * https://docs.arduino.cc/tutorials/uno-r4-wifi/rtc
 */

// Include the RTC library
#include "RTC.h"

//Include the NTP library
#include <NTPClient.h>

#include <WiFiS3.h>

#include <WiFiUdp.h>
#include "wifi_secrets.h"

void lcdMessage(char *l1, char *l2=NULL, char *l3=NULL, char *l4=NULL);

char ssid[] = SSID_NAME;
char pass[] = SSID_PASSWORD;

int wifiStatus = WL_IDLE_STATUS;
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp);

void printWifiStatus() {  
  char ssid[21];
  snprintf(ssid, 20, "SSID: %s", WiFi.SSID());

  char ipaddr[21];
  IPAddress ip = WiFi.localIP();
  snprintf(ipaddr, 20, "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  char rssi[21];
  snprintf(rssi, 20, "RSSI: %d dBm", WiFi.RSSI());

  lcdMessage(ssid, ipaddr, NULL, rssi);
}

void connectToWiFi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    lcdMessage("Comms with WiFi", "module failed!", NULL, "Halting");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    lcdMessage("Old wifi firmware", "Please upgrade");
    delay(2000);
  }

  // attempt to connect to WiFi network:
  while (wifiStatus != WL_CONNECTED) {
    lcdMessage("Connecting to", ssid);
    // Connect to WPA/WPA2 network.
    wifiStatus = WiFi.begin(ssid, pass);
    // Give it 5 seconds to connect, if not, retry
    delay(5000);
  }

  printWifiStatus();
}

void checkWiFiConnection() {
  wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);  
  }
}

void wiFiInit() {
  connectToWiFi();
  RTC.begin();
  Serial.println("Connecting to NTP server");
  timeClient.begin();
  timeClient.update();

  auto unixTime = timeClient.getEpochTime();
  Serial.print("Unix time = ");
  Serial.println(unixTime);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);

  // Retrieve the date and time from the RTC and print them
  RTCTime currentTime;
  RTC.getTime(currentTime); 
  Serial.println("RTC set to " + String(currentTime));
}