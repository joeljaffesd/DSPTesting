// Joel A. Jaffe 2024-04-17 
/*
Implementation of a time-domain pitch shifter

TO-DO:
-make windowSize adjustable 
*/

#include <cmath>
using namespace std;

class PitchShift {
public:
  PitchShift (int samprate) : sampleRate(samprate) {}

  void setPitchRatio (float ratio) {pitchRatio = ratio;}

  float processSample(float input) {
    float frequency = fabs(1000.f * ((1.f - pitchRatio) / windowSize));
    float phaseIncrement = frequency / static_cast<float>(sampleRate);

    if (pitchRatio < 1.f || pitchRatio > 1.f) { phase += phaseIncrement; } // up or down shifting
    else { phase = 0.f; } // no shift 
    phase = fmod(phase, 1.f); // modulo logic for phase 

    float phaseTap = 0.f; // create variable for sampling phase at given timestep
    if (pitchRatio > 1.f) {phaseTap = 1 - phase;} // reverse sawtooth for up shifting  
    else {phaseTap = phase;} // otherwise let it ride 

    this->writeSample(input); // write sample to delay buffer

    int readIndex = bufferSize - 1 - static_cast<int>(round( // readpoint 1 
      phaseTap * (windowSize * (sampleRate / 1000.f))));
    int readIndex2 = bufferSize - 1 - static_cast<int>(round( // readpoint 2
      (fmod(phaseTap + 0.5f, 1) * (windowSize * (sampleRate / 1000.f)))));

    float output = this->readSample(readIndex); // get sample
    float output2 = this->readSample(readIndex2); // get sample 2
    float windowOne = cosf((((phaseTap - 0.5f) / 2.f)) * 2.f * M_PI); // gain windowing 
    float windowTwo = cosf(((fmod((phaseTap + 0.5f), 1.f) - 0.5f) / 2.f) * 2.f * M_PI);// // 

    return output * windowOne + output2 * windowTwo; // windowed output
  }

  void writeSample (float sample) {
    for (int i = 0; i < bufferSize - 1; i++) {
      buffer[i] = buffer[i + 1];
    }
    buffer[bufferSize - 1] = sample;
  }

  float readSample (int index) {
    return buffer[index];
  }

private:
  static const int bufferSize = 96000;
  float buffer[bufferSize];
  int writeIndex;
  int sampleRate;
  float phase = 0.f;
  float pitchRatio = 1.f;
  float windowSize = 22.f; // make adjustable / calculate for optimized ratio? 
};