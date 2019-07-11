/* 
The Noise Generator uses the same development boards and is designed to send
 data to the server in order to create datapoints for the sensors.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
extern "C"
{
#include "user_interface.h"
}

const char WiFiSSID[] = "---";
const char WiFiPSK[] = "---";

const char ServerIP[] = "---";
const int httpPort = 50000;

uint8_t MAC_array[6];
char MAC_char[18];

#define DEADBATTERYVOLTAGE 3.7

int looptime = 0;
WiFiClient client;

void postData()
{
  while (WiFi.status() != WL_CONNECTED)
  {

    Serial.print("C");
    delay(1);
  }
  if (!client.connected())
  {
    if (!client.connect(ServerIP, httpPort))
    {
      return;
    }
    //If we manage to connect, Send the MAC of this device to the server
    client.write((char *)MAC_array, 6);
    client.write('@');
  }

  client.write("DankMemes"); //Just send any data to the server so that we can flood the airwaves
  uint8_t voltage = 10 * (analogRead(A0) / 140.00);
  Serial.println(voltage);
  client.write(voltage);
  //  client.write((char *)buffer, stream.bytes_written);  //client.write() max transmit appears to be ~2900 bytes.   WRITE IN CHUNKS WRITE IN CHUNKS WRITE IN CHUNKS WRITE IN CHUNKS

  client.flush(); //Ensure all data is sent
}

void flash()
{
  digitalWrite(BUILTIN_LED, LOW);
  delay(1);
  digitalWrite(BUILTIN_LED, HIGH);
  //delay(100);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i)
  {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }

  Serial.println(MAC_char);

  system_restore();

  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);

  pinMode(BUILTIN_LED, OUTPUT);

  //  Serial.begin(230400); //////////////Enable for debugging
}

void checkbattery()
{

  int sensorValue = analogRead(A0);
  float voltage = sensorValue / 140.00; //137.26

  if (voltage < DEADBATTERYVOLTAGE)
  {
    const int sleepSeconds = 5;
    Serial.print("Battery Dead");
    ESP.deepSleep(sleepSeconds * 1000000); //Note there is no wire from D0 to RST. So even after 5 seconds the device will not re-enable
  }
}

void loop()
{
  if ((millis() - looptime) > 100)
  { //10 times a second
    postData();
    flash();
    checkbattery();
    looptime = millis();
  }
  delay(10);
}
