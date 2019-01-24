// C++ standard libraries
#include <atomic>
#include <string>
#include <iostream>
#include <mutex>
#include <vector>

// PulseAudio libraries
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>

#include "Audio.hpp"

void AudioData::begin(AudioSettings audioSettings) {
	init(audioSettings);

	audioThread = std::thread(&AudioData::run, std::ref(*this));
}

void AudioData::end() {
	stop = true;
	audioThread.join();
	cleanup();
}

void AudioData::init(AudioSettings audioSettings) {

	settings.channels   = audioSettings.channels;
	settings.sampleSize = audioSettings.sampleSize*audioSettings.channels;
	settings.bufferSize = audioSettings.bufferSize*audioSettings.channels;
	settings.sampleRate = audioSettings.sampleRate;
	settings.sinkName   = audioSettings.sinkName;

	stop = false;
	modified = false;

	sampleBuffer = new float[settings.sampleSize];

	audioBuffer  = new float*[settings.bufferSize/settings.sampleSize];

	for (uint32_t i = 0; i < settings.bufferSize/settings.sampleSize; ++i) {
		audioBuffer[i] = new float[settings.sampleSize];
	}

	if (settings.sinkName.empty()) {
		getDefaultSink();
	}
	setupPulse();
}

void AudioData::run() {
	while(!this->stop) {
		if (pa_simple_read(s, reinterpret_cast<char*>(sampleBuffer), sizeof(float)*settings.sampleSize, &error) < 0) {
			std::cerr << __FILE__ ":" << __LINE__ << ": pa_simple_read() failed: " << pa_strerror(error) << std::endl;
			this->stop = true;
			break;
		}

		audioMutexLock.lock();

		for (uint32_t i = 0; i < settings.bufferSize/settings.sampleSize-1; ++i) {
			std::swap(audioBuffer[i], audioBuffer[i+1]);
		}
		std::swap(audioBuffer[settings.bufferSize/settings.sampleSize-1], sampleBuffer);

		audioMutexLock.unlock();
		this->modified = true;
	}
}

void AudioData::cleanup() {
	for (uint32_t i = 0; i < settings.bufferSize/settings.sampleSize; ++i) {
		delete[] audioBuffer[i];
	}
	delete[] audioBuffer;
	delete[] sampleBuffer;

	pa_simple_free(s);
}

void AudioData::copyData(std::vector<float>& lBuffer, std::vector<float>& rBuffer) {
	lBuffer.resize(settings.bufferSize/settings.channels);
	rBuffer.resize(settings.bufferSize/settings.channels);

	audioMutexLock.lock();
		for (uint32_t i = 0; i < settings.bufferSize; i += settings.channels) {
			if (settings.channels == 1) {
				lBuffer[i] = audioBuffer[i/settings.sampleSize][i%settings.sampleSize];
				rBuffer[i] = audioBuffer[i/settings.sampleSize][i%settings.sampleSize];
			} else {
				lBuffer[i/2] = audioBuffer[i    /settings.sampleSize][i    %settings.sampleSize];
				rBuffer[i/2] = audioBuffer[(i+1)/settings.sampleSize][(i+1)%settings.sampleSize];
			}
		}
	audioMutexLock.unlock();

	modified = false;
}

void AudioData::getDefaultSink() {

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

void AudioData::setupPulse() {
	const pa_sample_spec ss = {
		.format   = PA_SAMPLE_FLOAT32LE,
		.rate     = settings.sampleRate,
		.channels = settings.channels
	};

	s = pa_simple_new(
			NULL,
			"Vkav",
			PA_STREAM_RECORD,
			settings.sinkName.c_str(),
			"recorder for Vkav",
			&ss,
			NULL,
			NULL,
			&error
		);

	if (!s) {
		throw std::runtime_error("pa_simple_new() failed!");
	}
}

void AudioData::contextStateCallback(pa_context* c, void* userdata) {
	auto audioData = reinterpret_cast<AudioData*>(userdata);

	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_READY:
		pa_operation_unref(pa_context_get_server_info(c, callback, userdata));
			break;
		case PA_CONTEXT_FAILED:
			pa_mainloop_quit(audioData->mainloop, 0);
			break;
		case PA_CONTEXT_TERMINATED:
			pa_mainloop_quit(audioData->mainloop, 0);
			break;
	}
}

void AudioData::callback(pa_context* c, const pa_server_info* i, void* userdata) {
	auto audioData = reinterpret_cast<AudioData*>(userdata);
	audioData->settings.sinkName = i->default_sink_name;
	audioData->settings.sinkName += ".monitor";

	pa_mainloop_quit(audioData->mainloop, 0);
}
