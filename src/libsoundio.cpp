#ifdef LIBSOUNDIO
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

// libsoundio
#include <soundio/soundio.h>

#include "Audio.hpp"
#include "Data.hpp"

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__) ": "

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

		initSoundIo();
		running = true;
	}

	~AudioSamplerImpl() {
		soundio_instream_destroy(stream);
		soundio_device_unref(device);
		soundio_destroy(soundio);

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

	// libsoundio
	SoundIo* soundio = nullptr;
	SoundIoDevice* device = nullptr;
	SoundIoInStream* stream;

	uint32_t sampleRate;

	int error;

	void initSoundIo() {
		soundio = soundio_create();
		if (!soundio) throw std::runtime_error(LOCATION "Out of memory!");

		soundio->app_name = "Vkav";

		if ((error = soundio_connect(soundio)))
			throw std::runtime_error(LOCATION "failed to connect soundio instance: " +
			                         std::string(soundio_strerror(error)));

		soundio_flush_events(soundio);

		if (settings.sinkName.empty()) {
			int deviceID = soundio_default_input_device_index(soundio);
			device = soundio_get_input_device(soundio, deviceID);
		} else {
			int deviceCount = soundio_input_device_count(soundio);
			for (int i = 0; i < deviceCount; ++i) {
				if (soundio_get_input_device(soundio, i)->name == std::string(settings.sinkName)) {
					device = soundio_get_input_device(soundio, i);
					break;
				}
			}
		}
		if (!device) throw std::runtime_error(LOCATION "Unable to find input devices!");

		soundio_device_sort_channel_layouts(device);

		SoundIoChannelLayout* layout = nullptr;
		for (int i = 0; i < device->layout_count; ++i) {
			if (device->layouts[i].channel_count == settings.channels) {
				layout = device->layouts + i;
				break;
			}
		}

		if (!layout)
			throw std::runtime_error(LOCATION
			                         "Selected device does not support the chosen channel layout!");

		if (device->probe_error)
			throw std::runtime_error(LOCATION "Soundio probe error!:" +
			                         std::string(soundio_strerror(device->probe_error)));

		sampleRate = settings.sampleRate;
		if (!soundio_device_supports_sample_rate(device, settings.sampleRate)) {
			int sampleRateID = 0;
			while (device->sample_rates[sampleRateID].max == 0) {
				++sampleRateID;
				if (sampleRateID == device->sample_rate_count)
					throw std::runtime_error(LOCATION "Unable to find non-zero sample rate!");
			}

			if (settings.sampleRate > static_cast<uint32_t>(device->sample_rates[sampleRateID].max))
				sampleRate = device->sample_rates[sampleRateID].max;
			else
				sampleRate = device->sample_rates[sampleRateID].min;
		}

		if (!soundio_device_supports_format(device, SoundIoFormatFloat32NE))
			throw std::runtime_error(
			    LOCATION "Selected device does not support 32 bit floating point audio!");

		stream = soundio_instream_create(device);
		if (!stream) throw std::runtime_error(LOCATION "Out of memory!");

		stream->format = SoundIoFormatFloat32LE;
		stream->sample_rate = sampleRate;
		stream->read_callback = readCallback;
		stream->userdata = this;
		stream->software_latency =
		    sizeof(float) * settings.sampleSize * sampleRate / settings.sampleRate;
		stream->name = "Vkav";
		stream->layout = *layout;

		if ((error = soundio_instream_open(stream)))
			throw std::runtime_error(LOCATION "Failed to open input stream!");

		if ((error = soundio_instream_start(stream)))
			throw std::runtime_error(LOCATION "Failed to start input stream!");

		soundio_flush_events(soundio);
	}

	static void readCallback(SoundIoInStream* instream, [[maybe_unused]] int frameCountMin,
	                         int frameCountMax) {
		auto audio = reinterpret_cast<AudioSamplerImpl*>(instream->userdata);
		SoundIoChannelArea* areas;

		for (int framesLeft = frameCountMax; framesLeft > 0;) {
			int frameCount = framesLeft;
			if ((audio->error = soundio_instream_begin_read(instream, &areas, &frameCount))) {
				audio->exceptionPtr = std::make_exception_ptr(
				    std::runtime_error(LOCATION "Failed to read audio from soundio stream!: " +
				                       std::string(soundio_strerror(audio->error))));
				audio->running = false;
				return;
			}

			if (areas) {
				for (int frame = 0;
				     frame * audio->sampleRate < frameCount * audio->settings.sampleRate; ++frame) {
					for (int channel = 0; channel < instream->layout.channel_count; ++channel) {
						audio->pSampleBuffer[audio->bufPos] = *reinterpret_cast<float*>(
						    areas[channel].ptr +
						    areas[channel].step *
						        (frame * audio->sampleRate / audio->settings.sampleRate));
						++audio->bufPos;
					}
					if (audio->bufPos >= audio->settings.sampleSize) updateBuffers(audio);
				}
			}

			if ((audio->error = soundio_instream_end_read(instream))) {
				audio->exceptionPtr = std::make_exception_ptr(
				    std::runtime_error(LOCATION "Soundio read error!: " +
				                       std::string(soundio_strerror(audio->error))));
				audio->running = false;
				return;
			}

			framesLeft -= frameCount;
		}
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
