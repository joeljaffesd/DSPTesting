// Joel A. Jaffe 2024-04-21
// Basic Audio Input/Output App for Testing DSP Code

#include "al/app/al_App.hpp"
#include "al/graphics/al_Mesh.hpp"
#include "al/app/al_GUIDomain.hpp"
using namespace al;

#include <iostream>
using namespace std;

// handy functions in audio
float dBtoA (float dBVal) {return powf(10.f, dBVal / 20.f);}
float ampTodB (float ampVal) {return 20.f * log10f(fabs(ampVal));}

// Oscilliscope that inherits from mesh 
class Oscilliscope : public Mesh {
public:
  Oscilliscope (int samplerate) : bufferSize(samplerate) {
    this->primitive(Mesh::LINE_STRIP);
    for (int i = 0; i < bufferSize; i++) {
      this->vertex((i / static_cast<float>(bufferSize)) * 2.f - 1.f, 0);
      buffer.push_back(0.f);
    }
  }

  void writeSample (float sample) {
    for (int i = 0; i < bufferSize - 1; i++) {
      buffer[i] = buffer[i + 1];
    }
    buffer[bufferSize - 1] = sample;
  }

  void update() {
    for (int i = 0; i < bufferSize; i++) {
      this->vertices()[i][1] = buffer[i];
    }
  }
    
protected:
  int bufferSize;
  vector<float> buffer;
};

// app struct
struct Basic_IO : public App {
  Parameter volControl{"volControl", "", 0.f, -96.f, 6.f};
  Parameter rmsMeter{"rmsMeter", "", -96.f, -96.f, 0.f};
  ParameterBool audioOutput{"audioOutput", "", false, 0.f, 1.f};

  Oscilliscope scope{static_cast<int>(AudioIO().framesPerSecond())};

  void onInit() {
    // set up GUI
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(volControl); // add parameter to GUI
    gui.add(rmsMeter);
    gui.add(audioOutput); 
  }

  void onCreate() {}

  void onAnimate(double dt) {
    scope.update();
  }

  bool onKeyDown(const Keyboard &k) override {
    if (k.key() == 'm') { // <- on m, muteToggle
      audioOutput = !audioOutput;
      cout << "Mute Status: " << audioOutput << endl;
    }
    return true;
  }

  void onSound(AudioIOData& io) override {
    // variables reset for each call
    float bufferPower = 0; // for measuring output RMS
    float volFactor = dBtoA(volControl); // vol control

    // sample loop. variables declared inside reset for each sample
    while(io()) { 
      // capture input sample
      float input = io.in(0);

      // transform input for output (put your DSP here!)
      float output = input * volFactor * audioOutput; 
      // float output = g(f(input)) etc... 

      // for each channel, write output to speaker
      for (int channel = 0; channel < io.channelsOut(); channel++) {
        io.out(channel) = output; 
      }

      // feed to oscilliscope
      scope.writeSample(output);

      // feed to analysis buffer
      bufferPower += output;

      // overload detector (assuming 2 output channels)
      if (io.out(0) > 1.f || io.out(1) > 1.f) {
        cout << "CLIP!" << endl;
      }
    }

    bufferPower /= io.framesPerBuffer(); // calculate bufferPower
    rmsMeter = ampTodB(bufferPower); // print to GUI display
  }

  void onDraw(Graphics &g) {
    g.clear(0);
    g.color(1);
    g.camera(Viewpoint::IDENTITY); // Ortho [-1:1] x [-1:1]
    g.draw(scope);
  }
};
  
int main() {
  Basic_IO app; // instance of our app 

  // Allows for manual declaration of input and output devices, 
  // but causes unpredictable behavior. Needs investigation.
  app.audioIO().deviceIn(AudioDevice("External Microphone")); // change for your device
  app.audioIO().deviceOut(AudioDevice("External Headphones")); // change for your device
  cout << "outs: " << app.audioIO().channelsOutDevice() << endl;
  cout << "ins: " << app.audioIO().channelsInDevice() << endl;
  app.configureAudio(44100, 128, app.audioIO().channelsOutDevice(), app.audioIO().channelsInDevice());
  // ^ samplerate, buffersize, channels out, channels in
  
  /*
  // Declaration of AudioDevice using aggregate device
  AudioDevice alloAudio = AudioDevice("Your_Aggregate_Device");
  alloAudio.print();
  app.configureAudio(alloAudio, 44100, 128, alloAudio.channelsOutMax(), 2);
  */

  app.start();
  return 0;
}