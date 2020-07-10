//===================================================================================================
/*
  This code reads data from Plantower PMS 7003 air quality sensor and sends it to a local HTTP server.
  Be sure to update the correct SSID and PASSWORD before running to allow connection to your WiFi 
  network.

  Please refer to the readme.md file at https://github.com/shescitech/TIFR_Mask_Efficiency for 
  detailed instructions.
*/
//===================================================================================================
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

//===================================================================================================
//                Replace with your network credentials (IMPORTANT)
//===================================================================================================
const char* ssid = "BB30";
const char* password = "bb302445";
//===================================================================================================
const char* host = "esp8266-webupdate";
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";
const char* hotspot_id = "PMS7003";   // Alterntive Hotspot identifier
#define MAX_SRV_CLIENTS 3             // how many clients should be able to telnet to this ESP8266
#define LENGTH 32                     // length of data to be transmitted
ESP8266WebServer webserver(80);       // instantiate server at port 80 (http port)
ESP8266HTTPUpdateServer httpUpdater;
WiFiServer telnetserver(23);              // Telnet server at port 23
WiFiClient serverClients[MAX_SRV_CLIENTS];
String page = "";
unsigned long previousMillis = 0;         // will store last temp was read
unsigned long elapsedMillis = 0;
unsigned long timer = 0;
unsigned long time_interval = 20000;      // collect and send data after 20 seconds
float timepassed = 0;
bool flag = false;
int ledPin = 2;
bool ledState = LOW;
char startByte1 = 0x42;
char startByte2 = 0x4d;
uint16_t DATA[16] = {};                   // will store data
char buf[LENGTH];
char buf2[LENGTH];
int count = 0;

// particle concentration in micrograms per m^3 (CF=1, standard particle)
uint16_t PM1_CF, PM2_5_CF, PM10_CF;

// particle concentration in micrograms per m^3 (atmospheric environment)
uint16_t PM1_ATM, PM2_5_ATM, PM10_ATM ;

//number of particles with diameter beyond 0.3, 0.5, 1.0, 2.5, 5.0 and 10 um in 0.1 L of air
uint16_t NP_0_3, NP_0_5, NP_1_0, NP_2_5, NP_5_0, NP_10_0;

//===================================================================================================
//                    Power on setup
//===================================================================================================
void setup() {
  Serial.begin(9600); // opens Serial port, sets data rate to 9600 bps
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password); //begin WiFi connection
  Serial.println("");
  delay(30);

  // Wait for connection, if not found search for alternative hotspot to get the correct ssid and password
  Serial.println("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    count = count + 1;
    if (count > 60 && count < 70) {
      Serial.println("Searching for Alternative Wifi...");
      ScanForWifi();
    }
    if (count >= 70) {
      Serial.println("Searching for Default Wifi... ");
      //      WiFi.begin(ssid, password);
      count = 0;
    }
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  telnetserver.begin();
  telnetserver.setNoDelay(true);
  Serial.print("Telnet server started! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");
  webserver.on("/", SendData);
  webserver.on("/getIP", SendIP);
  webserver.begin();
  Serial.println("HTTP server started!");
  MDNS.begin(host);
  httpUpdater.setup(&webserver, update_path, update_username, update_password);
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTPUpdateServer ready!");
  Serial.printf("Open http://%s.local%s or http://", host, update_path);
  Serial.print(WiFi.localIP());
  Serial.printf("%s in your browser\n", update_path);
  Serial.printf("and login with username '%s' and password '%s'\n", update_username, update_password);
  previousMillis = millis();
}

//===================================================================================================
//                    Main Program Loop
//===================================================================================================
void loop() {
  webserver.handleClient();
  MDNS.update();

  // send data only after required time is passed
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

    for (int i = 0; i < LENGTH; i++) {        // copy data buffer
      buf2[i] = buf[i];
    }

    for (int i = 0; i < LENGTH; i++) {        // find start bytes position

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

//===================================================================================================
//                    Function for sending the data to http client
//===================================================================================================
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

//===================================================================================================
//               Function for sending the IP address of this ESP8266 to http client
//===================================================================================================
void SendIP() {

  page = "{\"IP\" : \"" + WiFi.localIP().toString() + "\"}";
  webserver.sendHeader("Access-Control-Allow-Origin", "*", true);
  webserver.send(200, "application/json", page);

}

//===================================================================================================
//                Function for scanning avialble Wifi SSID
//===================================================================================================
void ScanForWifi()
{
  Serial.print("Scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  for (int i = 0; i < n; i++)
  {
    Serial.println(WiFi.SSID(i));
    CheckWifiSSIDPassword(WiFi.SSID(i));
  }
  Serial.println();

  delay(5000);
}

//===================================================================================================
// Function for searching alternative Wifi hotspot to retrieve the correct Wifi ssid and password
//===================================================================================================
void CheckWifiSSIDPassword(String str)
{

  int indexoffirst = str.indexOf('_');
  String chunk1 = str.substring(0, indexoffirst);

  if (chunk1 == hotspot_id) {
    int indexofsecond = str.indexOf('_', indexoffirst + 1);
    String chunk2 = str.substring(indexoffirst + 1, indexofsecond);
    String chunk3 = str.substring(indexofsecond + 1, str.length());

    int ssid_len = chunk2.length() + 1;
    int password_len = chunk3.length() + 1;

    char char_array2[ssid_len];
    char char_array3[password_len];
    chunk2.toCharArray(char_array2, ssid_len);
    chunk3.toCharArray(char_array3, password_len);
    ssid = char_array2;
    password = char_array3;
    Serial.print("use ssid : ");
    Serial.println(ssid);
    Serial.print("use password : ");
    Serial.println(password);
    WiFi.begin(ssid, password);
    delay(30);
  }

}
