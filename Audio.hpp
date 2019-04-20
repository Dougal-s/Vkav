#pragma once
#ifndef AUDIO_HPP
#define AUDIO_HPP

// C++ standard libraries
#include <string>

#include "Data.hpp"

struct AudioSettings {
	unsigned char channels   = 2;
	size_t        sampleSize = 64;
	size_t        bufferSize = 2048;
	uint32_t      sampleRate = 5625;
	std::string   sinkName;
};

class AudioSampler {
public:
	bool stopped();
	bool modified();
	int ups();

	void start(const AudioSettings& audioSettings);

	void stop();

	void copyData(AudioData& audioData);

private:
	class AudioSamplerImpl;
	AudioSamplerImpl* audioSamplerImpl;
};

#endif
