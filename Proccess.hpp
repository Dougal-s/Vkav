#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP

#include <complex>
struct AudioData;

struct ProccessSettings {
	unsigned char channels;
	size_t inputSize;
	size_t outputSize;
	float smoothingLevel;
	float amplitude;
};

class Proccess {
public:
	void init(const ProccessSettings& settings);

	void proccessSignal(AudioData& audioData);

	void cleanup();

private:
	class ProccessImpl;
	ProccessImpl* impl;
};

#endif
