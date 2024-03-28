#include "al/app/al_App.hpp"
#include "al/graphics/al_Mesh.hpp"
#include "al/system/al_Time.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
using namespace al;

#include <iostream>
using namespace std;

#include "Gamma/SamplePlayer.h"

float dBtoA (float dBVal) {
  float ampVal = pow(10.f, dBVal / 20.f);
  return ampVal;
}

float ampTodB (float ampVal) {
  float dBVal = 20.f * log10f(abs(ampVal));
  return dBVal;
}

class ScopeBuffer {
public:
  ScopeBuffer (int samprate) : sampleRate(samprate) {}

  void writeSample (float sample) {
    for (int i = 0; i < bufferSize - 1; i++) {
      buffer[i] = buffer[i + 1];
    }
    buffer[bufferSize - 1] = sample;
  }

  float readSample (int index) {
    return buffer[index];
  }
    
protected:
  int sampleRate;
  static const int bufferSize = 44100; // <- set equal to samplerate
  float buffer[bufferSize];
};


class Phasor {
public:
  Phasor (int samprate) : sampleRate(samprate) {}

  virtual void setSampleRate (int samprate) {
    sampleRate = samprate;
    phaseIncrement = frequency / static_cast<float> (sampleRate);
  }

  virtual void setFrequency (float freq) {
    frequency = freq;
    phaseIncrement = frequency / static_cast<float> (sampleRate);
  }

  virtual float processSample() {
    float phaseIncrement = frequency / static_cast<float>(sampleRate);
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
    return phase;
  }

  virtual float setPhase (float ph) {phase = ph;}
  virtual float getPhase () {return phase;}

protected:
  float phase = 0.f;
  int sampleRate = 44100;
  float frequency = 1.f;
  float phaseIncrement = frequency / static_cast<float>(sampleRate);
};

// sinOsc using Nth-order Taylor Series expansion, N adjustable
// as seen in Gamma https://github.com/LancePutnam/Gamma 
class SinOsc : public Phasor {
public: 
  SinOsc (int samprate) : 
  Phasor(samprate) {}

  float processSample() override {
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
   return taylorNSin(
    fmod(phase + (0.25f*(frequency-1.f)), 1.f) // closer...
    * twoPi - pi, 
   N);
  }

  float setOrder (int order) {N = order;}

protected:
  int N = 11; // 11 is good
  const float pi = static_cast<float>(M_PI);
  const float twoPi = 2.f * pi;
  const float nTwoPi = -1.f * twoPi;
  int factorial (int x) {
    int output = 1;
    int n = x;
    while (n > 1) {
      output *= n;
      n -= 1;
    }
    return output;
  }

  float taylorNSin (float x, int order) {
    float output = x;
    int n = 3;
    int sign = -1;
    while (n <= order) {
     output += sign * (powf(x, n) / factorial(n));
      sign *= -1;
      n += 2;
    }
    return output;
  }
};

template<typename T>
class PolyphonyEngine {
public:
  PolyphonyEngine (int voices, int samprate) : 
  numVoices(voices), sampleRate(samprate) {}

  void prepare () {
    for (int i = 0; i < numVoices; i++) {
      oscBank.push_back(T (sampleRate));
    }
  }

  void setFrequency(float freq) {
    for (int i = 0; i < numVoices; i++) {
      oscBank[i].setFrequency((i + 1) * freq);
    }
  }

  float processSample() {
    float output = 0;
    for (int i = 0; i < numVoices; i++) {
      output += oscBank[i].processSample();
      if (i == 0) {
        float diff = oscBank[i].getPhase() - last;
        if (diff < 0) {
          for (int j = 1; j < numVoices; j++) {
            oscBank[j].setPhase(oscBank[i].getPhase());
          }  
        }
      last = oscBank[i].getPhase();
      }
    }
    return output * (1.f / numVoices); // <- should scale as a function of numVoices
  }

protected:
  int type;
  int numVoices;
  int sampleRate;
  float last;
  std::vector<T> oscBank;
};

// class PolySineSynth {
// public:
//   PolySineSynth (int voices, int samprate) : 
//   numVoices(voices), sampleRate(samprate) {}

//   void prepare () {
//     for (int i = 0; i < numVoices; i++) {
//       oscBank.push_back(SinOsc (sampleRate));
//     }
//   }

//   void setFrequency(float freq) {
//     for (int i = 0; i < numVoices; i++) {
//       oscBank[i].setFrequency((i + 1) * freq); 
//     }
//   }

//   float processSample() {
//     float output = 0;
//     float gain = 0;
//     for (int i = 0; i < numVoices; i++) {
//       float harmPower = 1.f / powf((i + 1), 2);
//       output += oscBank[i].processSample() * harmPower;
//       if (i == 0) {
//         float diff = last - oscBank[0].getPhase();
//         if (diff < 0) {
//           for (int j = 1; j < numVoices; j++) {
//             oscBank[i].setPhase(0.f);
//           }
//         float last = oscBank[0].getPhase();
//         }
//       }
//       gain += harmPower;
//     }
//     return output * (1 / gain); // <- should scale as a function of numVoices
//   }

// protected:
//   int numVoices;
//   int sampleRate;
//   float last;
//   std::vector<SinOsc> oscBank;
// };

struct DSPTester : public App {
  Parameter volControl{"volControl", "", 0.f, -96.f, 6.f};
  Parameter rmsMeter{"rmsMeter", "", -96.f, -96.f, 0.f};
  ParameterBool audioOutput{"audioOutput", "", false, 0.f, 1.f};
  ParameterBool filePlayback{"filePlayback", "", false, 0.f, 1.f};
  gam::SamplePlayer<float, gam::ipl::Linear, gam::phsInc::Loop> player;

  ScopeBuffer scopeBuffer{static_cast<int>(AudioIO().framesPerSecond())};
  Mesh oscScope{Mesh::LINE_STRIP};

  PolyphonyEngine<SinOsc> osc{5, static_cast<int>(AudioIO().framesPerSecond())};
  void onInit() {
    // set up GUI
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(volControl); // add parameter to GUI
    gui.add(rmsMeter); // add parameter to GUI
    gui.add(audioOutput); // add parameter to GUI
    gui.add(filePlayback); // add parameter to GUI
    
    //load file to player
    player.load("../Resources/HuckFinn.wav");
    osc.prepare();
    osc.setFrequency(1.f);
  }

  void onCreate() {
    for (int i = 0; i < 44100; i++) {
      oscScope.vertex((i / 44100.f) * 2 - 1, 0);
    }
  }

  void onAnimate(double dt) {
    for (int i = 0; i < 44100; i++) {
      oscScope.vertices()[i][1] = scopeBuffer.readSample(i);
    }
  }

  bool onKeyDown(const Keyboard &k) override {
    if (k.key() == 'm') { // <- on m, muteToggle
      audioOutput = !audioOutput;
      cout << "Mute Status: " << audioOutput << endl;
    }
    if (k.key() == 'p') { // <- on p, playTrack
      filePlayback = !filePlayback;
      player.reset();
      cout << "File Playback: " << filePlayback << endl; 
    }
    return true;
  }

  void onSound(AudioIOData& io) override {
    // audio throughput
    float bufferPower = 0;
    float volFactor = dBtoA(volControl);
    while(io()) { 
      if (filePlayback) {
        for (int channel = 0; channel < io.channelsOut(); channel++) {
          if (channel % 2 == 0) {
            io.out(channel) = player(0) * volFactor * audioOutput;
          } else {
            io.out(channel) = player(1) * volFactor * audioOutput;
          }
        }
      } else {
        io.out(0) = osc.processSample() * volFactor * audioOutput;
        io.out(1) = io.out(0);
      }

      // feed to oscilliscope
      if (filePlayback) {
        scopeBuffer.writeSample((io.out(0) + io.out(1)));
      } else {
        //scopeBuffer.writeSample((io.out(0) + io.out(1)));
        scopeBuffer.writeSample(io.out(0));
      }

      // feed to analysis buffer
      for (int channel = 0; channel < io.channelsIn(); channel++){
        bufferPower += powf(io.out(channel), 2);
      }
      // overload detector
      if (io.out(0) > 1.f || io.out(1) > 1.f) {
        cout << "CLIP!" << endl;
      }
    }
    bufferPower /= io.framesPerBuffer();
    rmsMeter = ampTodB(bufferPower);
  }

  void onDraw(Graphics &g) {
    g.clear(0);
    g.color(1);
    g.camera(Viewpoint::IDENTITY); // Ortho [-1:1] x [-1:1]
    g.draw(oscScope);
  }
};
  
int main() {
  DSPTester app;

  // Allows for manual declaration of input and output devices, 
  // but causes unpredictable behavior. Needs investigation.
  app.audioIO().deviceIn(AudioDevice("MacBook Pro Microphone"));
  app.audioIO().deviceOut(AudioDevice("MacBook Pro Speakers"));
  cout << "outs: " << app.audioIO().channelsOutDevice() << endl;
  cout << "ins: " << app.audioIO().channelsInDevice() << endl;
  app.player.rate(1.0 / app.audioIO().channelsOutDevice());
  app.configureAudio(44100, 128, app.audioIO().channelsOutDevice(), app.audioIO().channelsInDevice());

  /* 
  // Declaration of AudioDevice using aggregate device
  AudioDevice alloAudio = AudioDevice("AlloAudio");
  alloAudio.print();
  app.player.rate(1.0 / alloAudio.channelsOutMax());
  app.configureAudio(alloAudio, 44100, 128, alloAudio.channelsOutMax(), 2);
  */

  app.start();
}