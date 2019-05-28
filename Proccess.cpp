#include <cmath>
#include <complex>
#include <numeric>

#include "Data.hpp"
#include "Proccess.hpp"

class Proccess::ProccessImpl {
public:
	ProccessImpl(const ProccessSettings& settings) {
		channels = settings.channels;
		const size_t capacity =
		    std::max(settings.inputSize / 2, settings.outputSize);
		lBuffer = new float[capacity];
		rBuffer = new float[capacity];

		inputSize = settings.inputSize;

		amplitude = settings.amplitude;

		wfCoeff = M_PI / (settings.inputSize - 1);

		smoothedSize = settings.outputSize;
		smoothingFactor = inputSize * inputSize * 0.125f /
		                  (settings.smoothingLevel * smoothedSize *
		                   settings.smoothingLevel * smoothedSize);
	}

	void proccessSignal(AudioData& audioData) {
		windowFunction(audioData);
		magnitudes(audioData);
		equalise(audioData);
		calculateVolume(audioData);
		if (!std::isnan(smoothingFactor)) smooth(audioData);
	}

	~ProccessImpl() {
		delete[] lBuffer;
		delete[] rBuffer;
	}

private:
	// Member variables
	float* lBuffer;
	float* rBuffer;

	size_t inputSize;
	unsigned char channels;

	float amplitude;

	// window function
	float wfCoeff;

	// smooth
	size_t smoothedSize;
	float smoothingFactor;

	// Member functions
	void windowFunction(AudioData& audioData) {
		if (channels == 1)
			windowFunction(reinterpret_cast<float*>(audioData.buffer));
		else
			windowFunction(
			    reinterpret_cast<std::complex<float>*>(audioData.buffer));
	}

	template <class T>
	void windowFunction(T* audio) {
		for (size_t n = 0; n < inputSize; ++n)
			audio[n] *= pow(sinf(wfCoeff * n), 2);
	}

	void magnitudes(AudioData& audioData) {
		std::complex<float>* input =
		    reinterpret_cast<std::complex<float>*>(audioData.buffer);
		if (channels == 1) {
			// input has range [0, inputSize/2)
			fft(input, inputSize / 2);

			for (size_t r = 1; r < inputSize / 2; ++r) {
				std::complex<float> F =
				    0.5f * (input[r] + std::conj(input[inputSize / 2 - r]));
				std::complex<float> G =
				    std::complex<float>(0, 0.5f) *
				    (std::conj(input[inputSize / 2 - r]) - input[r]);

				std::complex<float> w =
				    exp(std::complex<float>(0.f, -2.f * M_PI * r / inputSize));
				std::complex<float> X = F + w * G;

				audioData.lBuffer[r] = std::abs(X);
				audioData.rBuffer[r] = audioData.lBuffer[r];
			}
		} else {
			// input has range [0, inputSize)
			fft(input, inputSize);

			for (size_t i = 1; i < inputSize / 2; ++i) {
				std::complex<float> val =
				    (input[i] + std::conj(input[inputSize - i])) * 0.5f;
				audioData.lBuffer[i] = std::abs(val);

				val = std::complex<float>(0, 0.5f) *
				      (std::conj(input[inputSize - i]) - input[i]);
				audioData.rBuffer[i] = std::abs(val);
			}
		}
		audioData.lBuffer[0] = audioData.rBuffer[1];
		audioData.rBuffer[0] = audioData.lBuffer[1];
	}

	void equalise(AudioData& audioData) {
		for (size_t n = 0; n < inputSize / 2; ++n) {
			float weight =
			    0.08f * amplitude * log10f(2.f * n / inputSize + 1.05f);
			audioData.lBuffer[n] *= weight;
			audioData.rBuffer[n] *= weight;
		}
	}

	void calculateVolume(AudioData& audioData) const {
		audioData.lVolume =
		    std::accumulate(audioData.lBuffer,
		                    audioData.lBuffer + inputSize / 2, 0.f) /
		    inputSize;
		audioData.rVolume =
		    std::accumulate(audioData.rBuffer,
		                    audioData.rBuffer + inputSize / 2, 0.f) /
		    inputSize;
	}

	void smooth(AudioData& audioData) { kernelSmooth(audioData); }

	void kernelSmooth(AudioData& audioData) {
		const float oldSize = inputSize / 2;
		const float radius =
		    sqrtf(-logf(0.05f) / smoothingFactor) * oldSize / smoothedSize;

		for (uint32_t i = 0; i < smoothedSize; ++i) {
			float sum = 0;
			uint32_t min =
			    std::max((int)0, (int)(i * oldSize / smoothedSize - radius));
			uint32_t max = std::min((int)oldSize,
			                        (int)(i * oldSize / smoothedSize + radius));
			lBuffer[i] = 0.f;
			rBuffer[i] = 0.f;
			for (uint32_t j = min; j < max; ++j) {
				float distance = (i - j * smoothedSize / oldSize);
				float weight = expf(-distance * distance * smoothingFactor);
				lBuffer[i] += audioData.lBuffer[j] * weight;
				rBuffer[i] += audioData.rBuffer[j] * weight;
				sum += weight;
			}
			lBuffer[i] /= sum;
			rBuffer[i] /= sum;
		}

		std::swap(audioData.lBuffer, lBuffer);
		std::swap(audioData.rBuffer, rBuffer);
	}

	// Static member functions
	static void fft(std::complex<float>* a, size_t n) {
		for (size_t i = n; i > 1; i /= 2)
			for (size_t j = 0; j < n; j += i) separate(a + j, i);

		for (size_t i = 2; i <= n; i *= 2) {
			for (size_t j = 0; j < n; j += i) {
				for (size_t k = j; k < j + i / 2; ++k) {
					std::complex<float> even = a[k];
					std::complex<float> odd = a[k + i / 2];

					std::complex<float> w =
					    exp(std::complex<float>(0.f, -2.f * M_PI * k / i));

					a[k] = even + w * odd;
					a[k + i / 2] = even - w * odd;
				}
			}
		}
	}

	static void separate(std::complex<float>* a, size_t n) {
		std::complex<float> tmp[n / 2];
		for (size_t i = 0; i < n / 2; ++i) tmp[i] = a[i * 2 + 1];
		for (size_t i = 0; i < n / 2; ++i) a[i] = a[i * 2];
		for (size_t i = 0; i < n / 2; ++i) a[i + n / 2] = tmp[i];
	}
};

void Proccess::init(const ProccessSettings& proccessSettings) {
	impl = new ProccessImpl(proccessSettings);
}

void Proccess::proccessSignal(AudioData& audioData) {
	impl->proccessSignal(audioData);
}

void Proccess::cleanup() { delete impl; }
