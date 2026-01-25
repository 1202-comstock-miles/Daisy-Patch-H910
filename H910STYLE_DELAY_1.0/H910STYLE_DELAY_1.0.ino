//PRIMETIME TYPE DELAY FOR PATCH WORKING BETTER
#include "DaisyDuino.h"

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 1.5f)

static DaisyHardware hw;

static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;

Svf holdFilt, holdFiltR;
PitchShifter ps, ps2;
float k1, k2, k3, k4;
float sample_rate;
float currentDelay, feedback, delayTarget, cutoff;
float outlCopy, outrCopy, outlFilt, outrFilt;
float lastoutl, lastoutr;
float shifted, unshifted, shifted2, unshifted2;
float drywet;
int dCount = 1;
int feedMode, prevfeedMode;
int aCount = 1;
bool infinFeed;

int DECIM = 1;   // 48k / 4 = 12 kHz


// Helper functions
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
      // Only update delay engine every N samples
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
    //out[0][i] = inl + shifted;
  }
} 

void setup() {
  // Inits and sample rate
  hw = DAISY.init(DAISY_PATCH, AUDIO_SR_48K);
  DAISY.SetAudioBlockSize(32);
  //num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  ps.Init(sample_rate);
  ps2.Init(sample_rate);

  // set transposition 1 octave up (12 semitones)
  ps.SetTransposition(12.0f);
  ps.SetDelSize(4096);
  ps2.SetDelSize(4096);
  ps2.SetTransposition(12.0f);

  dell.Init();
  delr.Init();
  holdFilt.Init(sample_rate);
  holdFilt.SetRes(0);
  holdFilt.SetDrive(0);
  holdFilt.SetFreq(2000);

  holdFiltR.Init(sample_rate);
  holdFiltR.SetRes(0);
  holdFiltR.SetDrive(0);
  holdFiltR.SetFreq(2000);

  // delay parameters
  currentDelay = delayTarget = (sample_rate * 0.75f);
  dell.SetDelay(currentDelay);
  delr.SetDelay(currentDelay);
  Serial.begin(9600);
  // start callback
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
  ps.SetTransposition(0.0f + feedMode);
  ps2.SetTransposition(0.0f + feedMode);

  float m = (float)MAX_DELAY - .05 * sample_rate;
	delayTarget = (k1 * m + .05 * sample_rate);
  feedMode += hw.encoder.Increment();
  DECIM = feedMode;
  if(feedMode > 12){
    feedMode = 12;
  }
  if(feedMode <-12){
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
  dCount++;
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

  outl = (feedback * outl);

  outr = delr.Read();
  delr.Write((feedback * outr) + inr);
  outr = (feedback * outr);

}
