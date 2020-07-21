## Usage Instructions

This folder consists of some ESP8266 related codes.

## Installation

1. Install the Arduino IDE from https://www.arduino.cc/en/Main/Software based on the operating system of your computer if it is not installed in your system.
2. Start __Arduino__ application and open __File > Preferences__ window.
3. Enter https://arduino.esp8266.com/stable/package_esp8266com_index.json into __Additional Board Manager URLs__ field.
4. Open Boards Manager from __Tools > Board__ menu and search for esp8266.
5. There will be one option by __ESP8266 community__. Select the latest version from the drop-down box and click install button.

## Using the codes

1. After downloading the repository go to the particular folder and open the main code __"......ino"__ file.
2. Connect the esp8266 based board to your computer via a usb cable.
3. Choose the correct port from __Tools > Port__. In case of windows operating system you can get the port no. from Device Manager > Ports. Look for the one which mentions usb-serial.
4. Go back to arduino IDE and select your ESP8266 board from __Tools > Board__ menu after installation.
5. In the expressions of __const char* ssid__ and __const char* password__ respectively, change the name and password of the WiFi network you want to connect to (inside the double        quotes).
6. Upload the code from __Sketch > Upload__ option.
7. If there is no error, the uploading will finish and arduino IDE will show a message something like __"Leaving...Hard resetting via RTS pin"__ in the bottom black panel and reset      the board. This is not an error. It means your code is uploaded succesfully to the ESP module.
8. Without disconnecting the ESP module from computer open the serial monitor from __Tools > Serial Monitor__ to know the IP address of the ESP module. If nothing comes up, press the reset button on the ESP module and read what comes up in the serial monitor. Alternatively you can also download any android application which supports local      mDNS, e.g. "BonjourBrowser" and get the IP address corresponding to the local host __"esp8266-webupdate"__. For this to work your smartphone must be connected to the same WiFi network as    the ESP module.
9. If you want to make changes to the code (e.g. change wifi name and password, acquisition rate in seconds etc.) you can change the required things in the code and either upload it via connecting the ESP module to the computer same as the initial setup (points 1 - 8) or you can do it via OTA (over the air) update through WiFi network without connecting    physically to the computer. Read below for OTA update.  
10. Compile the sketch from __Sketch > export compiled binary__ option. It will create a binary (.bin) file in the same folder as your code. Now get the IP address of the module from the android application mentioned in 8. If the wifi information is not changed and your WiFi router uses static IP, then IP address will remain unchanged. Now go to http://xxx.xxx.xxx.xxx/update, where xxx.xxx.xxx.xxx is the IP address. You will be required to use a username and password if you are going to open this link for the first time in your computer. Default username and password are __"admin"__ and __"admin"__ respectively. If needed you can also change these in the code (__const char* update_username__ and __const char* update_password__ expressions). Click on __"choose your file"__ button to browse to the location of the .bin file generated and select it. Click on the __"update firmware"__ button and wait for it to upload. If the upload is done successfully, a message will be displayed __"Update Success! Rebooting..."__
11. If you want to use multiple ESP modules in the same WiFi network, change the host name (__const char* host__) in the code to uniquely identify each modules for getting their IP addresses.
12. By mistake if you upload wrong WiFi name or password it will not connect to the WiFi network after next reboot. If there is no way to connect it to a computer because of any reason there is a option to correct it in the code. Create a WiFi hotspot from your smartphone named __"UniqueIdentifier_your WiFi ssid_your WiFi password"__, where put your correct WiFi name and password in the respective positions. The ESP module will scan the available WiFi networks and get the correct WiFi name and password to connect. Once it is connected to the network, change the correct Wifi name or password in the Arduino IDE and recompile and upload via OTA (step 10).
