
// *****************************************************************************
// * Amp1_Esp8266_Server                                                       *
// *    By: Tony Gargasz - tony.gargasz@gmail.com                              *
// *    WiFi Controller for changing effect modes in the AMP1 guitar amplifier *
// *    R0 - Initial development                                               *
// *    R1.2 - Changed from I2C to SPI communication type                      *
// *    R1.3 - Added Wifi comms                                                *
// *    R1.4 - Add code for D2 (espHasData) handshake to push data to Daisy    *
// *    R1.5 - Comment out 'Serial.print' statements and convert to AP mode    *
// *****************************************************************************

/*  SPI Slave - Connect Daisy to ESP8266 (SS is not used):
	GPIO   NodeMCU   Name     Color   Daisy
	=========================================
	 4       D2   espHasData   Grn    D11   
	13       D7      MOSI      Blk    D10 
	12       D6      MISO      Red    D9  
	14       D5      SCK       Wht    D8
	Note: espHasData used as ESP8266 output to tell Daisy it has new setings */

#include <ESP8266WiFi.h>
#include <SPISlave.h>
#include <StringSplitter.h>

// Use for AP Mode
const char* ssid = "GCA-Amp1";
const char* password = "123456789";

// Use for Debug Wifi 
// const char* ssid = "KB8WOW";
// const char* password = "4407230350";

// Global vars
boolean byp;
int Mode_Num;
const int bdLed = 16;           // MCU Board LED
const int espHasData = 4;       // Connect pin D2 (GPIO4) to D11 on Daisy
float delay_adj, feedback_adj;  // Delay Line (echo) vars 
float rate_adj, depth_adj;      // Tremelo vars
float gain_adj, mix_adj;        // Distortion vars

// To set timeout period
unsigned long currentMillis;
unsigned long nextMillis = 0;
const long interval = 300;  // interval at which to timeout

// Create an instance of the server
WiFiServer server(80);

void setup() 
{
	Serial.begin(115200);
	delay(10);

	// Used to signal Daisy to receive data from ESP
	pinMode(espHasData, OUTPUT);
	pinMode(bdLed, OUTPUT);
	
	byp = false;
	Mode_Num = 1;
	delay_adj = 24000.00;
	rate_adj = 5.00;
	gain_adj = 6.50;
	feedback_adj = 0.50;
	depth_adj = 0.50;
	mix_adj = 0.10;
	digitalWrite(espHasData, LOW);
	digitalWrite(bdLed, LOW);
	
	// ~~~ Start Wifi Access Point ~~~
	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid, password);

	/*
	// ~~~ Start of (Debug) WiFi Setup ~~~
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);
	
	WiFi.begin(ssid, password);
	
	while (WiFi.status() != WL_CONNECTED) 
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	*/
	
	// Start the server
	server.begin();
	Serial.println("Server started");
	// ~~~ End of WiFi Setup ~~~
	
	// Data Receive from Daisy
	SPISlave.onData(receiveData);
	
	// Start SPI Slave 
	SPISlave.begin();
}

//Receive SPI data from Daisy
// Note: len is always 32 (buffer is auto-filled with zeroes)
void receiveData(uint8_t * data, size_t len)
{    
	Serial.printf("From Daisy: %s\n", (char *)data); 
	String message = String((char *)data);
	String tmp = "";
	(void) len;

	if (message.equals("Keep Alive")) 
	{
		// Keep Alive (Debug)
		char answer[33];
		sprintf(answer, "Kept Alive for %lu seconds!", millis() / 1000);
		Serial.print("Ans Msg is: ");
		Serial.println(answer);
	}
	else if (message.startsWith("R")) 
	{
		tmp = message.substring(2, 3);
		Mode_Num = strtod(tmp.c_str(), NULL);
		tmp = message.substring(3, 13);
		delay_adj = strtof(tmp.c_str(), NULL);
		tmp = message.substring(13, 20);
		rate_adj = strtof(tmp.c_str(), NULL);
		tmp = message.substring(20, 27); 
		gain_adj = strtof(tmp.c_str(), NULL);
	}
	else if (message.startsWith("D")) 
	{
		tmp = message.substring(2, 3);
		Mode_Num = strtod(tmp.c_str(), NULL);
		tmp = message.substring(3, 9);
		feedback_adj = strtof(tmp.c_str(), NULL);
		tmp = message.substring(9, 15);
		depth_adj = strtof(tmp.c_str(), NULL);
		tmp = message.substring(15, 21); 
		mix_adj = strtof(tmp.c_str(), NULL);
	}
	else if (message.startsWith("M")) 
	{
		tmp = message.substring(2, 3);
		Mode_Num = strtod(tmp.c_str(), NULL);
	}
	else if (message.startsWith("S")) 
	{
		// Daisy ready to receive new settings
		char myData[1] = {0};
		// Send Depth values
		myData[0] = 'D';
		sendSettings(myData);
		digitalWrite(espHasData, LOW);
		digitalWrite(bdLed, LOW);
	}
}

// Send (SPI) Settings to Daisy
void sendSettings(const char *msgType)
{
	// D - Depth  R - Rate
	char sndData[32] = {0};
	if (msgType[0] == 'R') 
	{
		sprintf(sndData, "R %1d %9.3f %6.3f %6.3f", Mode_Num, delay_adj, rate_adj, gain_adj);
		SPISlave.setData(sndData);
	}
	else if (msgType[0] == 'D') 
	{
		sprintf(sndData, "D %1d %5.3f %5.3f %5.3f", Mode_Num, feedback_adj, depth_adj, mix_adj);
		SPISlave.setData(sndData);
	}
	// Debug
	Serial.printf("To Daisy: %s\n", (char *)sndData);
}

void loop() 
{
	// Timeout period is 300 ms and can be adjusted with var: interval 
	currentMillis = millis();
	if (currentMillis == nextMillis)
	{
		if (digitalRead(espHasData))
		{
			// Set handshake low (in case Daisy never responded to data push)
			digitalWrite(espHasData, LOW);
			digitalWrite(bdLed, LOW);
			Serial.println("Daisy did not respond in a timely fashion");
		}
	}

	// Check if a client has connected
	WiFiClient client = server.available();
	if (!client) 
	{
		return;
	}
  
	// Wait until the client sends some data
	Serial.println("new client");
	while (!client.available()) 
	{
		delay(1);
	}
  
	// Read the first line of the request
	String req = client.readStringUntil('\r');
	Serial.println(req);
	client.flush();

	// *************** HTTP Requests ***************
	// getsets - Gets new settings from phone
	// putsets - Puts the current settings to phone
	if (req.indexOf("/getsets") != -1)
	{
		String item = "";
		StringSplitter *splitter = new StringSplitter(req, ' ', 10);
		int itemCount = splitter->getItemCount();
		if (itemCount >= 9)
		{
			item = splitter->getItemAtIndex(2);
			Mode_Num = strtod(item.c_str(), NULL);
			item = splitter->getItemAtIndex(3);
			rate_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(4);
			depth_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(5);
			delay_adj =strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(6);
			feedback_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(7);
			gain_adj = strtof(item.c_str(), NULL);
			item = splitter->getItemAtIndex(8);
			mix_adj = strtof(item.c_str(), NULL);

			// Push data to Daisy through SPI
			char myData1[1] = {0};
			// Send Rate values
			myData1[0] = 'R';
			sendSettings(myData1);
			digitalWrite(espHasData, HIGH);
			digitalWrite(bdLed, HIGH);
			// Daisy will return 'S' when ready to receive
			// Set the timeout timer
			nextMillis = currentMillis + interval;
		}
		else
		{
			Serial.println(" Incomplete request");
			client.stop();
			return;   
		}
	}
	else if (req.indexOf("/putsets") != -1)
	{
		if (digitalRead(espHasData))
		{
			// Set handshake low (in case Daisy never responded to data push)
			digitalWrite(espHasData, LOW);
			digitalWrite(bdLed, LOW);
			Serial.println("Daisy did not respond in a timely fashion");
		}
		
		// Fall through as a valid HTTP request
		// Just sends the current settings to phone
		// (only happens when phone app cold starts)  
	}
	else 
	{
		// Set handshake low (in case Daisy never responded to data push)
		digitalWrite(espHasData, LOW);
		digitalWrite(bdLed, LOW);
		
		Serial.println("Invalid request");
		client.stop();
		return;
	}
	client.flush();

	// Prepare the response 
	char wifiData[64] = {0};
	sprintf(wifiData, "W %1d %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f", +
		Mode_Num, delay_adj, rate_adj, gain_adj, feedback_adj, depth_adj, mix_adj);
	String resp = "HTTP/1.1 200 OK\r\n";
	resp += "Content-Type: text/html\r\n\r\n";
	resp +=  wifiData;
	resp += "\r\n\r\n";
		 
	// Send the response to the client
	client.print(resp);
	delay(1);

	// Debug
	Serial.printf("To Phone: %s\n", (char*)wifiData);

	// --- Client gets automatically disconnected and destroyed ---
}
