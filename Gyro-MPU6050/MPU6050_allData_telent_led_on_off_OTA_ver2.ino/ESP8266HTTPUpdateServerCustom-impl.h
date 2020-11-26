#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <flash_hal.h>
#include <FS.h>
#include "StreamString.h"
#include "ESP8266HTTPUpdateServerCustom.h"

namespace esp8266httpupdateservercustom {
using namespace esp8266webserver;

static const char serverIndex[] PROGMEM =
  R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
		 <style type='text/css'>
		 body{
			 margin:0px;
		 }
		 .he{
			     text-align: center;
				font-size: 1.5rem;
				padding: 10px 0px;
				background-color: rgba(0,0,0,0.6);
				color: white;
			}
			 
		 
		 .wr{
			 background-color: rgba(10,200,255,0.8);
			width: 100%;
			height: 100vh;
			display: flex;
			align-items: center;
			justify-content: center;
		 }
		 .block{
			width: 350px;
			height: 350px;
			background-color: white;
			border-radius: 10px;
			box-shadow: 0 1px 6px rgba(32, 34, 36, 0.3);
		 }
		 .block-items{
			 display: flex;
			align-items: center;
			justify-content: center;
			height: 100%;
		 }
		 .inpu{
			 margin-right: 20px;
			display: flex;
			margin-left: 20px;
			margin-top: 20px;
			margin-bottom: 20px;
		 }
		 .file-sel{
			     display: flex;
				flex-direction: column;
		 }
		 .submi{
			padding-left: 10px;
			padding-right: 10px;
			padding-top: 20px;
			padding-bottom: 20px;
			background-color: rgba(10,200,255,0.8);
			color: white;
			font-size: 1.3rem;
			display: block;
			border: none;
			border-radius: 5px;
			cursor:pointer;
			
		 }
		 
		 
			.inpudiv{
				border-style:solid;
				text-align:center;
				padding-left: 10px;
				padding-right: 10px;
				padding-top:10px;
				padding-bottom:10px;
				border-width:1px;
				border-radius:5px;
				cursor:pointer;
				padding-left:10px;
				background-color:rgba(0,0,0,0.3);
				font-size:1.3rem;
				display:block;
			}
			.inpufile{
				display:none;
				
			}
		 </style>
     </head>
	 <script type='text/javascript'>
	 function hello(){document.getElementsByClassName('inpufile')[0].click();}
	 
	 function gello(){
		 var fileInput = document.getElementById('firmware');   
		var filename = fileInput.files[0].name;
		 document.getElementsByClassName('inpufileshow')[0].innerHTML=filename.substring(0, 20);
	 }
	 
	 function cholo(){
		 document.getElementById('my-form').submit();
	 }
	 </script>
     <body>
	 <div class='he'>OTA UPDATE</div>
     <div class='wr'>
		<div class='block'>
		 <div class='block-items'>
			
			<div class='file-sel b-items'>
				
				<form method='POST' action='' enctype='multipart/form-data' id='my-form'>
					<input class='inpu inpufile' onchange='gello()' type='file' accept='.bin,.bin.gz' id='firmware' name='firmware'>
					 <div class='inpu inpudiv' onclick='hello()'>Choose your file</div>
					 
					 <div class='inpu inpufileshow'></div>
					 <input class='inpu submi' type='button' value='Update Firmware' onclick='cholo()'>
				 </form>
					 
				
			</div>
			<div class='submit-div b-items'>
				<div class='submit-btn'></div>
			</div>
			
		 </div>
		</div>
	 </div>
     </body>
     </html>)" ;
static const char successResponse[] PROGMEM = 
  "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

template <typename ServerType>
ESP8266HTTPUpdateServerCustomTemplate<ServerType>::ESP8266HTTPUpdateServerCustomTemplate(bool serial_debug)
{
  _serial_output = serial_debug;
  _server = NULL;
  _username = emptyString;
  _password = emptyString;
  _authenticated = false;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerCustomTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), serverIndex);
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError()) {
        _server->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), successResponse);
        delay(100);
        _server->client().stop();
        ESP.restart();
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = _server->upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();
        if (_serial_output)
          Serial.setDebugOutput(true);

        _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
        if(!_authenticated){
          if (_serial_output)
            Serial.printf("Unauthenticated Update\n");
          return;
        }

        WiFiUDP::stopAll();
        if (_serial_output)
          Serial.printf("Update: %s\n", upload.filename.c_str());
        if (upload.name == "filesystem") {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          if (!Update.begin(fsSize, U_FS)){//start with max available size
            if (_serial_output) Update.printError(Serial);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) Serial.printf(".");
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          _setUpdaterError();
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
          if (_serial_output) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          _setUpdaterError();
        }
        if (_serial_output) Serial.setDebugOutput(false);
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        if (_serial_output) Serial.println("Update was aborted");
      }
      delay(0);
    });
}

template <typename ServerType>
void ESP8266HTTPUpdateServerCustomTemplate<ServerType>::_setUpdaterError()
{
  if (_serial_output) Update.printError(Serial);
  StreamString str;
  Update.printError(str);
  _updaterError = str.c_str();
}

};
