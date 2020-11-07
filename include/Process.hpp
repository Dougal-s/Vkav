#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP

struct AudioData;

struct ProcessSettings {
	unsigned char channels;
	size_t size;
	float smoothingLevel;
	float amplitude;
};

class Process {
public:
	void init(const ProcessSettings& settings);

	void processSignal(AudioData& audioData);

	void cleanup();

private:
	class ProcessImpl;
	ProcessImpl* impl;
};

#endif
