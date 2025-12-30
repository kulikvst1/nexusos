#pragma once
struct PitchResult { float hz; float confidence; };

struct IPitchDetector {
    virtual ~IPitchDetector() = default;
    virtual void prepare(int bufferSize, double sampleRate) = 0;
    virtual PitchResult process(const float* frame, int size) = 0;
};
