#include "al/app/al_App.hpp"
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
  DelayLine delayLine;

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
  }
  void onCreate() {}
  void onAnimate(double dt) {}
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
    delayLine.pushSample(io.in(0));
    if (filePlayback) {
      for (int i = 0; i < io.channelsOut(); i++) {
        if (i % 2 == 0) {
          io.out(i) = player(0) * volFactor * audioOutput;
        } else {
          io.out(i) = player(1) * volFactor * audioOutput;
        }
      }
    } else {
      for (int i = 0; i < io.channelsOut(); i++) {
        if (i % 2 == 0) {
          io.out(i) = io.in(0) * volFactor * audioOutput;
        } else {
          io.out(i) = (io.in(1) + delayLine.popSample(44100)) * volFactor * audioOutput;
        }
      }
    }
    // audio analysis
    for (int channel = 0; channel < io.channelsIn(); channel++){
      bufferPower += powf(io.out(channel), 2);
    }
    }
    bufferPower /= io.framesPerBuffer();
    rmsMeter = ampTodB(bufferPower);
  }

  void onDraw(Graphics &g) {
    g.clear(0);
  }
};
  
int main() {
  DSPTester app;

  // Allows for manual declaration of input and output devices, 
  // but causes unpredictable behavior. Needs investigation.
  app.audioIO().deviceIn(AudioDevice("BlackHole 2ch"));
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