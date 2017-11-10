/*
 * Time_NTP.pde
 * Example showing time sync to NTP time source
 *
 * This sketch uses the ESP8266WiFi library
 */
 
#include <TimeLib.h> 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
//
//const char ssid[] = "*************";  //  your network SSID (name)
//const char pass[] = "********";       // your network password

char ssid[] = " ";//  your network SSID (name)
char pass[] = " "; 
// NTP Servers:
IPAddress timeServer(129, 6, 15, 28); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov


const int timeZone = 2;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

#include "SSD1306.h"
#include <Wire.h>
#include <Arduino.h>

#define SDA_PIN D3
#define CLK_PIN D5
SSD1306  display(0x3c, D3, D5);

const int compresionPin = D7;
const int filterPin = D6;
const int lightsWhitePin = D2;
const int waterLevelPin = A0;
const int inputPompPin = D8;
//const int lightsUPin = D4;

//The bigger waterLevelPin value the least water left in tank
const int minWaterLevel = 0;
const int maxWaterLevel = 1;



WiFiUDP Udp;
unsigned int localPort = 2390;  // local port to listen for UDP packets

void DrawTextOLED(String  text, int delayIn = 0) {

	display.clear();
	String str = String(text);
	display.drawStringMaxWidth(0, 0, 128, str);
	//display.println(str);
	display.display();
	if (delayIn == 0)
	{
		delay(10);
	}
	else {
		delay(delayIn);
	}
}


void DrawStatusTextOLED() {

	display.clear();
	String time = String(getTimeStr());
	String pins = ((digitalRead(compresionPin) == HIGH) ? "\n Compression \n" : "");
	pins += (digitalRead(filterPin) == HIGH) ? " Filtration " : "";
	pins += (digitalRead(lightsWhitePin) == HIGH) ? " Lights " : "";
	//pins += " Water Level \n " + String(analogRead(waterLevelPin));
	display.drawStringMaxWidth(0, 0, 128, time + pins);
	display.display();
	delay(10);
}

void setup() 
{
  Serial.begin(9600);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  pinMode(compresionPin, OUTPUT);
  pinMode(filterPin, OUTPUT);
  pinMode(lightsWhitePin, OUTPUT);
  //pinMode(waterLevelPin, INPUT);
  //pinMode(inputPompPin, OUTPUT)
  delay(250);
  DrawTextOLED("Auto Aqua 1.0 Loading",2000);
  DrawTextOLED("Connecting to ",1000);
  DrawTextOLED(ssid);
  WiFi.begin(ssid, pass);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
	counter++;
    delay(500);
	String s = "";
	for (int i = 0; i < counter; i++)
	{
		s += ".";
	}
	DrawTextOLED("Setup wifi" + s);
  }

  
  Serial.println(WiFi.localIP());
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
}

time_t prevDisplay = 0; // when the digital clock was displayed
int lastMillis = 0;

void loop()
{  
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      tick();  
    }
  }
  if (millis() - lastMillis > 10000)
  {
	  restartWebServices();
  }
} 

void tick(){
	if (hour() >= 8 && hour() < 18)
	{
		
		if (digitalRead(lightsWhitePin) != HIGH)
		{
			digitalWrite(lightsWhitePin, HIGH);
			delay(10);
		}
		/*if (digitalRead(compresionPin) != HIGH)
		{
			digitalWrite(compresionPin, HIGH);
			delay(10);
		}*/
		if (digitalRead(filterPin) != HIGH)
		{
			digitalWrite(filterPin, HIGH);
			delay(10);
		}
		if (minute() == 5 || minute() == 35)
		{
			if (digitalRead(compresionPin) != HIGH)
			{
				digitalWrite(compresionPin, HIGH);
			}
		}
		else if (minute() == 10 || minute() == 40)
		{
			if (digitalRead(compresionPin) != LOW)
			{
				digitalWrite(compresionPin, LOW);
			}
		}
	}
	else {
		digitalWrite(lightsWhitePin, LOW);
		delay(10);
		digitalWrite(compresionPin, LOW);
		delay(10);
		if (minute() == 5 || minute() == 35)
		{
			if (digitalRead(filterPin) != HIGH)
			{
				digitalWrite(filterPin, HIGH);
			}
		}
		else if (minute() == 10 || minute() == 40)
		{
			if (digitalRead(filterPin) != LOW)
			{
				digitalWrite(filterPin, LOW);
			}
		}
	}
	DrawStatusTextOLED();
	delay(5000);
	lastMillis = millis();
}

String getTimeStr() {
	// digital clock display of the time
	String time = String(hour());
	time += printDigits(minute());
	time += printDigits(second());
	time += " ";
	time += day();
	time += ".";
	time += month();
	time += ".";
	time += year();
	return time;
}



String printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  String s = ":";
  if(digits < 10)
    s += "0";
   s += String(digits);
   return s;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void RefillTank() {
	while (analogRead(waterLevelPin) < maxWaterLevel)
	{
		if (digitalRead(inputPompPin) == LOW)
		{
			digitalWrite(inputPompPin, HIGH);
			delay(100);
		}
	}
	digitalWrite(inputPompPin, LOW);
	delay(10);
}

void restartWebServices() {
	Udp.stopAll();
	WiFi.disconnect();
	WiFi.begin(ssid, pass);
	int counter = 0;
	while (WiFi.status() != WL_CONNECTED) {
		counter++;
		delay(500);
		String s = "";
		for (int i = 0; i < counter; i++)
		{
			s += ".";
		}
		DrawTextOLED("Setup wifi" + s);
	}
	Udp.begin(localPort);
	delay(10);
	setSyncProvider(getNtpTime);
	delay(10);
}

