// Joel A. Jaffe 2024-03-14 
/*
Implementation of an intermediary buffer to capture and store samples from
an audio buffer and use them for an oscilliscope

To Do:
-initialize bufferSize in terms of sampleRate
*/

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