#ifdef PULSEAUDIO
// C++ standard libraries
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

// PulseAudio
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

#include "Audio.hpp"
#include "Data.hpp"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

class AudioSampler::AudioSamplerImpl {
public:
	std::atomic<bool> running;
	std::atomic<bool> modified;
	std::atomic<int> ups;

	AudioSamplerImpl(const AudioSettings& audioSettings) {
		init(audioSettings);

		audioThread = std::thread([&]() {
			try {
				run();
			} catch (const std::exception& e) {
				running = false;
				exceptionPtr = std::current_exception();
			}
		});
	}

	~AudioSamplerImpl() {
		running = false;
		audioThread.join();

		for (uint32_t i = 0; i < settings.bufferSize / settings.sampleSize; ++i)
			delete[] ppAudioBuffer[i];

		delete[] ppAudioBuffer;
		delete[] pSampleBuffer;

		pa_simple_free(s);
	}

	void copyData(AudioData& audioData) {
		audioMutexLock.lock();
		for (size_t i = 0; i < settings.bufferSize; ++i)
			audioData.buffer[i] = ppAudioBuffer[i / settings.sampleSize][i % settings.sampleSize];
		audioMutexLock.unlock();

		modified = false;
	}

	void rethrowExceptions() {
		if (exceptionPtr) std::rethrow_exception(exceptionPtr);
	}

private:
	// data
	float** ppAudioBuffer;
	float* pSampleBuffer;

	// multithreading

	std::mutex audioMutexLock;

	// used to handle exceptions
	std::exception_ptr exceptionPtr = nullptr;

	std::thread audioThread;

	// settings
	AudioSettings settings;

	// pulseaudio
	pa_simple* s;

	pa_mainloop* mainloop;

	int error;

	void init(const AudioSettings& audioSettings) {
		settings.channels = audioSettings.channels;
		settings.sampleSize = audioSettings.sampleSize * audioSettings.channels;
		settings.bufferSize = audioSettings.bufferSize * audioSettings.channels;
		settings.sampleRate = audioSettings.sampleRate;
		settings.sinkName = audioSettings.sinkName;

		running = true;
		modified = false;
		ups = settings.sampleRate / settings.sampleSize;

		pSampleBuffer = new float[settings.sampleSize];

		ppAudioBuffer = new float*[settings.bufferSize / settings.sampleSize];

		for (uint32_t i = 0; i < settings.bufferSize / settings.sampleSize; ++i)
			ppAudioBuffer[i] = new float[settings.sampleSize];

		if (settings.sinkName.empty()) getDefaultSink();

		std::clog << "Using PulseAudio sink: \"" << settings.sinkName << "\"\n";
		setupPulse();
	}

	void run() {
		std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
		int numUpdates = 0;

		while (this->running) {
			if (pa_simple_read(s, pSampleBuffer, sizeof(float) * settings.sampleSize, &error) < 0)
				throw std::runtime_error(std::string(LOCATION "pa_simple_read() failed: ") +
				                         pa_strerror(error));

			audioMutexLock.lock();
			std::swap(ppAudioBuffer[0], pSampleBuffer);
			for (size_t i = 1; i < settings.bufferSize / settings.sampleSize; ++i)
				std::swap(ppAudioBuffer[i - 1], ppAudioBuffer[i]);
			audioMutexLock.unlock();
			this->modified = true;

			++numUpdates;
			std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastFrame).count() >=
			    1) {
				ups = numUpdates;
				numUpdates = 0;
				lastFrame = currentTime;
			}
		}
	}

	void getDefaultSink() {
		pa_mainloop_api* mainloopAPI;
		pa_context* context;

		mainloop = pa_mainloop_new();
		mainloopAPI = pa_mainloop_get_api(mainloop);
		context = pa_context_new(mainloopAPI, "Vkav");

		pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL);

		pa_context_set_state_callback(context, contextStateCallback, reinterpret_cast<void*>(this));

		pa_mainloop_run(mainloop, nullptr);

		pa_context_disconnect(context);
		pa_context_unref(context);

		pa_mainloop_free(mainloop);
	}

	void setupPulse() {
		pa_sample_spec ss = {};
		ss.format = PA_SAMPLE_FLOAT32LE;
		ss.rate = settings.sampleRate;
		ss.channels = settings.channels;

		pa_buffer_attr attr = {};
		attr.maxlength = (uint32_t)-1;
		attr.fragsize = sizeof(float) * settings.sampleSize;

		s = pa_simple_new(NULL, "Vkav", PA_STREAM_RECORD, settings.sinkName.c_str(),
		                  "recorder for Vkav", &ss, NULL, &attr, &error);

		if (!s)
			throw std::runtime_error(std::string(LOCATION "pa_simple_new() failed: ") +
			                         pa_strerror(error));
	}

	static void callback([[maybe_unused]] pa_context* c, const pa_server_info* i, void* userdata) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userdata);
		audio->settings.sinkName = i->default_sink_name;
		audio->settings.sinkName += ".monitor";

		pa_mainloop_quit(audio->mainloop, 0);
	}

	static void contextStateCallback(pa_context* c, void* userdata) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userdata);

		switch (pa_context_get_state(c)) {
			case PA_CONTEXT_READY:
				pa_operation_unref(pa_context_get_server_info(c, callback, userdata));
				break;
			case PA_CONTEXT_FAILED:
			case PA_CONTEXT_TERMINATED:
				pa_mainloop_quit(audio->mainloop, 0);
				break;
			default:
				// Do nothing
				break;
		}
	}
};

bool AudioSampler::running() const { return audioSamplerImpl->running; }

bool AudioSampler::modified() const { return audioSamplerImpl->modified; }

int AudioSampler::ups() const { return audioSamplerImpl->ups; }

void AudioSampler::start(const AudioSettings& audioSettings) {
	audioSamplerImpl = new AudioSamplerImpl(audioSettings);
}

void AudioSampler::stop() { delete audioSamplerImpl; }

void AudioSampler::copyData(AudioData& audioData) { audioSamplerImpl->copyData(audioData); }

void AudioSampler::rethrowExceptions() { return audioSamplerImpl->rethrowExceptions(); }
#endif
