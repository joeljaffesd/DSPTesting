// Joel A. Jaffe 2024-03-14 
/*
Implementation of a lightweight circular buffer using a fixed-size array.
Maximum delay is 96000 samples.
*/

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