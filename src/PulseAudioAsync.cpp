#ifdef PULSEAUDIO_ASYNC
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
		settings.channels = audioSettings.channels;
		settings.sampleSize = audioSettings.sampleSize * audioSettings.channels;
		settings.bufferSize = audioSettings.bufferSize * audioSettings.channels;
		settings.sampleRate = audioSettings.sampleRate;
		settings.sinkName = audioSettings.sinkName;

		modified = false;
		ups = settings.sampleRate / settings.sampleSize;

		pSampleBuffer = new float[settings.sampleSize];
		ppAudioBuffer = new float*[settings.bufferSize / settings.sampleSize];
		for (uint32_t i = 0; i < settings.bufferSize / settings.sampleSize; ++i)
			ppAudioBuffer[i] = new float[settings.sampleSize];

		initPulse();

		if (settings.sinkName.empty()) getDefaultSink();

		std::clog << "Using PulseAudio sink: \"" << settings.sinkName << "\"\n";
		initStream();
		running = true;
	}

	~AudioSamplerImpl() {
		pa_threaded_mainloop_stop(mainloop);

		pa_stream_disconnect(stream);
		pa_stream_unref(stream);

		pa_context_disconnect(context);
		pa_context_unref(context);

		pa_threaded_mainloop_free(mainloop);

		for (uint32_t i = 0; i < settings.bufferSize / settings.sampleSize; ++i)
			delete[] ppAudioBuffer[i];

		delete[] ppAudioBuffer;
		delete[] pSampleBuffer;
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
	size_t bufPos = 0;

	// multithreading

	std::mutex audioMutexLock;

	// used to handle exceptions
	std::exception_ptr exceptionPtr = nullptr;

	// settings
	AudioSettings settings;

	// pulseaudio
	pa_threaded_mainloop* mainloop;
	pa_mainloop_api* mainloopAPI;
	pa_context* context;
	pa_stream* stream;

	int error;

	void getDefaultSink() {
		running = true;
		pa_operation_unref(
		    pa_context_get_server_info(context, callback, reinterpret_cast<void*>(this)));
		while (running)
			;
	}

	void initPulse() {
		mainloop = pa_threaded_mainloop_new();
		mainloopAPI = pa_threaded_mainloop_get_api(mainloop);
		context = pa_context_new(mainloopAPI, "Vkav");

		if ((error = pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL)) < 0)
			throw std::runtime_error(
			    std::string(LOCATION "pulseaudio context failed to connect!: ") +
			    pa_strerror(error));

		pa_context_set_state_callback(context, contextReadyStateCallback,
		                              reinterpret_cast<void*>(this));

		running = true;
		if ((error = pa_threaded_mainloop_start(mainloop)) < 0)
			throw std::runtime_error(
			    std::string(LOCATION "failed to start pulseaudio mainloop!: ") +
			    pa_strerror(error));
		// wait for context to connect
		while (running)
			;
		pa_context_set_state_callback(context, contextStateCallback, reinterpret_cast<void*>(this));
	}

	void initStream() {
		pa_sample_spec ss = {};
		ss.format = PA_SAMPLE_FLOAT32LE;
		ss.rate = settings.sampleRate;
		ss.channels = settings.channels;

		pa_channel_map map;
		pa_channel_map_init_stereo(&map);

		stream = pa_stream_new(context, "Vkav", &ss, &map);

		pa_buffer_attr attr = {};
		attr.maxlength = (uint32_t)-1;
		attr.fragsize = sizeof(float) * settings.sampleSize;

		if ((error = pa_stream_connect_record(stream, settings.sinkName.c_str(), &attr,
		                                      PA_STREAM_ADJUST_LATENCY)) != 0)
			throw std::runtime_error(
			    std::string(LOCATION "failed to connect pulseaudio stream!: ") +
			    pa_strerror(error));

		pa_stream_set_read_callback(stream, read_callback, reinterpret_cast<void*>(this));
	}

	static void read_callback(pa_stream* stream, size_t nBytes, void* userData) {
		static std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
		static int numUpdates = 0;
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userData);

		const float* buf;
		size_t size;
		pa_stream_peek(stream, reinterpret_cast<const void**>(&buf), &size);
		size /= sizeof(float);
		// copy data
		for (size_t i = 0; i < size; ++i, ++audio->bufPos) {
			if (audio->bufPos == audio->settings.sampleSize) {
				audio->audioMutexLock.lock();
				std::swap(audio->ppAudioBuffer[0], audio->pSampleBuffer);
				for (size_t i = 1; i * audio->settings.sampleSize < audio->settings.bufferSize; ++i)
					std::swap(audio->ppAudioBuffer[i - 1], audio->ppAudioBuffer[i]);
				audio->audioMutexLock.unlock();
				audio->modified = true;

				++numUpdates;
				std::chrono::steady_clock::time_point currentTime =
				    std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastFrame)
				        .count() >= 1) {
					audio->ups = numUpdates;
					numUpdates = 0;
					lastFrame = currentTime;
				}
				audio->bufPos = 0;
			}

			audio->pSampleBuffer[audio->bufPos] = buf[i];
		}

		// discard data
		pa_stream_drop(stream);
	}

	static void callback(pa_context* c, const pa_server_info* i, void* userdata) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userdata);
		audio->settings.sinkName = i->default_sink_name;
		audio->settings.sinkName += ".monitor";
		audio->running = false;
	}

	static void contextStateCallback(pa_context* c, void* userdata) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userdata);
		switch (pa_context_get_state(c)) {
			case PA_CONTEXT_FAILED:
			case PA_CONTEXT_TERMINATED:
				audio->exceptionPtr = std::make_exception_ptr(
				    std::runtime_error(std::string(LOCATION "pulseaudio context failed!")));
				audio->running = false;
				break;
			case PA_CONTEXT_READY:
			default:
				// Do nothing
				break;
		}
	}

	static void contextReadyStateCallback(pa_context* c, void* userdata) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userdata);
		switch (pa_context_get_state(c)) {
			case PA_CONTEXT_FAILED:
			case PA_CONTEXT_TERMINATED:
				audio->exceptionPtr = make_exception_ptr(
				    std::runtime_error(std::string(LOCATION "pulseaudio context failed!")));
			case PA_CONTEXT_READY:
				audio->running = false;
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
