#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "Wire.h"

// Replace with your network credentials
const char* ssid = "BB30";
const char* password = "bb302445";
//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 3
unsigned int localPort = 123;      // local port to listen for UDP packets
WiFiUDP udp;                     // Create an instance of the WiFiUDP class to send and receive
IPAddress timeServerIP(158, 144, 1,37);          // tifr NTP server address
const char* ntpServerName = "ntp.tifr.res.in";//"time.nist.gov";
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
 
ESP8266WebServer webserver(80);   //instantiate server at port 80 (http port) 
WiFiServer telnetserver(23);         //Telnet server at port 23 
WiFiClient serverClients[MAX_SRV_CLIENTS];
String page = "";
double data;
const int MPU_addr=0x68; // I2C address of the MPU-6050
unsigned long previousMillis = 0;        // will store last temp was read
unsigned long elapsedMillis = 0;
unsigned long count = 0;
bool flag = false;

const float MPU_GYRO_250_SCALE = 131.0;
const float MPU_GYRO_500_SCALE = 65.5;
const float MPU_GYRO_1000_SCALE = 32.8;
const float MPU_GYRO_2000_SCALE = 16.4;
const float MPU_ACCL_2_SCALE = 16384.0;
const float MPU_ACCL_4_SCALE = 8192.0;
const float MPU_ACCL_8_SCALE = 4096.0;
const float MPU_ACCL_16_SCALE = 2048.0;
 
int16_t r_AcX,r_AcY,r_AcZ,r_Tmp,r_GyX,r_GyY,r_GyZ;
float s_AcX,s_AcY,s_AcZ,s_Tmp,s_GyX,s_GyY,s_GyZ;
int hours,mins,secs;

int ledPin = 2;
bool ledState = LOW;

struct rawdata {
int16_t AcX;
int16_t AcY;
int16_t AcZ;
int16_t Tmp;
int16_t GyX;
int16_t GyY;
int16_t GyZ;
};
 
struct scaleddata{
float AcX;
float AcY;
float AcZ;
float Tmp;
float GyX;
float GyY;
float GyZ;
};

rawdata offsets;

//byte check_I2c(byte addr);
void mpu6050Begin(byte addr);
rawdata mpu6050Read(byte addr, bool Debug);
void setMPU6050scales(byte addr,uint8_t Gyro,uint8_t Accl);
void getMPU6050scales(byte addr,uint8_t &Gyro,uint8_t &Accl);
scaleddata convertRawToScaled(byte addr, rawdata data_in,bool Debug);
//
void calibrateMPU6050(byte addr, rawdata &offsets,char up_axis, int num_samples,bool Debug);
rawdata averageSamples(rawdata * samps,int len);
void turnOff();
void turnOn();
void toggle();
//unsigned long sendNTPpacket(IPAddress& address);
//inline int getHours(uint32_t UNIXTime);
//inline int getMinutes(uint32_t UNIXTime);
//inline int getSeconds(uint32_t UNIXTime);
 
void setup() {
Wire.begin();
Serial.begin(115200);

pinMode(ledPin, OUTPUT);
digitalWrite(ledPin, ledState);

WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password); //begin WiFi connection
Serial.println("");
 
check_I2c(MPU_addr); // Check that there is an MPU
 
Wire.beginTransmission(MPU_addr);
Wire.write(0x6B); // PWR_MGMT_1 register
Wire.write(0); // set to zero (wakes up the MPU-6050)
Wire.endTransmission(true);

delay(30); // Ensure gyro has enough time to power up

// Wait for connection
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
//  server.on("/", [](){
//     // page = "<h1>Sensor to Node MCU Web Server</h1><h3>GyX = </h3> <h4>"+String(data)+"</h4>";
//   //page="Raw data \nGyX = "+String(r_GyX)+" | GyY = "+String(r_GyY)+" | GyZ = "+String(r_GyZ)+" | Tmp = "+String(r_Tmp)+" | AcX = "+String(r_AcX)+" | AcY = "+String(r_AcY)+" | AcZ = "+String(r_AcZ)+"\nScaled data \nGyX = "+String(s_GyX)+" &deg;/s | GyY = "+String(s_GyY)+" &deg;/s | GyZ = "+String(s_GyZ)+" &deg;/s | Tmp = "+String(s_Tmp)+" &deg;C | AcX = "+String(s_AcX)+" g | AcY = "+String(s_AcY)+" g | AcZ = "+String(s_AcZ)+" g\n";
//    page=String(s_GyX)+"\t"+String(s_GyY)+"\t"+String(s_GyZ)+"\t"+String(s_AcX)+"\t"+String(s_AcY)+"\t"+String(s_AcZ)+"\n";
//    server.send(200, "text/plain", page);
//  });

  Serial.println("Starting UDP");
  udp.begin(localPort);                          // Start listening for UDP messages on port 123
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.print("Time server IP: ");
  Serial.println(timeServerIP);
//start UART and the server
  //Serial.begin(115200);
  telnetserver.begin();
  telnetserver.setNoDelay(true);
  
  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");

  webserver.on("/1", turnOn);         //Associate the handler function to the path
  webserver.on("/0", turnOff);        //Associate the handler function to the path
  webserver.on("/toggle", toggle);   //Associate the handler function to the path
  
  webserver.begin();
  Serial.println("Web server started!");
  previousMillis = millis();
}
 
void loop() {

rawdata next_sample;
setMPU6050scales(MPU_addr,0b00011000,0b00010000);
next_sample = mpu6050Read(MPU_addr, true);
scaleddata scaled_value;
scaled_value = convertRawToScaled(MPU_addr, next_sample,true);

r_GyX=next_sample.GyX;
r_GyY=next_sample.GyY;
r_GyZ=next_sample.GyZ;
r_Tmp=next_sample.Tmp;
r_AcX=next_sample.AcX;
r_AcY=next_sample.AcY;
r_AcZ=next_sample.AcZ; 

s_GyX=scaled_value.GyX;
s_GyY=scaled_value.GyY;
s_GyZ=scaled_value.GyZ;
s_Tmp=scaled_value.Tmp;
s_AcX=scaled_value.AcX;
s_AcY=scaled_value.AcY;
s_AcZ=scaled_value.AcZ; 

delay(194); // Wait 0.05 seconds and scan again

//Serial.print(s_GyX);
//Serial.print("\t"); Serial.print(s_GyY);
//Serial.print("\t"); Serial.print(s_GyZ);
////Serial.print(" | Tmp = "); Serial.print(values.Tmp);
//Serial.print("\t"); Serial.print(s_AcX);
//Serial.print("\t"); Serial.print(s_AcY);
//Serial.print("\t"); Serial.println(s_AcZ);

webserver.handleClient();

uint8_t i;
  //check if there are any new clients
  if (telnetserver.hasClient()){
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()){
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = telnetserver.available();
        Serial.print("New client: "); Serial.println(i+1);
        flag = true;
        break;
      }
    }
  }
    //no free/disconnected spot so reject
    if ( i == MAX_SRV_CLIENTS) {
       WiFiClient serverClient = telnetserver.available(); 
       serverClient.stop();
        Serial.println("Connection rejected ");
    }
  //}
//  //check clients for data
//  for(i = 0; i < MAX_SRV_CLIENTS; i++){
//    if (serverClients[i] && serverClients[i].connected()){
//      if(serverClients[i].available()){
//        //get data from the telnet client and push it to the UART
//        while(serverClients[i].available()){ 
//          char check=serverClients[i].read();
//          Serial.println(check);
//          if(check=='l'){turnOn; }
//          if(check=='h'){turnOff; }
//        }
//      }
//    }
//  }

//Send First 10 timestamp
  
  if(count<10 && flag){
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    //Serial.print("packet received, length=");
    //Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = " );
    //Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    //Serial.println(epoch);
    
  unsigned long currentMillis = millis();
  //elapsedMillis += currentMillis - previousMillis;
    hours = getHours(epoch);
    mins = getMinutes(epoch);
    secs = getSeconds(epoch); 
    //String str=String(s_GyX)+"\t"+String(s_GyY)+"\t"+String(s_GyZ)+"\t"+String(s_AcX)+"\t"+String(s_AcY)+"\t"+String(s_AcZ)+"\t"+String(elapsedMillis)+"\r\n";
    String str="UTC time:\t"+String(hours)+":"+String(mins)+":"+String(secs)+"\n";
   previousMillis = currentMillis;
    //push data to all connected telnet clients
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        //serverClients[i].write((const uint8_t *)sbuf, len);
        //serverClients[i].write(sbuf, len);
        serverClients[i].println(str);
        Serial.printf("\rUTC time:\t%d:%d:%d   \n", hours, mins, secs);
        delay(1);
      }
    }
  }
    count++;
  }
  //Send data
  //else {
  if(count >=10 && flag){
  unsigned long currentMillis = millis();
  elapsedMillis += currentMillis - previousMillis;
    String str=String(s_GyX)+"\t"+String(s_GyY)+"\t"+String(s_GyZ)+"\t"+String(s_AcX)+"\t"+String(s_AcY)+"\t"+String(s_AcZ)+"\t"+String(analogRead(A0))+"\t"+String(elapsedMillis)+"\t"+String(digitalRead(ledPin))+"\n";
   previousMillis = currentMillis;
    //push data to all connected telnet clients
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        //serverClients[i].write((const uint8_t *)sbuf, len);
        //serverClients[i].write(sbuf, len);
        serverClients[i].println(str);
        delay(1);
      }
    }
  //}
  
}
}
//}
 
byte check_I2c(byte addr){
// We are using the return value of
// the Write.endTransmisstion to see if
// a device did acknowledge to the address.
byte error;
Wire.beginTransmission(addr);
error = Wire.endTransmission();
 
if (error == 0)
{
Serial.print(" Device Found at 0x");
Serial.println(addr,HEX);
}
else
{
Serial.print(" No Device Found at 0x");
Serial.println(addr,HEX);
}
return error;
}

rawdata mpu6050Read(byte addr, bool Debug){
// This function reads the raw 16-bit data values from
// the MPU-6050
 
rawdata values;
 
Wire.beginTransmission(addr);
Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
Wire.endTransmission(false);
Wire.requestFrom(addr,14,true); // request a total of 14 registers
values.AcX=Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
values.AcY=Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
values.AcZ=Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
values.Tmp=Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
values.GyX=Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
values.GyY=Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
values.GyZ=Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L) 

values.AcX-=offsets.AcX;
values.AcY-=offsets.AcY;
values.AcZ-=offsets.AcZ;
values.GyX-=offsets.GyX;
values.GyY-=offsets.GyY;
values.GyZ-=offsets.GyZ;
 
if(Debug){
//Serial.print(" GyX = "); Serial.print(values.GyX);
//Serial.print(" | GyY = "); Serial.print(values.GyY);
//Serial.print(" | GyZ = "); Serial.print(values.GyZ);
//Serial.print(" | Tmp = "); Serial.print(values.Tmp);
//Serial.print(" | AcX = "); Serial.print(values.AcX);
//Serial.print(" | AcY = "); Serial.print(values.AcY);
//Serial.print(" | AcZ = "); Serial.println(values.AcZ);
}
 
return values;
}

//
void calibrateMPU6050(byte addr,rawdata &offsets,char up_axis ,int num_samples, bool Debug){
// This function reads in the first num_samples and averages them
// to determine calibration offsets, which are then used in
// when the sensor data is read.
 
// It simply assumes that the up_axis is vertical and that the sensor is not
// moving.
rawdata temp[num_samples];
int scale_value;
byte Gyro, Accl;
 
for(int i=0; i<num_samples; i++){
temp[i] = mpu6050Read(addr,false);
}
 
offsets = averageSamples(temp,num_samples);
getMPU6050scales(MPU_addr, Gyro, Accl);
 
switch (Accl){
case 0:
scale_value = (int)MPU_ACCL_2_SCALE;
break;
case 1:
scale_value = (int)MPU_ACCL_4_SCALE;
break;
case 2:
scale_value = (int)MPU_ACCL_8_SCALE;
break;
case 3:
scale_value = (int)MPU_ACCL_16_SCALE;
break;
default:
break;
}
 
 
switch(up_axis){
case 'X':
offsets.AcX -= scale_value;
break;
case 'Y':
offsets.AcY -= scale_value;
break;
case 'Z':
offsets.AcZ -= scale_value;
break;
default:
break;
}
if(Debug){
//Serial.print(" Offsets: GyX = "); Serial.print(offsets.GyX);
//Serial.print(" | GyY = "); Serial.print(offsets.GyY);
//Serial.print(" | GyZ = "); Serial.print(offsets.GyZ);
//Serial.print(" | AcX = "); Serial.print(offsets.AcX);
//Serial.print(" | AcY = "); Serial.print(offsets.AcY);
//Serial.print(" | AcZ = "); Serial.println(offsets.AcZ);
}
}
 
rawdata averageSamples(rawdata * samps,int len){
rawdata out_data;
scaleddata temp;
 
temp.GyX = 0.0;
temp.GyY = 0.0;
temp.GyZ = 0.0;
temp.AcX = 0.0;
temp.AcY = 0.0;
temp.AcZ = 0.0;
 
for(int i = 0; i < len; i++){
temp.GyX += (float)samps[i].GyX;
temp.GyY += (float)samps[i].GyY;
temp.GyZ += (float)samps[i].GyZ;
temp.AcX += (float)samps[i].AcX;
temp.AcY += (float)samps[i].AcY;
temp.AcZ += (float)samps[i].AcZ;
}
 
out_data.GyX = (int16_t)(temp.GyX/(float)len);
out_data.GyY = (int16_t)(temp.GyY/(float)len);
out_data.GyZ = (int16_t)(temp.GyZ/(float)len);
out_data.AcX = (int16_t)(temp.AcX/(float)len);
out_data.AcY = (int16_t)(temp.AcY/(float)len);
out_data.AcZ = (int16_t)(temp.AcZ/(float)len);
 
return out_data;
 
}
 
void setMPU6050scales(byte addr,uint8_t Gyro,uint8_t Accl){
Wire.beginTransmission(addr);
Wire.write(0x1B); // write to register starting at 0x1B
Wire.write(Gyro); // Self Tests Off and set Gyro FS to 2000
Wire.write(Accl); // Self Tests Off and set Accl FS to 8g
Wire.endTransmission(true);
}
 
void getMPU6050scales(byte addr,uint8_t &Gyro,uint8_t &Accl){
Wire.beginTransmission(addr);
Wire.write(0x1B); // starting with register 0x3B (ACCEL_XOUT_H)
Wire.endTransmission(false);
Wire.requestFrom(addr,2,true); // request a total of 2 registers
Gyro = (Wire.read()&(bit(3)|bit(4)))>>3;
Accl = (Wire.read()&(bit(3)|bit(4)))>>3;
}
 
  
 
scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug){
 
scaleddata values;
float scale_value = 0.0;
byte Gyro, Accl;
 
getMPU6050scales(MPU_addr, Gyro, Accl);
 
if(Debug){
//Serial.print("Gyro Full-Scale = ");
}
 
switch (Gyro){
case 0:
scale_value = MPU_GYRO_250_SCALE;
if(Debug){
//Serial.println("±250 °/s");
}
break;
case 1:
scale_value = MPU_GYRO_500_SCALE;
if(Debug){
//Serial.println("±500 °/s");
}
break;
case 2:
scale_value = MPU_GYRO_1000_SCALE;
if(Debug){
//Serial.println("±1000 °/s");
}
break;
case 3:
scale_value = MPU_GYRO_2000_SCALE;
if(Debug){
//Serial.println("±2000 °/s");
}
break;
default:
break;
}
 
values.GyX = (float) data_in.GyX / scale_value;
values.GyY = (float) data_in.GyY / scale_value;
values.GyZ = (float) data_in.GyZ / scale_value;
 
scale_value = 0.0;
if(Debug){
//Serial.print("Accl Full-Scale = ");
}
switch (Accl){
case 0:
scale_value = MPU_ACCL_2_SCALE;
if(Debug){
//Serial.println("±2 g");
}
break;
case 1:
scale_value = MPU_ACCL_4_SCALE;
if(Debug){
//Serial.println("±4 g");
}
break;
case 2:
scale_value = MPU_ACCL_8_SCALE;
if(Debug){
//Serial.println("±8 g");
}
break;
case 3:
scale_value = MPU_ACCL_16_SCALE;
if(Debug){
//Serial.println("±16 g");
}
break;
default:
break;
}
values.AcX = (float) data_in.AcX / scale_value;
values.AcY = (float) data_in.AcY / scale_value;
values.AcZ = (float) data_in.AcZ / scale_value;
 
  
 
values.Tmp = (float) data_in.Tmp / 340.0 + 36.53;
 
if(Debug){
//Serial.print(" GyX = "); Serial.print(values.GyX);
//Serial.print(" °/s| GyY = "); Serial.print(values.GyY);
//Serial.print(" °/s| GyZ = "); Serial.print(values.GyZ);
//Serial.print(" °/s| Tmp = "); Serial.print(values.Tmp);
//Serial.print(" °C| AcX = "); Serial.print(values.AcX);
//Serial.print(" g| AcY = "); Serial.print(values.AcY);
//Serial.print(" g| AcZ = "); Serial.print(values.AcZ);Serial.println(" g");
}
 
return values;
}

void turnOff(){
ledState = HIGH;
digitalWrite(ledPin, ledState);
webserver.send(200, "text/plain", "LED off");
}

void turnOn(){
ledState = LOW;
digitalWrite(ledPin, ledState);
webserver.send(200, "text/plain", "LED on");
}

void toggle(){
ledState = !ledState;
digitalWrite(ledPin, ledState);
webserver.send(200, "text/plain", "LED toggled");
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("Sending NTP request...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, localPort); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}
