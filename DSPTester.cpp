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

Vec3f randomVec3f(float scale) { // <- Function that returns a Vec2f containing random coords
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
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
  ScopeBuffer scopeBuffer;
  Mesh oscScope{Mesh::POINTS};

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
    if (k.key() == ' ') { // <- on space, print scopeBuffer.readSample(0)
      cout << "scopeBuffer[0]: " << scopeBuffer.readSample(0) << endl;
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
        for (int channel = 0; channel < io.channelsOut(); channel++) {
          if (channel % 2 == 0) {
            io.out(channel) = io.in(0) * volFactor * audioOutput;
          } else {
            io.out(channel) = io.in(1) * volFactor * audioOutput;
          }
        }
      }

      // audio analysis
      for (int channel = 0; channel < io.channelsIn(); channel++){
        bufferPower += powf(io.out(channel), 2);
      }
      //feed to oscilliscope
      scopeBuffer.writeSample(io.out(0) + io.out(1));
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