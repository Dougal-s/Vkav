// C++ standard libraries
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

// PortAudio
#include "portaudio.h"

#include "Audio.hpp"
#include "Data.hpp"

#include "printf.hpp"

#ifdef NDEBUG
	#define LOCATION
#else
	#define STR_HELPER(x) #x
	#define STR(x) STR_HELPER(x)
	#define LOCATION __FILE__ ":" STR(__LINE__) ": "
#endif

class AudioSampler::AudioSamplerImpl {
public:
	std::atomic<bool> running;
	std::atomic<bool> modified;
	std::atomic<int> ups;
	PaError _result;
	PaStream* stream;

	AudioSamplerImpl(const Settings& audioSettings) {
		settings.channels = audioSettings.channels;
		settings.sampleSize = audioSettings.sampleSize * audioSettings.channels;
		settings.bufferSize = audioSettings.bufferSize * audioSettings.channels;
		settings.sampleRate = audioSettings.sampleRate;
		settings.normalize = audioSettings.normalize;
		settings.sinkName = audioSettings.sinkName;

		modified = false;
		ups = settings.sampleRate / settings.sampleSize;

		pSampleBuffer = new float[settings.sampleSize];
		ppAudioBuffer = new float*[settings.bufferSize / settings.sampleSize];
		for (uint32_t i = 0; i < settings.bufferSize / settings.sampleSize; ++i)
			ppAudioBuffer[i] = new float[settings.sampleSize];

		initSoundIo();
		running = true;
	}

	~AudioSamplerImpl() {
		if (_result == paNoError) {
			Pa_Terminate();
		}

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
	Settings settings;

	uint32_t sampleRate;

	int error;

	void initSoundIo() {
		_result = Pa_Initialize();
		if (_result != paNoError) throw std::runtime_error(LOCATION "PortAudio init failed");

		int numHostApis = Pa_GetHostApiCount();
		printf("found %d PortAudio host APIs:\n", numHostApis);
		if (numHostApis < 0)
			return throw std::runtime_error(LOCATION "no host APIs found");

		const PaHostApiInfo* apiInfo;
		int asioApiIdx = -1;
		for (int i = 0; i < numHostApis; i++) {
			apiInfo = Pa_GetHostApiInfo(i);
			printf("  %d: %s\n", i, apiInfo->name);
			if (!strcmp(apiInfo->name, "ASIO")) asioApiIdx = i;
		}

		int numDevices = Pa_GetDeviceCount();
		printf("found %d PortAudio devices:\n", numDevices);
		if (numDevices < 0)
			return throw std::runtime_error(LOCATION "no devices found");

		const int defInputDevIdx = Pa_GetDefaultInputDevice();
		const int defOutputDevIdx = Pa_GetDefaultOutputDevice();
		const PaDeviceInfo* devInfo;
		int selectedDevice = -1;
		bool selectedIsAsio4All = false;
		int selectedHostApi = -2;
		double selectedDevLatency = 1000.;
		for (int i = 0; i < numDevices; i++) {
			devInfo = Pa_GetDeviceInfo(i);
			PaHostApiIndex hostApi = devInfo->hostApi;
			printf("  %d %s", i, devInfo->name);
			if (i == defInputDevIdx) printf(" (default input)");
			if (i == defOutputDevIdx) printf(" (default output)");
			printf(": API %d", hostApi);
			printf(", %d/%d max in/out channels", devInfo->maxInputChannels,
				devInfo->maxOutputChannels);
			printf(", %gms default low latency", devInfo->defaultLowInputLatency * 1e3);
			printf("\n");

			if (devInfo->maxInputChannels < settings.channels) continue;
#ifdef _DEBUG
			// seemingly not working under a debugger:
			if (!strncmp(devInfo->name, "Voicemeeter", 10)) continue;
			if (!strncmp(devInfo->name, "Yamaha", 6)) continue;
			if (strstr(devInfo->name, "ASIOVADPRO")) continue;
#endif
			if (!settings.sinkName.empty() && settings.sinkName._Equal(devInfo->name)) {
				selectedDevice = i;
				break;
			}

			// prefer ASIO devices
			if (selectedHostApi == asioApiIdx && devInfo->hostApi != asioApiIdx) continue;
			// prefer ASIO4ALL devices because they are known to work under a debugger
			if (selectedIsAsio4All) continue;
			bool isAsio4All = strncmp(devInfo->name, "ASIO4ALL", 8) == 0;
			if (!isAsio4All && selectedIsAsio4All) continue;
			if (devInfo->defaultLowInputLatency < selectedDevLatency) {
				selectedDevLatency = devInfo->defaultLowInputLatency;
				selectedDevice = i;
				selectedHostApi = devInfo->hostApi;
				selectedIsAsio4All = isAsio4All;
			}
		}
		
		printf("selected device: %d\n", selectedDevice);

        PaStreamParameters inputParameters;

		inputParameters.device = selectedDevice;
		if (inputParameters.device == paNoDevice)
			return throw std::runtime_error(LOCATION "no default input device found");

		const PaDeviceInfo* pInfo = Pa_GetDeviceInfo(inputParameters.device);
		if (devInfo != 0) {
		}

		inputParameters.channelCount = settings.channels;
		inputParameters.sampleFormat = paFloat32;
		inputParameters.suggestedLatency =
		    Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
		inputParameters.hostApiSpecificStreamInfo = NULL;

		printf("sample rate: %d\n", settings.sampleRate);
		
		_result = Pa_OpenStream(
		    &stream, &inputParameters, NULL,
		    settings.sampleRate, settings.bufferSize / settings.sampleSize,
		    paClipOff, recordCallback, this);
		if (_result == paInvalidSampleRate) {
			printf("invalid sample rate, switching to 11025\n");
			settings.sampleRate = 11025;
			_result = Pa_OpenStream(&stream, &inputParameters, NULL,
			                        settings.sampleRate, settings.bufferSize / settings.sampleSize,
			                        paClipOff, recordCallback, this);
		}

		if (_result != paNoError) return throw std::runtime_error("");

		_result = Pa_StartStream(stream);
		if (_result != paNoError) return throw std::runtime_error("");

		_DBGPRINT("portaudio init done\n");
	}

	typedef float SAMPLE;
	static int recordCallback(const void* inputBuffer, void* outputBuffer,
								  unsigned long framesPerBuffer,
								  const PaStreamCallbackTimeInfo* timeInfo,
								  PaStreamCallbackFlags statusFlags, void* userData)
	{
		auto audio = reinterpret_cast<AudioSamplerImpl*>(userData);

		(void)outputBuffer; /* Prevent unused variable warnings. */
		(void)timeInfo;
		(void)statusFlags;

		static float maxAmp = 0.f;
		const float tgtVol = 9.99f;
		static float normFac = 1.f;

		if (inputBuffer == NULL) return paContinue;
		for (int frame = 0; frame < framesPerBuffer; ++frame) {
			for (int channel = 0; channel < audio->settings.channels; ++channel) {
				float amp = *((SAMPLE*)inputBuffer + frame * audio->settings.channels + channel);
				if (std::abs(amp) > maxAmp) maxAmp = std::abs(amp);
				audio->pSampleBuffer[audio->bufPos] = normFac * amp;
				++audio->bufPos;
			}
			if (audio->bufPos >= audio->settings.sampleSize) updateBuffers(audio);
		}
		
		if (audio->settings.normalize && maxAmp > 0.f) normFac = tgtVol / maxAmp;

		return paContinue;
	}

	static void updateBuffers(AudioSamplerImpl* audio) {
		static std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
		static int numUpdates = 0;
		audio->audioMutexLock.lock();
		std::swap(audio->ppAudioBuffer[0], audio->pSampleBuffer);
		for (size_t i = 1; i * audio->settings.sampleSize < audio->settings.bufferSize; ++i)
			std::swap(audio->ppAudioBuffer[i - 1], audio->ppAudioBuffer[i]);
		audio->audioMutexLock.unlock();
		audio->modified = true;

		++numUpdates;
		std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastFrame).count() >=
		    1) {
			audio->ups = numUpdates;
			numUpdates = 0;
			lastFrame = currentTime;
		}
		audio->bufPos = 0;
	}
};

AudioSampler::AudioSampler(const Settings& audioSettings) {
	audioSamplerImpl = new AudioSamplerImpl(audioSettings);
}

AudioSampler::~AudioSampler() { delete audioSamplerImpl; }

AudioSampler& AudioSampler::operator=(AudioSampler&& other) noexcept {
	std::swap(audioSamplerImpl, other.audioSamplerImpl);
	return *this;
}

bool AudioSampler::running() const { return audioSamplerImpl->running; }

bool AudioSampler::modified() const { return audioSamplerImpl->modified; }

int AudioSampler::ups() const { return audioSamplerImpl->ups; }

void AudioSampler::copyData(AudioData& audioData) { audioSamplerImpl->copyData(audioData); }

void AudioSampler::rethrowExceptions() { return audioSamplerImpl->rethrowExceptions(); }
