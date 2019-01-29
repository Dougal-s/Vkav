#ifndef AUDIO_HPP
#define AUDIO_HPP

// C++ standard libraries
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// PulseAudio libraries
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>

struct AudioSettings {
	unsigned char channels   = 2;
	uint32_t      sampleSize = 64;
	uint32_t      bufferSize = 2048;
	uint32_t      sampleRate = 5625;
	std::string   sinkName;
};

class AudioData {
public:
	std::atomic<bool> stop;
	std::atomic<bool> modified;
	std::atomic<int> ups;

	void begin(AudioSettings audioSettings);

	void end();

	void copyData(std::vector<float>& lBuffer, std::vector<float>& rBuffer);

private:
	float** audioBuffer;
	float* sampleBuffer;

	std::mutex audioMutexLock;

	pa_simple* s;

	AudioSettings settings;

	std::thread audioThread;

	pa_mainloop* mainloop;

	int error;

	void init(AudioSettings audioSettings);

	void run();

	void cleanup();

	void getDefaultSink();

	void setupPulse();

	static void callback(pa_context* c, const pa_server_info* i, void* userdata);

	static void contextStateCallback(pa_context* c, void* userdata);
};

#endif
