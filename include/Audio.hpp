#pragma once
#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <string>
struct AudioData;

struct AudioSettings {
	unsigned char channels = 2;
	size_t sampleSize = 64;
	size_t bufferSize = 2048;
	uint32_t sampleRate = 5625;
	std::string sinkName;
};

class AudioSampler {
public:
	AudioSampler() = default;
	AudioSampler(const AudioSettings& audioSettings);
	~AudioSampler();

	AudioSampler& operator=(AudioSampler&& other) noexcept;

	bool running() const;
	bool modified() const;
	int ups() const;

	void copyData(AudioData& audioData);

	void rethrowExceptions();

private:
	class AudioSamplerImpl;
	AudioSamplerImpl* audioSamplerImpl = nullptr;
};

#endif
