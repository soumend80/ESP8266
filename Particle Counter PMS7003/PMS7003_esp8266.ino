#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

// Replace with your network credentials
const char* ssid = "BB30";
const char* password = "bb302445";
//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 3
ESP8266WebServer webserver(80);   //instantiate server at port 80 (http port)
WiFiServer telnetserver(23);         //Telnet server at port 23
WiFiClient serverClients[MAX_SRV_CLIENTS];
String page = "";
unsigned long previousMillis = 0;        // will store last temp was read
unsigned long elapsedMillis = 0;
unsigned long timer = 0;
unsigned long time_interval = 10000;     // collect and send data after 10 seconds
float timepassed = 0;
bool flag = false;
int ledPin = 2;
bool ledState = LOW;
#define LENGTH 32       // length of data
char startByte1 = 0x42;
char startByte2 = 0x4d;
uint16_t DATA[16] = {};
char buf[LENGTH];
char buf2[LENGTH];
uint16_t PM1_CF, PM2_5_CF, PM10_CF, PM1_ATM, PM2_5_ATM, PM10_ATM  ; // particle values 1.0, 2.5, 10 in micrograms per m^3
uint16_t NP_0_3, NP_0_5, NP_1_0, NP_2_5, NP_5_0, NP_10_0;

void setup() {
  Serial.begin(9600); // opens Serial port, sets data rate to 9600 bps
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); //begin WiFi connection
  Serial.println("");
  delay(30);

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
  telnetserver.begin();
  telnetserver.setNoDelay(true);
  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");
  webserver.on("/", SendData);
  webserver.begin();
  Serial.println("HTTP server started!");
  previousMillis = millis();
}

void loop() {
  webserver.handleClient();

  // send data only when you receive data:
  if ((millis() - timer) > time_interval) {

    int x = 0;
    uint8_t start1, start2;
    PM1_CF = PM2_5_CF = PM10_CF = PM1_ATM = PM2_5_ATM = PM10_ATM = 0;
    NP_0_3 = NP_0_5 = NP_1_0 = NP_2_5 = NP_5_0 = NP_10_0 = 0;

    if (Serial.available() > 0) {

      while (Serial.available() > 0) {
        buf[x] = Serial.read();
        //Serial.println(buf[x],HEX);
        x++;
        if (x == LENGTH) {
          break;
        }
      }

    }


    for (int i = 0; i < LENGTH; i++) { // copy data buffer
      buf2[i] = buf[i];
    }

    for (int i = 0; i < LENGTH; i++) { // find start bytes position

      if (buf[i] == startByte1) {

        if ( buf[ ( i + 1) % LENGTH ] == startByte2) {
          start1 = i;
          start2 = buf[ ( i + 1) % LENGTH];
          break;
        }
      }
    }

    for (int i = 0; i < LENGTH; i++) {
      buf[i] = buf2[(start1 + i) % LENGTH];
    }

    if (buf[0] == startByte1) {
      for ( uint8_t i = 0; i <= 12; i++) {
        DATA[i] = ((buf[i * 2 + 2] << 8) + buf[i * 2 + 3] );
      }
      PM1_CF = DATA[1];
      PM2_5_CF = DATA[2];
      PM10_CF = DATA[3];
      PM1_ATM = DATA[4];
      PM2_5_ATM = DATA[5];
      PM10_ATM = DATA[6];
      NP_0_3 = DATA[7];
      NP_0_5 = DATA[8];
      NP_1_0 = DATA[9];
      NP_2_5 = DATA[10];
      NP_5_0 = DATA[11];
      NP_10_0 = DATA[12];

    }

    //  Serial.println("");
    //  Serial.print("Connected to ");
    //  Serial.println(ssid);
    //  Serial.print("IP address: ");
    //  Serial.println(WiFi.localIP());
    //  Serial.println("PM1_CF : " + String(PM1_CF));
    //  Serial.println("PM2_5_CF : " + String(PM2_5_CF));
    //  Serial.println("PM10_CF : " + String(PM10_CF));
    //  Serial.println("PM1_ATM : " + String(PM1_ATM));
    //  Serial.println("PM2_5_ATM : " + String(PM2_5_ATM));
    //  Serial.println("PM10_ATM : " + String(PM10_ATM));
    //  Serial.println("NP_0_3 : " + String(NP_0_3));
    //  Serial.println("NP_0_5 : " + String(NP_0_5));
    //  Serial.println("NP_1_0 : " + String(NP_1_0));
    //  Serial.println("NP_2_5 : " + String(NP_2_5));
    //  Serial.println("NP_5_0 : " + String(NP_5_0));
    //  Serial.println("NP_10_0 : " + String(NP_10_0));
    //  Serial.println(".........................");

    elapsedMillis = millis() - previousMillis;

    uint8_t i;
    //check if there are any new clients
    if (telnetserver.hasClient()) {
      for (i = 0; i < MAX_SRV_CLIENTS; i++) {
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected()) {
          if (serverClients[i]) serverClients[i].stop();
          serverClients[i] = telnetserver.available();
          Serial.print("New client: "); Serial.println(i + 1);
          flag = true;
          break;
        }
      }
    }

    timepassed = float(elapsedMillis) / 1000;
    String str = String(timepassed) + "\t" + String(PM1_CF) + "\t" + String(PM2_5_CF) + "\t" + String(PM10_CF);
    str += "\t" + String(PM1_ATM) + "\t" + String(PM2_5_ATM) + "\t" + String(PM10_ATM);
    str += "\t" + String(NP_0_3) + "\t" + String(NP_0_5) + "\t" + String(NP_1_0);
    str += "\t" + String(NP_2_5) + "\t" + String(NP_5_0) + "\t" + String(NP_10_0);
    //push data to all connected telnet clients
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        serverClients[i].println(str);
        delay(1);
      }
    }

    timer = millis();
  }
}

void SendData() {

  page = "{";
  page += "\"Time_passed\" : \"" + String(elapsedMillis) + " milliseconds" + "\",";
  page += "\"PM1.0_CF\" : \"" + String(PM1_CF) + "\",";
  page += "\"PM2.0_CF\" : \"" + String(PM2_5_CF) + "\",";
  page += "\"PM10_CF\" : \"" + String(PM10_CF) + "\",";
  page += "\"PM1.0_ATM\" : \"" + String(PM1_ATM) + "\",";
  page += "\"PM2.5_ATM\" : \"" + String(PM2_5_ATM) + "\",";
  page += "\"PM10_ATM\" : \"" + String(PM10_ATM) + "\",";
  page += "\"NP_0.3\" : \"" + String(NP_0_3) + "\",";
  page += "\"NP_0.5\" : \"" + String(NP_0_5) + "\",";
  page += "\"NP_1.0\" : \"" + String(NP_1_0) + "\",";
  page += "\"NP_2.5\" : \"" + String(NP_2_5) + "\",";
  page += "\"NP_5.0\" : \"" + String(NP_5_0) + "\",";
  page += "\"NP_10\" : \"" + String(NP_10_0) + "\"";
  page += "}";
  webserver.sendHeader("Access-Control-Allow-Origin", "*", true);
  webserver.send(200, "application/json", page);

}
