//H910 Style Delay, written by Miles Comstock with the help of examples from Stephen Hensley and Ben Sergentanis
//Version 1.3

#include "DaisyDuino.h"
#include <U8g2lib.h> 
#define MAX_DELAY static_cast<size_t>(48000 * 1.5f)

static DaisyHardware hw;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;

PitchShifter ps, ps2;

float k1, k2, k3, k4; //Declarations for delay lines and audio
float sample_rate;
float currentDelay, currentDelayR, feedback, delayTarget, delayTargetR, cutoff;
float outlCopy, outrCopy;
float shifted, unshifted, shifted2, unshifted2;
float feedMode;
int counter;
bool infinFeed;
float Doffset, DoffsetLast;

U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI oled(U8G2_R0, /* clock=*/8,
                                             /* data=*/10, /* cs=*/7, /* dc=*/9,
                                             /* reset=*/30);

int x, y; //Declarations for OLED
int xvel, yvel;
char str[] = "daisypatch";
int pos;

void Controls();
void GetDelaySample(float &outl, float &outr, float inl, float inr);
void AudioCallback(float **in, float **out, size_t size) {
  float outl, outr, inl, inldry, inr, inrdry;
  static int decimCounter = 0;
  Controls();

  // audio
  for (size_t i = 0; i < size; i ++) {
    inl = in[0][i];
    inldry = inl;
    inr = in[1][i];
    unshifted2 = inl;
    shifted2 = ps2.Process(unshifted2);
    GetDelaySample(outl, outr, inl, inr);
    unshifted = outl;
    shifted = ps.Process(unshifted);
	//if in delay only mode
    if(infinFeed == 1){
      out[0][i] = (outl * k4) + (inldry * k3);
      out[1][i] = (outr * k4) + (inr * k3);
    }
	//else in pitch shift mode
    else{
      out[0][i] = (inldry * k3) + ((shifted + shifted2) * k4);
      out[1][i] = (outr * k4) + (inr * k3);
    }
  }
} 

void setup() {
  hw = DAISY.init(DAISY_PATCH, AUDIO_SR_48K);		//Initializing everything
  DAISY.SetAudioBlockSize(32);
  sample_rate = DAISY.get_samplerate();

  ps.Init(sample_rate);
  ps2.Init(sample_rate);

  ps.SetTransposition(12.0f);
  ps.SetDelSize(4096);
  ps2.SetDelSize(4096);
  ps2.SetTransposition(12.0f);

  dell.Init();
  delr.Init();

  //OLED SETUP------
  x = y = 30;
  xvel = yvel = -1;
  pos = 0;

  oled.setFont(u8g2_font_inb24_mf);
  oled.setFontDirection(0);
  oled.setFontMode(1);
  oled.begin();
  counter = 1;
  //---------------

  currentDelay = currentDelayR = delayTarget = delayTargetR = (sample_rate * 0.3f);
  dell.SetDelay(currentDelay);
  delr.SetDelay(currentDelay);
  Serial.begin(9600);
  DAISY.begin(AudioCallback);
  feedMode = 0;
  infinFeed = 0;
}

void loop() {
  Serial.println(currentDelayR + Doffset);

  char buf[12]; // screen character buffer

  if(counter == 1){ // Only updates the OLED once every 32 loops
    oled.clearBuffer();

  if(infinFeed == 0)
  {
      itoa((int)feedMode, buf, 10);   // convert feedMode to integer string
  }
  else
  {
      itoa((int)Doffset, buf, 10);    // convert Doffset to integer string
  }

    int w = oled.getStrWidth(buf); // text width in pixels
    int h = oled.getAscent();      // font ascent (~42)
    int x = (128 - w) / 2;         // Keeps text in middle of the OLED
    int y = (64 + h) / 2;
    
    oled.drawStr(x, y, buf);
    x = 50;
    y = 50;
    oled.sendBuffer();
  }
  counter++;
  if (counter>32){ 						//resets loop counter
    counter = 1;
  }

}

void UpdateKnobs(float &k1, float &k2, float&k3, float&k4) {
  k1 = 1 - (analogRead(PIN_PATCH_CTRL_1) / 1023.f);		//Analog read for knob controls
  k2 = 1 - (analogRead(PIN_PATCH_CTRL_2) / 1023.f);
  k3 = 1 - (analogRead(PIN_PATCH_CTRL_3) / 1023.f);
  k4 = 1 - (analogRead(PIN_PATCH_CTRL_4) / 1023.f);

  if(k2 > 0.89){ 						//Crude way to keep feedback from getting so high that Daisy crashes on pitch shift mode
    k2 = 0.89;
  }

  float m = (float)MAX_DELAY - .05 * sample_rate;
	delayTarget = (k1 * m + .05 * sample_rate);
  delayTargetR = (k1 * m + .05 * sample_rate);
	
  if(infinFeed == 0){						//if in pitch shift mode increment pitch shift ratio each encoder turn
    feedMode += hw.encoder.Increment() * 10;
   // ps.SetTransposition(feedMode);
   // ps2.SetTransposition(feedMode);
    ps.SetCents(feedMode);   // 37.5 cents
    ps2.SetCents(feedMode);
  }
  else{
    Doffset += (hw.encoder.Increment() * 250);	//if in delay only mode increment R delay offset each encoder turn
    if(Doffset > 100000){						//Max R delay offset increase at 100000 addresses
      Doffset = 100000;
    }
    if(Doffset + currentDelayR < 1){			//Maximum negative R delay offset can never be less than the minimum delay time
      fonepole(Doffset, 1.0f - currentDelayR, 0.0007f);
    }
  }

  if(feedMode > 1200){							//Sets maxiumum and minimum pitch shift ratios
    feedMode = 1200;
  }
  if(feedMode < -1200){
    feedMode = -1200;
  }
  
}
void Controls() {
  delayTarget = feedback = 0;
  delayTargetR = feedback = 0;
  UpdateKnobs(k1, k2, k3, k4);

  feedback = k2;
  if(feedback > 0.89){
    feedback = 0.89;
  }
  hw.DebounceControls();						//Hardware debounce for encoder pushes
  //hw.ProcessDigitalControls();
  if(hw.encoder.RisingEdge()){					//If encoder pushed down change mode
    infinFeed = !infinFeed;
  }
}


void GetDelaySample(float &outl, float &outr, float inl, float inr) {
  if(infinFeed == 1){							//If delay only mode add offset to R delay line
    fonepole(currentDelayR, delayTargetR + Doffset, .00007f);
    fonepole(currentDelay, delayTarget, .00007f);
    dell.SetDelay(currentDelay);
    delr.SetDelay(currentDelayR);
  }
  else{
    fonepole(currentDelay, delayTarget, .00007f);		//otherwise left and right delay lines are seperate but same times
    delr.SetDelay(currentDelay);
    dell.SetDelay(currentDelay);

  }

  outl = dell.Read();
  outr = delr.Read(); 								//Must be before write

  if(infinFeed == 1){								//if delay only mode
    dell.Write((feedback * outl) + inl);
  }
  else{												//if pitch shift mode
    dell.Write((feedback * shifted) + inl);
  }
  delr.Write((feedback * outr) + inr);

  outl = (feedback * outl);
  outr = (feedback * outr);

}
