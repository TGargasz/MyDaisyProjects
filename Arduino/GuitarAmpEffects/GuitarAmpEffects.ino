/*
GuitarAmpEffects.ino - Multiple audio effects for guitar amplifier
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

// ************************************************************
// * Daisy Seed effects With Rate and Depth Knobs             *
// *    By: Tony Gargasz - tony.gargasz@gmail.com             *
// *    R1 - Optimized control interaction                    *
// *    R2 - Added Distortion and Tremelo effects             *
// *    R3 - Clean up for release                             *
// *    R3.1 - Added license for release into the wild        *
// ************************************************************
#include <DaisyDuino.h>

DaisyHardware hw;

size_t num_channels;
boolean bypass_On = false;
boolean btnBypassEnabled = true;
boolean btnModeEnabled = true;
boolean modeChanged = false;

// General vars
int Mode_Num, Mode_Bits;
float sample_rate, out_buf, prev_knob1val, prev_knob2val, knob1val, knob2val;
// Delay Line (echo) vars
float wet, delay_adj, feedback_adj;
// Tremelo vars 
float rate_adj, depth_adj;
// Distortion vars
float fuzz_buf, gain_adj, mix_adj, fuzz_quo;

// Switches and LEDs [D11 & D12 used for I2C]
int btnBypass = D7;     // Bypass Switch
int btnMode = D9;       // Mode Switch
int ledBypass = D10;    // Bypass Led (blue) 
int ledMode3 = D8;      // Mode 3 Led (Distortion)
int ledMode2 = D13;     // Mode 2 Led (Echo)
int ledMode1 = D14;     // Mode 1 Led (Tremelo)

// To set period to poll knobs and switchs
unsigned long currentMillis;
unsigned long previousMillis = 0;
const long interval = 100; // interval at which to poll (milliseconds)

// Set max delay time to 100% of samplerate (1 second)
#define MAX_DELAY static_cast<size_t>(48000)

// Instantiate Daisy Seed Effects
static DelayLine<float, MAX_DELAY> del;
static Oscillator osc_sine;

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
                // No Echo if depth is turned all the way down
                if (feedback_adj != 0)
                {
                    // Read Wet from Delay Line
                    wet = del.Read();
        
                    // Write to Delay
                    del.Write((wet * feedback_adj) + out_buf);
                    
                    // Send to I/O
                    out_buf = wet + out_buf;
                }
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
    pinMode(ledBypass, OUTPUT);
    pinMode(ledMode3, OUTPUT);
    pinMode(ledMode2, OUTPUT);
    pinMode(ledMode1, OUTPUT);
    
    // Initialize Daisy Seed at 48kHz
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    num_channels = hw.num_channels;
    sample_rate = DAISY.get_samplerate();
    
    // Initialize Effects
    del.Init();
    osc_sine.Init(sample_rate);
    
    // Initialize Bypass
    delay(1000); // Give footswitch and mode pushbutton capacitors time to charge
    bypass_On = false;
    digitalWrite(ledBypass, HIGH);

    // Initialize Mode
    Mode_Num = 1;
    Mode_Bits = 0;
    bitSet(Mode_Bits, 1);
    digitalWrite(ledMode1, HIGH);
    
    // Read Knobs (return range is 0 to 1023)
    knob1val = analogRead(A0);
    knob2val = analogRead(A1);
    
    // Set Rate values - Echo:delay time  Tremelo:rate  Distort:gain
    prev_knob1val = knob1val;
    delay_adj = sample_rate * ((knob1val / 1023.00f) + 0.01f);
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
     
    // Start Audio
    DAISY.begin(MyCallback);
}

void loop()
{ // Poll knobs and switches. Poll period is 100 ms and can be adjusted with var: interval 
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        // save the last poll time
        previousMillis = currentMillis;
    
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
                    case 7:
                        Mode_Num = 1; // Tremelo: Green
                        bitSet(Mode_Bits, 1);
                        digitalWrite(ledMode1, HIGH);
                        break;
                }
                modeChanged = true;
                btnModeEnabled = false;
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
        knob1val = analogRead(A0);
        knob2val = analogRead(A1);

        // *************************************************************************
        // * NOTE: Only Tremelo adjustments available in Modes 4, 5, 7             *
        // *       Only Echo adjustments available in Mode 6                       *
        // *************************************************************************
        
        // ========= Set only when knob 1 (Rate) position has changed ==============
        if (abs(prev_knob1val - knob1val) > 2)
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
                    // Echo Delay time (in number of samples)  Knob Scale: 0.01 to 1.01
                    delay_adj = sample_rate * ((knob1val / 1023.00f) + 0.01f);
                    del.Reset();
                    del.SetDelay(delay_adj);
                    break;
                case 3:
                    // Distortion gain Scale: 2.0 to 11.0
                    gain_adj = (knob1val / 113.66f) + 2.00f;
                    break;
            }
        }
        
        // ========= Set only when knob 2 (Depth) position has changed =============
        if (abs(prev_knob2val - knob2val) > 2)
        {
            prev_knob2val = knob2val;
            switch(Mode_Num)
            {
                case 1:
                case 4:
                case 5:
                case 7:
                    // Tremelo depth Scale: 0.00 to 01.00
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
        }
    }
}
