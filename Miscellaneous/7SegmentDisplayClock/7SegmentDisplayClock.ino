#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "ESP8266HTTPUpdateServerCustom.h"
#include <WiFiUdp.h>

const char* ssid = "BB30";
const char* password = "bb302445";
const char* host = "esp8266-webupdate";
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";
ESP8266WebServer webserver(80);       // instantiate server at port 80 (http port)
ESP8266HTTPUpdateServerCustom httpUpdater;
WiFiUDP udp;                        // A UDP instance to let us send and receive packets over UDP
unsigned int localPort = 123;      // local port to listen for UDP packets
IPAddress timeServerIP(158, 144, 1,37);          // tifr NTP server address
const char* ntpServerName = "ntp.tifr.res.in";//"time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
unsigned long ISTtime;
const int segment_pins[7] = {16,4,12,13,2,5,15};   // pin numbers corresponding to segments a,b,c,d,e,f,g
const int numeral[10] = {      // binary array to preserve the leftmost 0 or else code will ignore it
   //abcdefg/dp (not using DP hence set to 0
   B00000010, // 0    
   B10011110, // 1
   B00100100, // 2
   B00001100, // 3
   B10011000, // 4
   B01001000, // 5
   B01000000, // 6
   B00011110, // 7
   B00000000, // 8
   B00001000, // 9
};
const int digit_pins[4] = {14,0,1,3};          // pin numbers corresponding to the digits d1,d2,d3,d4 
//int p = ;
int randNumber;
unsigned long timer1, timer2, timer3;
String page = "";

void setup() {
  for(int i=0;i<=6;i++){
    pinMode(segment_pins[i], OUTPUT);
  }
  for(int i=0;i<=3;i++){
    pinMode(digit_pins[i], OUTPUT);
  }
  
//  pinMode(p, OUTPUT);
//  Serial.begin(9600);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid,password);
//  Serial.println("Connecting...");
  delay(30);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
//    Serial.print(".");
  }
//  Serial.println("");
//  Serial.print("Connected to ");
//  Serial.println(ssid);
//  Serial.print("IP address: ");
//  Serial.println(WiFi.localIP());
  long rssi = WiFi.RSSI();
//  Serial.print("RSSI:");
//  Serial.println(rssi);
  webserver.on("/", SendData);
  webserver.on("/getIP", SendIP);
  webserver.begin();
//  Serial.println("HTTP server started!");
  udp.begin(localPort);
  MDNS.begin(host);
  httpUpdater.setup(&webserver, update_path, update_username, update_password);
  MDNS.addService("http", "tcp", 80);
//  Serial.println("HTTPUpdateServer ready!");
//  Serial.printf("Open http://%s.local%s or http://", host, update_path);
//  Serial.print(WiFi.localIP());
//  Serial.printf("%s in your browser\n", update_path);
//  Serial.printf("and login with username '%s' and password '%s'\n", update_username, update_password);
}

void loop() {
  webserver.handleClient();
  MDNS.update();
//  int count = 0;
//  randNumber = random(0,10000);
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
//  delay(1000);
  timer2 = millis();                 //async wait to see if a reply is available (display the earlier time)
  while(millis() - timer2 < 1000){
    num2Digit(randNumber);
  }
  
  int cb = udp.parsePacket();
  if (!cb) {
//    Serial.println("no packet yet");
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
    ISTtime = epoch + (5*60+30)*60;    // IST is UTC + 5.30
    randNumber = getHours(ISTtime)*100 +  getMinutes(ISTtime);
  }
    
//  Serial.println("............");
  timer1 = millis();
  while(millis() - timer1 < 5000){
  num2Digit(randNumber);
//  count += 1;
 }
// Serial.println(count);

//  timer3 = millis();
//  while(millis() - timer3 < 1000){
//  clearLEDs();
// }

}

void clearLEDs(){ //clear the 7-segment display screen
  for(int i=0;i<=6;i++){
    digitalWrite(segment_pins[i], HIGH);
  }
//  digitalWrite(p, HIGH);
}

void num2Digit(int num){
  for(int i=3;i>=0;i--){
     //  clearLEDs();
    showdigit(num%10,i);
    num = num/10;
  }
}
void showdigit(int x, int digit){
  for(int j=0;j<=3;j++){
    digitalWrite(digit_pins[j], LOW);
  }
    digitalWrite(digit_pins[digit], HIGH);
  bool pinstate;  
  for(int k=1;k<=7;k++){
     if(bitRead(numeral[x],k)==1){      // reads right most bit
        pinstate = HIGH;
     }
     else{
        pinstate = LOW;
     }
     digitalWrite(segment_pins[7-k], pinstate);
  }
  delay(1);    
}

void SendData() {
  page = "{";
  page += "\"Random number\" : \"" + String(randNumber) + "\"";
  page += "}";
  webserver.sendHeader("Access-Control-Allow-Origin", "*", true);
  webserver.send(200, "application/json", page);
}

void SendIP() {
  page = "{\"IP\" : \"" + WiFi.localIP().toString() + "\"}";
  webserver.sendHeader("Access-Control-Allow-Origin", "*", true);
  webserver.send(200, "application/json", page);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address){
  
  Serial.println("sending NTP packet...");
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
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
//  return UNIXTime / 3600 % 24;    // 24-hour format
  return UNIXTime / 3600 % 12;    // 12-hour format
}
