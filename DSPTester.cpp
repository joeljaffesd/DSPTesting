#include "al/app/al_App.hpp"
#include "al/graphics/al_Mesh.hpp"
#include "al/system/al_Time.hpp"
#include "al/app/al_GUIDomain.hpp"
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
  // ScopeBuffer (int samplerate) {
  //   sampleRate = samplerate;
  // }

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
  static const int bufferSize = 44100;
  float buffer[bufferSize];
};

class Phasor {
public:
  void setSampleRate (int samplerate) {
    sampleRate = samplerate;
  }

  void setFrequency (float freq) {
    frequency = freq;
    phaseIncrement = frequency / static_cast<float> (sampleRate);
  }

  virtual float processSample() {
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
    return phase;
  }

protected:
  float phase = 0;
  int sampleRate = 44100;
  float frequency = 1.f;
  float phaseIncrement = frequency / static_cast<float>(sampleRate);
};

// ideal sawtooth wave, needs anti-aliasing filter
class SawOsc : public Phasor {
public:
  float processSample() override {
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
  return phase * 2.f - 1.f;
  }
};

// ideal triangle wave, needs anti-aliasing filter
class TriOsc : public Phasor {
public:
  float processSample() override {
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
  return fabs(phase * 2.f - 1.f) * 2 - 1;
  }
};

// sinOsc using 7th-order Taylor Series expansion
class SinOsc : public Phasor {
public: 
  float processSample() override {
    phase += phaseIncrement;
    phase = fmod(phase, 1.f);
   return taylor9Sin(phase * static_cast<float>(M_2PI)
    - static_cast<float>(M_PI));
  //  return sin(phase * static_cast<float>(M_2PI)
  //   - static_cast<float>(M_PI));
  }

protected:
  float taylor9Sin (float x) {
  return x - (powf(x, 3.f) / factorial(3)) + (powf(x, 5.f) / 
    factorial(5)) - (powf(x, 7.f) / factorial(7)) + 
    (powf(x, 9.f) / factorial(9));
  }

  float factorial (int x) {
    float output = 1;
    int n = x;
    while (n > 1) {
      output *= n;
      n -= 1;
    }
  return output;
  }
};

class DelayLine {
public:
  void pushSample (float sample) {
    buffer[writeIndex] = sample;
    writeIndex = (writeIndex + 1) % bufferSize;
  }

  float popSample (int delayTimeInSamples) {
    readIndex = (writeIndex - delayTimeInSamples + bufferSize) % bufferSize;
    return buffer[readIndex];
  }

private:
  static const int bufferSize = 96000;
  float buffer[bufferSize];
  int readIndex;
  int writeIndex;
};

struct DSPTester : public App {
  Parameter volControl{"volControl", "", 0.f, -96.f, 6.f};
  Parameter rmsMeter{"rmsMeter", "", -96.f, -96.f, 0.f};
  ParameterBool audioOutput{"audioOutput", "", false, 0.f, 1.f};
  ParameterBool filePlayback{"filePlayback", "", false, 0.f, 1.f};
  gam::SamplePlayer<float, gam::ipl::Linear, gam::phsInc::Loop> player;

  ScopeBuffer scopeBuffer;
  Mesh oscScope{Mesh::LINE_STRIP};

  SinOsc osc;

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
    //osc.setFrequency(220.f);
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
        // for (int channel = 0; channel < io.channelsOut(); channel++) {
        //   if (channel % 2 == 0) {
        //     io.out(channel) = io.in(0) * volFactor * audioOutput;
        //   } else {
        //     io.out(channel) = io.in(1) * volFactor * audioOutput;
        //   }
        // }
        io.out(0) = osc.processSample();
      }

      // audio analysis
      for (int channel = 0; channel < io.channelsIn(); channel++){
        bufferPower += powf(io.out(channel), 2);
      }
      //feed to oscilliscope
      //scopeBuffer.writeSample(sinOsc.processSample());
      scopeBuffer.writeSample(io.out(0));
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
  app.audioIO().deviceOut(AudioDevice("BlackHole 2ch"));
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