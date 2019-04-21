#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP

#include <complex>
struct AudioData;

struct ProccessSettings {
	size_t inputSize;
	size_t outputSize;
	float smoothingLevel;
	float amplitude;
};

class Proccess {
public:

	void init(const ProccessSettings& proccessSettings);

	void proccessSignal(AudioData& audioData);

	void cleanup();

private:
	// Member variables
	float* lBuffer;
	float* rBuffer;

	size_t inputSize;

	float amplitude;

	// window function
	float wfCoeff;

	// smooth
	bool smoothData;
	size_t smoothedSize;
	float smoothingFactor;

	// Member functions
	void windowFunction(AudioData& audioData);

	void magnitudes(AudioData& audioData);

	void equalise(AudioData& audioData);

	void calculateVolume(AudioData& audioData);

	void smooth(AudioData& audioData);

	void kernelSmooth(AudioData& audioData);

	// Static member functions
	static void fft(std::complex<float>* a, size_t n);

	static void separate(std::complex<float>* a, size_t n);
};

#endif
