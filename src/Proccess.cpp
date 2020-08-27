#include <cmath>
#include <complex>
#include <numeric>
#include <utility>

#include "Data.hpp"
#include "Proccess.hpp"

class Proccess::ProccessImpl {
public:
	ProccessImpl(const ProccessSettings& settings) {
		channels = settings.channels;
		inputSize = settings.size;
		amplitude = settings.amplitude;
		smooth = settings.smoothingLevel;

		if (smooth) {
			// generate convolutionVec
			convolutionVec = new std::complex<float>[inputSize / 2];
			const float smoothingFactor = 1.f / (settings.smoothingLevel * settings.smoothingLevel);
			float sum = 0;
			for (size_t i = 0; i < inputSize / 2; ++i) {
				float dx = static_cast<float>(2 * i) / inputSize;
				convolutionVec[i].real(std::exp(-dx * dx * smoothingFactor) +
				                       std::exp(-(1.f - dx) * (1.f - dx) * smoothingFactor));
				dx = static_cast<float>(2 * i + 1) / inputSize;
				convolutionVec[i].imag(std::exp(-dx * dx * smoothingFactor) +
				                       std::exp(-(1.f - dx) * (1.f - dx) * smoothingFactor));

				sum += convolutionVec[i].real() + convolutionVec[i].imag();
			}
			for (size_t i = 0; i < inputSize / 2; ++i) convolutionVec[i] /= sum;

			// precompute fft
			fft(convolutionVec, inputSize / 2);

			convolutionVec[0] = convolutionVec[0].imag() + convolutionVec[0].real();
			convolutionVec[inputSize / 4] =
			    convolutionVec[inputSize / 4].imag() + convolutionVec[inputSize / 4].real();
			const std::complex<float> wm =
			    std::exp(std::complex<float>(0.f, -2.f * M_PI / inputSize));
			std::complex<float> w = wm;
			for (size_t r = 1; r < inputSize / 4; ++r) {
				auto F1 = 0.5f * (convolutionVec[r] + std::conj(convolutionVec[inputSize / 2 - r]));
				auto G1 = std::complex<float>(0, 0.5f) *
				          (std::conj(convolutionVec[inputSize / 2 - r]) - convolutionVec[r]);

				auto F2 = 0.5f * (convolutionVec[inputSize / 2 - r] + std::conj(convolutionVec[r]));
				auto G2 = std::complex<float>(0, 0.5f) *
				          (std::conj(convolutionVec[r]) - convolutionVec[inputSize / 2 - r]);

				convolutionVec[r] = F1 + w * G1;
				convolutionVec[inputSize / 2 - r] = F2 - G2 / w;

				w *= wm;
			}
		}

		wfCoeff = M_PI / (settings.size - 1);
	}

	void proccessSignal(AudioData& audioData) {
		windowFunction(audioData);
		magnitudes(audioData);
		equalise(audioData);
		calculateVolume(audioData);
		if (smooth) smoothBuffer(audioData);
	}

	~ProccessImpl() {
		if (smooth) delete[] convolutionVec;
	}

private:
	// Member variables

	// for smoothing
	std::complex<float>* convolutionVec;
	bool smooth;

	size_t inputSize;
	unsigned char channels;

	float amplitude;

	// window function
	float wfCoeff;

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

			audioData.lBuffer[0] = audioData.rBuffer[0] = input[0].imag() + input[0].real();

			const std::complex<float> wm =
			    std::exp(std::complex<float>(0.f, -2.f * M_PI / inputSize));
			std::complex<float> w = wm;
			for (size_t r = 1; r < inputSize / 2; ++r) {
				auto F = 0.5f * (input[r] + std::conj(input[inputSize / 2 - r]));
				auto G =
				    std::complex<float>(0, 0.5f) * (std::conj(input[inputSize / 2 - r]) - input[r]);

				audioData.lBuffer[r] = audioData.rBuffer[r] = std::abs(F + w * G);
				w *= wm;
			}
		} else {
			// input has range [0, inputSize)
			fft(input, inputSize);

			audioData.lBuffer[0] = input[0].real();
			audioData.rBuffer[0] = input[0].imag();

			for (size_t i = 1; i < inputSize / 2; ++i) {
				std::complex<float> val = 0.5f * (std::conj(input[inputSize - i]) + input[i]);
				audioData.lBuffer[i] = std::abs(val);

				val = std::complex<float>(0, 0.5f) * (std::conj(input[inputSize - i]) - input[i]);
				audioData.rBuffer[i] = std::abs(val);
			}
		}
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

	/**
	 * Performs a fast convolution between the input audio and convolutionVec
	 */
	void smoothBuffer(AudioData& audioData) {
		auto input = reinterpret_cast<std::complex<float>*>(audioData.buffer);
		for (size_t i = 0; i < inputSize / 2; ++i) {
			input[i] = {audioData.lBuffer[i], audioData.rBuffer[i]};
			input[i + inputSize / 2] = {audioData.lBuffer[inputSize / 2 - i - 1],
			                            audioData.rBuffer[inputSize / 2 - i - 1]};
		}
		fft(input, inputSize);

		input[0] *= convolutionVec[0];
		for (size_t i = 1; i < inputSize / 2; ++i) {
			input[i] *= convolutionVec[i];
			input[inputSize - i] *= convolutionVec[i];
		}
		ifft(input, inputSize);
		for (size_t i = 0; i < inputSize / 2; ++i) {
			audioData.lBuffer[i] = input[i].real();
			audioData.rBuffer[i] = input[i].imag();
		}
	}

	// Static member functions

	/**
	 * Performs an ifft using the fft
	 * Requires the input buffer size to be a power of 2
	 */

	static void ifft(std::complex<float>* first, const size_t size) {
		for (size_t i = 0; i < size; ++i) first[i] = std::conj(first[i]);
		fft(first, size);
		for (size_t i = 0; i < size; ++i) first[i] = std::conj(first[i]) / static_cast<float>(size);
	}

	/**
	 * Performs bit reverse fft
	 * Requires the input buffer size to be a power of 2
	 */

	static void fft(std::complex<float>* first, const size_t size) {
		bitReverseShuffle(first, size);
		for (size_t m = 2; m <= size; m <<= 1) {
			const std::complex<float> wm = std::exp(std::complex<float>(0.f, -2.0 * M_PI / m));
			for (size_t k = 0; k < size; k += m) {
				std::complex<float> w = 1;
				for (size_t j = 0; 2 * j < m; ++j, w *= wm) {
					const std::complex<float> t = w * first[k + j + m / 2];
					const std::complex<float> u = first[k + j];
					first[k + j] = u + t;
					first[k + j + m / 2] = u - t;
				}
			}
		}
	}

	static void bitReverseShuffle(std::complex<float>* first, size_t size) {
/**
 * num_bits = log2(size).
 * here i am assuming that the fft size is less than 2^16,
 * that it is not 0 and that it is a power of 2
 */
#ifdef __builtin_ctz
		uint8_t numBits = __builtin_ctz(size);  // count number of trailing 0s
#else
		uint8_t numBits = 15;
		if (size & 0x00FF) numBits -= 8;
		if (size & 0x0F0F) numBits -= 4;
		if (size & 0x3333) numBits -= 2;
		if (size & 0x5555) numBits -= 1;
#endif

		for (size_t i = 0; i < size; ++i) {
			size_t j = reverseBits(i, numBits);
			if (i < j) std::swap(first[i], first[j]);
		}
	}

	/**
	 * Reverses the first n bits in val
	 */
	static size_t reverseBits(size_t val, uint8_t n) {
		size_t reversed = 0;
		for (uint8_t i = 0; i < n; ++i) {
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
