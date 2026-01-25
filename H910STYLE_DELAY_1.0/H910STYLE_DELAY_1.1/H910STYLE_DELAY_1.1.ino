//H910 Style Delay, written by Miles Comstock with the help of examples from Stephen Hensley and Ben Sergentanis
//Version 1.1
#include "DaisyDuino.h"
#define MAX_DELAY static_cast<size_t>(48000 * 1.5f)

static DaisyHardware hw;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;

PitchShifter ps, ps2;
float k1, k2, k3, k4;
float sample_rate;
float currentDelay, feedback, delayTarget, cutoff;
float outlCopy, outrCopy;
float shifted, unshifted, shifted2, unshifted2;
int feedMode;
bool infinFeed;


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
    if(infinFeed == 1){
      out[0][i] = (outl * k4) + (inldry * k3);
      out[1][i] = (outr * k4) + (inr * k3);
    }
    else{
      out[0][i] = (inldry * k3) + ((shifted + shifted2) * k4);
      out[1][i] = (outr * k4) + (inr * k3);
    }
  }
} 

void setup() {
  hw = DAISY.init(DAISY_PATCH, AUDIO_SR_48K);
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

  currentDelay = delayTarget = (sample_rate * 0.75f);
  dell.SetDelay(currentDelay);
  delr.SetDelay(currentDelay);
  Serial.begin(9600);
  DAISY.begin(AudioCallback);
  feedMode = 0;
  infinFeed = 0;
}

void loop() {}

void UpdateKnobs(float &k1, float &k2) {
  k1 = 1 - (analogRead(PIN_PATCH_CTRL_1) / 1023.f);
  k2 = 1 - (analogRead(PIN_PATCH_CTRL_2) / 1023.f);
  k3 = 1 - (analogRead(PIN_PATCH_CTRL_3) / 1023.f);
  k4 = 1 - (analogRead(PIN_PATCH_CTRL_4) / 1023.f);

  if(k2 > 0.89){
    k2 = 0.89;
  }
  ps.SetTransposition(feedMode);
  ps2.SetTransposition(feedMode);

  float m = (float)MAX_DELAY - .05 * sample_rate;
	delayTarget = (k1 * m + .05 * sample_rate);
  feedMode += hw.encoder.Increment();
  if(feedMode > 12){
    feedMode = 12;
  }
  if(feedMode < -12){
    feedMode = -12;
  }
  Serial.println(feedMode);
}
void Controls() {
  delayTarget = feedback = drywet = 0;
  UpdateKnobs(k1, k2);

  feedback = k2;
  if(feedback > 0.89){
    feedback = 0.89;
  }
  hw.DebounceControls();
  //hw.ProcessDigitalControls();
  if(hw.encoder.RisingEdge()){
    infinFeed = !infinFeed;
  }
}


void GetDelaySample(float &outl, float &outr, float inl, float inr) {
  fonepole(currentDelay, delayTarget, .00007f);
  delr.SetDelay(currentDelay);
  dell.SetDelay(currentDelay / 2);

  outl = dell.Read();
  if(infinFeed == 1){
    dell.Write((feedback * outl) + inl);
  }
  else{
    dell.Write((feedback * shifted) + inl);
  }
  delr.Write((feedback * outr) + inr);

  outl = (feedback * outl);
  outr = delr.Read();
  outr = (feedback * outr);

}
