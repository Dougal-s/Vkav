#include <cmath>
#include <complex>
#include <numeric>

#include "Data.hpp"
#include "Proccess.hpp"

class Proccess::ProccessImpl {
public:
	ProccessImpl(const ProccessSettings& settings) {
		channels = settings.channels;
		const size_t capacity = std::max(settings.inputSize / 2, settings.outputSize);
		lBuffer = new float[capacity];
		rBuffer = new float[capacity];

		inputSize = settings.inputSize;

		amplitude = settings.amplitude;

		wfCoeff = M_PI / (settings.inputSize - 1);

		smoothedSize = settings.outputSize;
		smoothingFactor =
		    inputSize * inputSize * 0.125f /
		    (settings.smoothingLevel * smoothedSize * settings.smoothingLevel * smoothedSize);
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
	void windowFunction(AudioData& audioData) const {
		if (channels == 1)
			windowFunction(reinterpret_cast<float*>(audioData.buffer));
		else
			windowFunction(reinterpret_cast<std::complex<float>*>(audioData.buffer));
	}

	template <class T>
	void windowFunction(T* audio) const {
		for (size_t n = 0; n < inputSize; ++n) audio[n] *= pow(sinf(wfCoeff * n), 2);
	}

	void magnitudes(AudioData& audioData) {
		std::complex<float>* input = reinterpret_cast<std::complex<float>*>(audioData.buffer);
		if (channels == 1) {
			// input has range [0, inputSize/2)
			fft(input, inputSize / 2);

			for (size_t r = 1; r < inputSize / 2; ++r) {
				std::complex<float> F = 0.5f * (input[r] + std::conj(input[inputSize / 2 - r]));
				std::complex<float> G =
				    std::complex<float>(0, 0.5f) * (std::conj(input[inputSize / 2 - r]) - input[r]);

				std::complex<float> w = exp(std::complex<float>(0.f, -2.f * M_PI * r / inputSize));
				std::complex<float> X = F + w * G;

				audioData.lBuffer[r] = std::abs(X);
				audioData.rBuffer[r] = audioData.lBuffer[r];
			}
		} else {
			// input has range [0, inputSize)
			fft(input, inputSize);

			for (size_t i = 1; i < inputSize / 2; ++i) {
				std::complex<float> val = (input[i] + std::conj(input[inputSize - i])) * 0.5f;
				audioData.lBuffer[i] = std::abs(val);

				val = std::complex<float>(0, 0.5f) * (std::conj(input[inputSize - i]) - input[i]);
				audioData.rBuffer[i] = std::abs(val);
			}
		}
		audioData.lBuffer[0] = audioData.rBuffer[1];
		audioData.rBuffer[0] = audioData.lBuffer[1];
	}

	void equalise(AudioData& audioData) const {
		for (size_t n = 0; n < inputSize / 2; ++n) {
			float weight = 0.08f * amplitude * log10f(2.f * n / inputSize + 1.05f);
			audioData.lBuffer[n] *= weight;
			audioData.rBuffer[n] *= weight;
		}
	}

	void calculateVolume(AudioData& audioData) const {
		audioData.lVolume =
		    std::accumulate(audioData.lBuffer, audioData.lBuffer + inputSize / 2, 0.f) / inputSize;
		audioData.rVolume =
		    std::accumulate(audioData.rBuffer, audioData.rBuffer + inputSize / 2, 0.f) / inputSize;
	}

	void smooth(AudioData& audioData) { kernelSmooth(audioData); }

	void kernelSmooth(AudioData& audioData) {
		const float oldSize = inputSize / 2;
		const float radius = sqrtf(-logf(0.05f) / smoothingFactor) * oldSize / smoothedSize;

		for (uint32_t i = 0; i < smoothedSize; ++i) {
			float sum = 0;
			uint32_t min = std::max((int)0, (int)(i * oldSize / smoothedSize - radius));
			uint32_t max = std::min((int)oldSize, (int)(i * oldSize / smoothedSize + radius));
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

	// Bit Reverse Radix-2 fft

	static void fft(std::complex<float>* first, size_t size) {
		std::complex<float> reversed[size];
		bit_reverse_copy(first, size, reversed);

		for (size_t s = 1; s < log2(size) + 1; ++s) {
			size_t m = 1 << s;
			std::complex<float> wm = std::exp(std::complex<float>(0.0, -2.0 * M_PI / m));
			for (size_t k = 0; k < size; k += m) {
				std::complex<float> w = 1;
				for (size_t j = 0; j < m / 2; ++j) {
					std::complex<float> t = w * reversed[k + j + m / 2];
					std::complex<float> u = reversed[k + j];
					reversed[k + j] = u + t;
					reversed[k + j + m / 2] = u - t;
					w *= wm;
				}
			}
		}

		for (size_t i = 0; i < size; ++i) first[i] = reversed[i];
	}

	static void bit_reverse_copy(std::complex<float>* in, size_t size, std::complex<float>* out) {
		for (size_t i = 0; i < size; ++i) out[reverse_bits(i, log2(size))] = in[i];
	}

	static size_t reverse_bits(size_t val, uint8_t exp) {
		size_t reversed = 0;
		for (uint8_t i = 0; i < exp; ++i) {
			reversed <<= 1;
			reversed += (val & (1 << i)) != 0;
		}
		return reversed;
	}
};

void Proccess::init(const ProccessSettings& proccessSettings) {
	impl = new ProccessImpl(proccessSettings);
}

void Proccess::proccessSignal(AudioData& audioData) { impl->proccessSignal(audioData); }

void Proccess::cleanup() { delete impl; }
