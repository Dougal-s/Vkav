#pragma once
#ifndef AUDIO_HPP
#define AUDIO_HPP

// C++ standard libraries
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// PulseAudio libraries
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>

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
	std::atomic<bool> stopped;
	std::atomic<bool> modified;
	std::atomic<int> ups;

	void start(const AudioSettings& audioSettings);

	void stop();

	void copyData(AudioData& audioData);

private:
	float** ppAudioBuffer;
	float* pSampleBuffer;

	std::mutex audioMutexLock;

	pa_simple* s;

	AudioSettings settings;

	std::thread audioThread;

	pa_mainloop* mainloop;

	int error;

	void init(const AudioSettings& audioSettings);

	void run();

	void cleanup();

	void getDefaultSink();

	void setupPulse();

	static void callback(pa_context* c, const pa_server_info* i, void* userdata);

	static void contextStateCallback(pa_context* c, void* userdata);
};

#endif
