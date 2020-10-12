/*
Amp1_Rx.x.ino - Multiple audio effects for guitar amplifier
Copyright (C) 2020  By: Tony Gargasz

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses
*/

// ******************************************************************
// * Daisy Seed effects With Rate and Depth Knobs                   *
// *    By: Tony Gargasz - tony.gargasz@gmail.com                   *
// *    R1 - Optimized control interaction                          *
// *    R2 - Added Distortion and Tremelo effects                   *
// *    R3 - Clean up for release                                   *
// *    R3.1 - Added license for release into the wild              *
// *    R3.2 - Add I2C to ESP WiFi AP to make adjs with cell phone  *
// *    R3.3 - Remove I2C and add SPI to ESP8266 for WiFi app       *
// *    R3.4 - Remove SPI and add SoftwareSerial for Wifi app       *
// *    R3.5 - Changed back to SPI to ESP8266 for WiFi app          *
// *    R3.6 - Add comm routines to send settings back and forth    *
// *    R3.7 - Removed duplicate code in adjustSettings()           *
// *    R4.0 - Lockput knobs while under control by Wifi app        *
// ******************************************************************
#include <DaisyDuino.h>
#include <SPI.h>

DaisyHardware hw;

size_t num_channels;
boolean bypass_On = false;
boolean btnBypassEnabled = true;
boolean btnModeEnabled = true;
boolean espHasDataEnabled = true;
boolean modeChanged = false;
boolean phoneMode = false;

// General vars
int Mode_Num, Mode_Bits;
float sample_rate, out_buf, prev_knob1val, prev_knob2val, knob1val, knob2val;
// Delay Line (echo) vars
float wet, delay_adj, feedback_adj;
// Tremelo vars 
float rate_adj, depth_adj;
// Distortion vars
float fuzz_buf, gain_adj, mix_adj, fuzz_quo;
    
// Switches and LEDs
const int btnBypass = D1;   // Bypass Switch
const int btnMode = D2;     // Mode Switch
const int ledBypass = D3;   // Bypass Led (blue) 
const int ledMode3 = D4;    // Mode 3 Led (Distortion)
const int ledMode2 = D5;    // Mode 2 Led (Echo)
const int ledMode1 = D6;    // Mode 1 Led (Tremelo)
const int SelectPin = D7;   // Chip Select
const int espHasData = D11; // ESP8266 has a settings change (to D2 on ESP)

/*  SPI Slave - Connect Daisy to ESP8266 (SS is not used):
    GPIO   NodeMCU   Name     Color   Daisy
    =========================================
     4       D2   espHasData   Grn    D11   
    13       D7      MOSI      Blk    D10 
    12       D6      MISO      Red    D9  
    14       D5      SCK       Wht    D8
    Note: espHasData used as ESP8266 output to tell Daisy it has new setings */
    
// To set period to poll knobs and switchs
unsigned long currentMillis;
unsigned long previousMillis = 0;
const long interval = 100; // interval at which to poll (milliseconds)

// Set max delay time to 100% of samplerate (1 second)
#define MAX_DELAY static_cast<size_t>(48000)

// Instantiate Daisy Seed Effects
static DelayLine<float, MAX_DELAY> del;
static Oscillator osc_sine;

// SPI 
class ESPMaster 
{
    public:
        ESPMaster() {}
        void readData(uint8_t * data) 
        {
            SPI.transfer(0x03);
            SPI.transfer(0x00);
            for (uint8_t i = 0; i < 32; i++) 
            {
                data[i] = SPI.transfer(0);
            }
        }
        
        void writeData(uint8_t * data, size_t len) 
        {
            uint8_t i = 0;
            SPI.transfer(0x02);
            SPI.transfer(0x00);
            while (len-- && i < 32) 
            {
                SPI.transfer(data[i++]);
            }
            while (i++ < 32) 
            {
                SPI.transfer(0);
            }
        }

        String readData() 
        {
            char data[33];
            data[32] = 0;
            readData((uint8_t *)data);
            return String(data);
        }
        
        void writeData(const char * data) 
        {
            writeData((uint8_t *)data, strlen(data));
        }
};

// Instantiate SPI class
ESPMaster esp;

// Send settings to ESP8266 (event driven)
void sendData(const char * msgType) 
{
    digitalWrite(SelectPin, LOW);
    // D - Depth  R - Rate  M - Mode  K - Keep Alive  S - Settings from phone app
    char sndData[32] = {0};
    if (msgType == "R") 
    {
        sprintf(sndData, "R %1d %9.3f %6.3f %6.3f", Mode_Num, delay_adj, rate_adj, gain_adj);
        esp.writeData(sndData);
    }
    else if (msgType == "D") 
    {
        sprintf(sndData, "D %1d %5.3f %5.3f %5.3f", Mode_Num, feedback_adj, depth_adj, mix_adj);
        esp.writeData(sndData);
    }
    else if (msgType == "M") 
    {
        sprintf(sndData, "M %1d", Mode_Num);
        esp.writeData(sndData);
    }
    else if (msgType == "K") 
    {
        esp.writeData("Keep Alive"); // Only used during testing
    }
    else if (msgType == "S") 
    {
        esp.writeData("S");
    }
    digitalWrite(SelectPin, HIGH);
}

// Receive settings from ESP8266 (polled in Loop)
void receiveData(String message)
{
    String tmp = "";
    if (message.startsWith("R")) 
    {
        // Parse the values
        tmp = message.substring(2, 3);
        Mode_Num = strtod(tmp.c_str(), NULL);
        tmp = message.substring(3, 13);
        delay_adj = strtof(tmp.c_str(), NULL);
        tmp = message.substring(13, 20);
        rate_adj = strtof(tmp.c_str(), NULL);
        tmp = message.substring(20, 27); 
        gain_adj = strtof(tmp.c_str(), NULL);
        // Debug
        // !sendData("R");
    }
    else if (message.startsWith("D")) 
    {
        // Parse the values
        tmp = message.substring(2, 3);
        Mode_Num = strtod(tmp.c_str(), NULL);
        tmp = message.substring(3, 9);
        feedback_adj = strtof(tmp.c_str(), NULL);
        tmp = message.substring(9, 15);
        depth_adj = strtof(tmp.c_str(), NULL);
        tmp = message.substring(15, 21); 
        mix_adj = strtof(tmp.c_str(), NULL);
        // Debug
        // !sendData("D");
    }
    else if (message.startsWith("M")) 
    {
        // Parse the value
        tmp = message.substring(2, 3);
        Mode_Num = strtod(tmp.c_str(), NULL);
        // Debug
        // !sendData("M");
    }
}

void adjustSettings()
{
    // Set Mode and adjust LED color
    Mode_Bits = 0; // Set all bits to false
    digitalWrite(ledMode1, LOW); // Green
    digitalWrite(ledMode2, LOW); // Blue
    digitalWrite(ledMode3, LOW); // Red
    
    switch(Mode_Num)
    {
        case 1:
            // Tremelo: Green
            bitSet(Mode_Bits, 1);
            digitalWrite(ledMode1, HIGH);
            break;
        case 2:
            // Echo: Blue
            bitSet(Mode_Bits, 2);
            digitalWrite(ledMode2, HIGH);
            break;
        case 3:
            // Distort: Red
            bitSet(Mode_Bits, 3);
            digitalWrite(ledMode3, HIGH);
            break;
        case 4:
            // Tremelo: Green + Echo: Blue 
            bitSet(Mode_Bits, 1);
            bitSet(Mode_Bits, 2);
            digitalWrite(ledMode1, HIGH);
            digitalWrite(ledMode2, HIGH);
            break;
        case 5:
            // Tremelo: Green + Distort: Red
            bitSet(Mode_Bits, 1);
            bitSet(Mode_Bits, 3);
            digitalWrite(ledMode1, HIGH);
            digitalWrite(ledMode3, HIGH);
            break;
        case 6:
            // Echo: Blue + Distort: Red
            bitSet(Mode_Bits, 2);
            bitSet(Mode_Bits, 3);
            digitalWrite(ledMode2, HIGH);
            digitalWrite(ledMode3, HIGH);
            break;
        case 7:
            // All: White
            bitSet(Mode_Bits, 1);
            bitSet(Mode_Bits, 2);
            bitSet(Mode_Bits, 3);
            digitalWrite(ledMode1, HIGH);
            digitalWrite(ledMode2, HIGH);
            digitalWrite(ledMode3, HIGH);
            break;
    }
    
    // Apply settings (Only settings NOT inside of MyCallback)
    del.Reset();
    del.SetDelay(delay_adj);
    osc_sine.SetFreq(rate_adj);
    osc_sine.SetAmp(depth_adj);
}

// Callback to process audio
void MyCallback(float **in, float **out, size_t size)
{
    // Process the effect 
    for (size_t i = 0; i < size; i++)
    {
        // Read from I/O
         out_buf = in[0][i];

        if (bypass_On != true)
        {
            if (bitRead(Mode_Bits, 1) == 1)
            {   // ==================== Tremelo ==========================
                // Apply modulation to the dry signal
                out_buf = out_buf + (out_buf * osc_sine.Process());
            }
            if (bitRead(Mode_Bits, 2) == 1)
            {   // ==================== Delayline ========================
                // Read Wet from Delay Line
                wet = del.Read();
    
                // Write to Delay
                del.Write((wet * feedback_adj) + out_buf);
                
                // Send to I/O
                out_buf = wet + out_buf;
            } 
            if (bitRead(Mode_Bits, 3) == 1)
            {   // =================== Distortion ========================
                // Distortion based on an exponential function
                fuzz_quo = out_buf / abs(out_buf);
                fuzz_buf = fuzz_quo * (1 - exp(gain_adj * (fuzz_quo * out_buf)));
                out_buf = mix_adj * fuzz_buf + (1 - mix_adj) * out_buf;

                // Reduce signal by 50 percent
                out_buf = out_buf * 0.50f;
            }
        }
        // Note: Line level audio is mono and tied to both audio inputs.
        // Only Audio Out 2 is being used and Audio Out 1 is not connected.        
        for (size_t chn = 0; chn < num_channels; chn++)
        {
            out[chn][i] = out_buf;
        }
    }
}

void setup()
{
    // Switches and LEDs
    pinMode(btnBypass, INPUT_PULLUP);
    pinMode(btnMode, INPUT_PULLUP);
    pinMode(espHasData, INPUT_PULLUP);
    pinMode(SelectPin, OUTPUT);
    pinMode(ledBypass, OUTPUT);
    pinMode(ledMode3, OUTPUT);
    pinMode(ledMode2, OUTPUT);
    pinMode(ledMode1, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    
    // Initialize Daisy Seed at 48kHz
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    num_channels = hw.num_channels;
    sample_rate = DAISY.get_samplerate();
    
    // Initialize Effects
    del.Init();
    osc_sine.Init(sample_rate);

    // Initialize Mode
    Mode_Num = 1;
    Mode_Bits = 0;
    bitSet(Mode_Bits, 1);
    digitalWrite(ledMode1, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Read Knobs (return range is 0 to 1023)
    knob1val = analogRead(A0);
    knob2val = analogRead(A1);
    
    // Set Rate values - Echo:delay time  Tremelo:rate  Distort:gain
    prev_knob1val = knob1val;
    delay_adj = map(knob1val, 0, 1023, (sample_rate / 1000), sample_rate);
    del.SetDelay(delay_adj);
    rate_adj = knob1val / 102.30f;
    gain_adj = (knob1val / 113.66f) + 2.00f;
    
    // Set Depth values - Echo:feedback  Tremelo:depth  Distort:mix
    prev_knob2val = knob2val;
    feedback_adj = knob2val / 1075.00f;
    depth_adj = knob2val / 1023.00f;
    mix_adj = knob2val / 5110.00f;
                    
    // Set parameters for tremelo LFO 
    osc_sine.SetWaveform(Oscillator::WAVE_SIN);
    osc_sine.SetFreq(rate_adj);
    osc_sine.SetAmp(depth_adj);

    // Start SPI 
    SPI.begin();

    // Initialize Bypass
    delay(1000); // Give footswitch and mode pushbutton capacitors time to charge
    bypass_On = false;
    digitalWrite(ledBypass, HIGH);
    
    // Send data to ESP8266, D - Depth  R - Rate  M - Mode  K - Keep Alive
    sendData("D");
    sendData("R");
    
    // Start Audio
    DAISY.begin(MyCallback);
}

void loop()
{
    // Poll knobs and switches. Poll period is 100 ms and can be adjusted with var: interval 
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        // save the last poll time
        previousMillis = currentMillis;

        // ================= Check for wireless Settings change =====================
        if(digitalRead(espHasData) == HIGH)
        {
            if(espHasDataEnabled == true)
            {
                receiveData(esp.readData());
                sendData("S");
                delay(5);
                receiveData(esp.readData());
                adjustSettings();
                espHasDataEnabled = false;
                phoneMode = true;
                // Debug
                sendData("K");
            }
        }
        else
        {
            if(espHasDataEnabled == false)
            {
                espHasDataEnabled = true;
            }     
        }
        
        // ================== Check for Bypass switch activity =====================
        if(digitalRead(btnBypass) == LOW)
        {
            if(btnBypassEnabled == true)
            {
                digitalWrite(ledBypass, !(digitalRead(ledBypass)));
                btnBypassEnabled = false;
                if(digitalRead(ledBypass) == LOW)
                {
                    del.Reset(); // Reset Delayline
                    del.SetDelay(delay_adj);
                }
            }
        }
        else
        {
            if(btnBypassEnabled == false)
            {
                btnBypassEnabled = true;
            }     
        }
        if(digitalRead(ledBypass) == HIGH)
        {
            bypass_On = false;
        }
        else
        {
            bypass_On = true;
        }
        
        // ================== Check for Mode switch activity =======================
        if(digitalRead(btnMode) == LOW)
        {
            if(btnModeEnabled == true)
            {
                Mode_Bits = 0; // Set all bits to false
                digitalWrite(ledMode1, LOW); // Green
                digitalWrite(ledMode2, LOW); // Blue
                digitalWrite(ledMode3, LOW); // Red
                
                // ****** Modes ******
                // 0 None (only possible via phone app)
                // 1 Tremelo
                // 2 Echo
                // 3 Distort
                // 4 Tremelo + Echo
                // 5 Tremelo + Distort
                // 6 Echo + Distort
                // 7 All Combined
                
                switch(Mode_Num)
                {
                    case 1:
                        Mode_Num = 2; // Echo: Blue
                        bitSet(Mode_Bits, 2);
                        digitalWrite(ledMode2, HIGH);
                        del.Reset(); // Reset Delayline
                        del.SetDelay(delay_adj);
                        break;
                    case 2:
                        Mode_Num = 3; // Distort: Red
                        bitSet(Mode_Bits, 3);
                        digitalWrite(ledMode3, HIGH);
                        break;
                    case 3:
                        Mode_Num = 4; // Tremelo: Green + Echo: Blue 
                        bitSet(Mode_Bits, 1);
                        bitSet(Mode_Bits, 2);
                        digitalWrite(ledMode1, HIGH);
                        digitalWrite(ledMode2, HIGH);
                        del.Reset(); // Reset Delayline
                        del.SetDelay(delay_adj);
                        break;
                    case 4:
                        Mode_Num = 5; // Tremelo: Green + Distort: Red
                        bitSet(Mode_Bits, 1);
                        bitSet(Mode_Bits, 3);
                        digitalWrite(ledMode1, HIGH);
                        digitalWrite(ledMode3, HIGH);
                        break;
                    case 5:
                        Mode_Num = 6; // Echo: Blue + Distort: Red
                        bitSet(Mode_Bits, 2);
                        bitSet(Mode_Bits, 3);
                        digitalWrite(ledMode2, HIGH);
                        digitalWrite(ledMode3, HIGH);
                        del.Reset(); // Reset Delayline
                        del.SetDelay(delay_adj);
                        break;
                    case 6:
                        Mode_Num = 7; // All: White
                        bitSet(Mode_Bits, 1);
                        bitSet(Mode_Bits, 2);
                        bitSet(Mode_Bits, 3);
                        digitalWrite(ledMode1, HIGH);
                        digitalWrite(ledMode2, HIGH);
                        digitalWrite(ledMode3, HIGH);
                        del.Reset(); // Reset Delayline
                        del.SetDelay(delay_adj);
                        break;
                    case 0:
                    case 7:
                        Mode_Num = 1; // Tremelo: Green
                        bitSet(Mode_Bits, 1);
                        digitalWrite(ledMode1, HIGH);
                        break;
                }
                phoneMode = false;
                modeChanged = true;
                btnModeEnabled = false;
                // Send data to ESP8266, D - Depth  R - Rate  M - Mode  K - Keep Alive
                sendData("M");
            }
        }
        else
        {
            if(btnModeEnabled == false)
            {
                btnModeEnabled = true;
            }     
        }
        
        // =============== Read Knobs (return range is 0 to 1023) ==================
        if (phoneMode != true)
        {
            knob1val = analogRead(A0);
            knob2val = analogRead(A1);
    
            // *************************************************************************
            // * NOTE: Only Tremelo adjustments available in Modes 4, 5, 7             *
            // *       Only Echo adjustments available in Mode 6                       *
            // *************************************************************************
            
            // ========= Set only when knob 1 (Rate) position has changed ==============
            if (abs(prev_knob1val - knob1val) > 3)
            {
                prev_knob1val = knob1val;
                switch(Mode_Num)
                {
                    case 1:
                    case 4:
                    case 5:
                    case 7:
                        // Tremelo sweep rate Scale: 0.00 to 10.0
                        rate_adj = knob1val / 102.30f;
                        osc_sine.SetFreq(rate_adj);
                        break;
                    case 2:
                    case 6:
                        // Echo Delay time (in number of samples)  Knob Scale: 0.001 to 1.000
                        // Actual scale w/sample_rate(48000); 480.00 to 48000.00
                        delay_adj = map(knob1val, 0, 1023, (sample_rate / 1000), sample_rate);
                        del.Reset();
                        del.SetDelay(delay_adj);
                        break;
                    case 3:
                        // Distortion gain Scale: 2.0 to 11.0 (a little Spinal Tap humor)
                        gain_adj = (knob1val / 113.66f) + 2.00f;
                        break;
                }
                // Send data to ESP8266, D - Depth  R - Rate  M - Mode  K - Keep Alive
                sendData("R");
            }
            
            // ========= Set only when knob 2 (Depth) position has changed =============
            if (abs(prev_knob2val - knob2val) > 3)
            {
                prev_knob2val = knob2val;
                switch(Mode_Num)
                {
                    case 1:
                    case 4:
                    case 5:
                    case 7:
                        // Tremelo depth Scale: 0.00 to 1.00
                        depth_adj = knob2val / 1023.00f;
                        osc_sine.SetAmp(depth_adj);
                        break;
                    case 2:
                    case 6:
                        // Number of Echo repeats multiplier   Knob Scale: 0.00 to 0.95
                        feedback_adj = knob2val / 1075.00f;
                        break;
                    case 3:
                        // Distortion mix Scale: 0.00 to 0.20
                        mix_adj = knob2val / 5110.00f;
                        break;
                }
                // Send data to ESP8266, D - Depth  R - Rate  M - Mode  K - Keep Alive
                sendData("D");
            }
        }
    }
}
