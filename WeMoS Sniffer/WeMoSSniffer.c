/*
These guys are the sensors.
They cycle between sniffing wifi packets and then reporting the information.

Sorry for the lack of comments. Hopefully most of it is self explanitory
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

//#include <Event.h>
#include <Timer.h> //https://github.com/JChristensen/Timer
#include <string.h>

#define DEVICEBUFFERSIZE 40
#define CYCLEFREQ 40
#define DOWNTIME 7
#define MINDEVICETIME 1

#define THISDEVICEX 4
#define THISDEVICEY 3

#define DEADBATTERYVOLTAGE 3.7

const char WiFiSSID[] = "---";
const char WiFiPSK[] = "---";
const char ServerIP[] = "---";

extern "C"
{
#include "user_interface.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "thesisprotocol.pb.h"
}
#include "sniffer_structs.h"

struct deviceDataPoints
{
  signed rssi;
  unsigned long timesince;
};

struct deviceInfo
{
  uint8 mac[6];
  deviceDataPoints data[CYCLEFREQ - DOWNTIME];
};

struct deviceInfo tableofinfo[DEVICEBUFFERSIZE];

long looptime;

void normalmode()
{
  ///ENTERING NORMAL WIFI MODE//////
  wifi_promiscuous_enable(0);
  system_restore();

  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);
}

int writeindex = 0;
struct MAC80211 *mac;           // = (struct MAC80211 *)sniffer->buf;
struct RxControl rxcontrolData; // = sniffer->rx_ctrl;
static void rx_callback(uint8 *buf, uint16 len)
{
  // Reference: ESP8266 SDK Programming Guide section 8.3 Sniffer Structure Introduction

  if (writeindex >= DEVICEBUFFERSIZE)
  {
    return;
  }

  if (len == 128)
  {
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2 *)buf;
    mac = (struct MAC80211 *)sniffer->buf;
    rxcontrolData = sniffer->rx_ctrl;
  }
  else if (len == 12)
  {
    return;
    // Case 2: len == 12
    // buf contains RxControl but it is not reliable. It cannot show the MAC addresses or the length of the packet.
    // Do nothing with it.
  }
  else
  {
    // Case 3: len == X*10
    // buf contains sniffer_buf
    struct sniffer_buf *sniffer = (struct sniffer_buf *)buf;

    // These data packets passed CRC, so the MAC addresses should be reliable.

    mac = (struct MAC80211 *)sniffer->buf;
    rxcontrolData = sniffer->rx_ctrl;
    // sniffer->buf contains the first 36 bytes of the IEEE80211 header
  }

  float alpha = 0.3;
  bool could_not_find_address = true;
  //check to see if mac address is in the table already
  for (int x = 0; x < writeindex; x++)
  {
    if (memcmp(mac->addr2, tableofinfo[x].mac, sizeof(tableofinfo[x].mac)) == 0)
    {
      could_not_find_address = false;
      for (int i = 0; i < (CYCLEFREQ - DOWNTIME); i++)
      {

        if (tableofinfo[x].data[i].rssi == 0)
        { //If we are at the end of current data

          tableofinfo[x].data[i].rssi = rxcontrolData.rssi;
          tableofinfo[x].data[i].timesince = millis();
          break;
        }

        if ((millis() - tableofinfo[x].data[i].timesince) < MINDEVICETIME * 1000)
        {                                                                                                       //If the last data point was less than 1 second ago, just add to the average of that second
          tableofinfo[x].data[i].rssi = alpha * rxcontrolData.rssi + (1 - alpha) * tableofinfo[x].data[i].rssi; // Exponentially Weighted Moving Average;
          break;
        }
      }
      break;
    }
  }

  //if not in the table
  if (could_not_find_address)
  {
    memcpy(tableofinfo[writeindex].mac, mac->addr2, 6);
    tableofinfo[writeindex].data[0].rssi = rxcontrolData.rssi;
    tableofinfo[writeindex].data[0].timesince = millis();

    writeindex++;
  }
}

void sniffmode()
{
  wifi_set_promiscuous_rx_cb(rx_callback);
  ///ENTERING SNIFFER MODE//////
  WiFi.disconnect(); //This is messy, but switching the microcontroller between these modes so often gets glitchy so we just do all of the commands
  wifi_station_disconnect();
  system_restore();
  WiFi.disconnect();
  wifi_station_disconnect();
  wifi_set_opmode(STATION_MODE);
  wifi_set_channel(1);
  wifi_promiscuous_enable(1);
}

int totalsent = 0;

bool createSubMessageDataPoints(pb_ostream_t *streamz, const pb_field_t *field, void *const *arg)
{
  for (int i = 0; i < (CYCLEFREQ - DOWNTIME); i++)
  {
    ReceivedMessage_DataPoint dataPointToAdd;

    if (tableofinfo[totalsent].data[i].rssi == 0)
    { //If we are at the end of current data
      dataPointToAdd.rssi = 0;
      dataPointToAdd.secondsSince = 0;
    }
    else
    {
      dataPointToAdd.rssi = tableofinfo[totalsent].data[i].rssi;
      dataPointToAdd.secondsSince = (int)((millis() - tableofinfo[totalsent].data[i].timesince) / 1000);
    }
    pb_encode_tag_for_field(streamz, field);
    if (!pb_encode_submessage(streamz, ReceivedMessage_DataPoint_fields, &dataPointToAdd))
    {
    }
  }
}

bool generateStringMAC(pb_ostream_t *streamz, const pb_field_t *field, void *const *arg)
{
  pb_encode_tag_for_field(streamz, field);
  pb_encode_string(streamz, tableofinfo[totalsent].mac, 6);
}

bool createSubMessageDevice(pb_ostream_t *streamz, const pb_field_t *field, void *const *arg)
{
  totalsent = 0;
  while (totalsent < writeindex)
  { //For all of the devices we have found

    ReceivedMessage_Device deviceToAdd;
    deviceToAdd.mac.funcs.encode = &generateStringMAC;

    deviceToAdd.datapoints.funcs.encode = &createSubMessageDataPoints;

    pb_encode_tag_for_field(streamz, field);

    if (!pb_encode_submessage(streamz, ReceivedMessage_Device_fields, &deviceToAdd))
    {
    }
    totalsent++;
  }
}

unsigned long timeToConnectToWifi;
bool lastPost = 0;

uint8_t buffer[(DEVICEBUFFERSIZE * (sizeof(ReceivedMessage_Device) + 5)) + (DEVICEBUFFERSIZE * (CYCLEFREQ - DOWNTIME) * (sizeof(_ReceivedMessage_DataPoint) + 5)) + sizeof(ReceivedMessage) + 15]; //This currently in testing
uint8_t wifibuffer[2900];                                                                                                                                                                          //For sending data over wifi (Note the maximum size that arduino allows to send at once with built in methods is around 2950ish, hence a buffer of 2900. Altho this is not documented anywhere)
void postData()
{

  if (writeindex == 0)
  {
    return;
  }

  int i = 0;

  //Generate the Protobuf stream
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  ReceivedMessage dataToSend;

  dataToSend.deviceX = THISDEVICEX;
  dataToSend.deviceY = THISDEVICEY;
  dataToSend.deviceVoltage = analogRead(A0) / 140.00;

  if (lastPost)
  { //Tell the PC this is the last time we are posting because the battery is dead
    dataToSend.lastPost = 1;
  }

  dataToSend.devices.funcs.encode = &createSubMessageDevice;

  pb_encode(&stream, ReceivedMessage_fields, &dataToSend);

  int timeToSubmit = millis();
  while (WiFi.status() != WL_CONNECTED) //If wifi has still not connected while we were processing the data, wait for it
  {
    delay(100);
  }

  WiFiClient client;
  const int httpPort = 52000;
  if (!client.connect(ServerIP, httpPort)) //This is blocking
  {
    return;
  }

  client.write(highByte(stream.bytes_written)); //Start of transmission tell server how much data we will be sending
  client.write(lowByte(stream.bytes_written));
  client.write((int8_t)((millis() - timeToSubmit) / 1000));

  int totalTime = (millis() - timeToConnectToWifi);

  client.write(highByte(totalTime));
  client.write(lowByte(totalTime));

  int t = 0;
  for (t = 0; t < floor(stream.bytes_written / 2900); t++)
  { //How many full wifibuffers of data we will send
    for (int y = 0; y < 2900; y++)
    {
      wifibuffer[y] = buffer[y + (t * 2900)];
    }
    client.write((char *)wifibuffer, 2900);
  }

  for (int u = 0; u < (stream.bytes_written % 2900); u++)
  { //How many bytes are left after all those other buffers
    wifibuffer[u] = buffer[u + (t * 2900)];
  }

  client.write((char *)wifibuffer, (stream.bytes_written % 2900));
  client.flush(); //Ensure all data is sent
}

void printtendevices()
{ //For debug purposes
  //Serial.print("\n\r");
  for (int i = 0; i < 10; i++)
  {
    //Serial.print("|");
    for (int j = 0; j < 6; j++)
    {
      //Serial.print(tableofinfo[i].mac[j]);
      //Serial.print(":");
    }
    //Serial.print("|");
    for (int k = 0; k < (CYCLEFREQ - DOWNTIME); k++)
    {
      //Serial.print("[");
      //Serial.print(tableofinfo[i].data[k].rssi);
      //Serial.print(",");
      //Serial.print(millis() - tableofinfo[i].data[k].timesince);
      //Serial.print("]");
    }
    //Serial.print("\n\r");
  }
}

bool firstBoot = 1;
void checkbattery()
{

  int sensorValue = analogRead(A0);
  float voltage = sensorValue / 140.00; //137.26

  if (voltage < DEADBATTERYVOLTAGE)
  {
    if (!firstBoot)
    { //If the battery isnt good enough on boot, then turn off
      lastPost = 1;
      normalmode(); //Otherwise if we have been running for a while and the battery has finally dropped, then do a last post to notify server
      postData();
    }
    const int sleepSeconds = 5;
    ESP.deepSleep(sleepSeconds * 1000000); //Note there is no wire from D0 to RST. So even after 5 seconds the device will not re-enable
  }
  firstBoot = 0;
}

void flash()
{
  digitalWrite(BUILTIN_LED, LOW);
  delay(1);
  digitalWrite(BUILTIN_LED, HIGH);
}

void gogogo()
{
  timeToConnectToWifi = millis();
  int ignoreme = millis();
  normalmode();

  postData();
  writeindex = 0; //Now that we have sent all the data or tried at least. Start overwriting old stuff since there is no use sending the same data twice
  memset(tableofinfo, 0, sizeof(tableofinfo));

  while ((millis() - timeToConnectToWifi) < (DOWNTIME * 1000))
  { //Make sure the downtime of all the devices is about the same
    delay(1);
  }

  sniffmode();
}

void waitForStartCommand()
{

  normalmode();
  while (WiFi.status() != WL_CONNECTED)
  {
    flash();
    firstBoot = 1;
    checkbattery();
    delay(100);
  }

  WiFiClient client;
  const int starterPort = 54000;
  while (!client.connect(ServerIP, starterPort)) //This is blocking
  {                                              // If we fail to connect, just start again
    firstBoot = 1;
    checkbattery();
    delay(100);
  }

  digitalWrite(BUILTIN_LED, LOW);
  client.flush();
  while (client.read() == -1 && client.connected())
  { //Just wait until the server sends some data, then begin everything.
    delay(1);
  }
}

Timer ledTimer;
Timer battTimer;
void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
  checkbattery();
  waitForStartCommand();
  looptime = millis() - (1000 * DOWNTIME);
  //  Serial.begin(230400); /////////////////Enable for debugging
  sniffmode();

  ledTimer.every(3000, flash);          //3 seconds
  battTimer.every(30000, checkbattery); //30 seconds
}

void loop()
{
  delay(1);
  ledTimer.update();
  battTimer.update();

  if (((millis() - looptime) > (CYCLEFREQ * 1000)))
  { //If it has been time
    looptime = millis();
    gogogo();
  }

  // Memory barrier to forbid the optimiser from moving the write to the volatile variable have_new_address
  // above any of the above code. This ensures that the print functions have finished reading memory before
  // the receive callback will write new addresses in place.
  asm volatile(""
               :
               :
               : "memory");
}
