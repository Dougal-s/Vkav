#include <complex>
#include <math.h>
#include <vector>

#include "SignalFunctions.hpp"

// Fast Fourier transform

static inline void separate(std::complex<float>* a, std::complex<float>* tmp, size_t n) {
    for (size_t i = 0; i < n/2; ++i)
        tmp[i] = a[i*2+1];
    for (size_t i = 0; i < n/2; ++i)
        a[i] = a[i*2];
    for (size_t i = 0; i < n/2; ++i)
        a[i + n/2] = tmp[i];
}

static void fft(std::complex<float>* a, size_t n) {
    std::complex<float> tmp[n/2];
    for (size_t i = n; i > 1; i /= 2) {
        for (size_t j = 0; j < n; j += i) {
            separate(a+j, tmp, i);
        }
    }

    for (size_t i = 2; i <= n ; i *= 2) {
        for (size_t j = 0; j < n; j += i) {
            for (size_t k = j; k < j+i/2; ++k) {
                std::complex<float> even = a[k];
                std::complex<float> odd  = a[k + i/2];

                std::complex<float> w = exp( std::complex<float>(0.f, -2.f * M_PI * k / i));

                a[k      ] = even + w * odd;
                a[k + i/2] = even - w * odd;
            }
        }
    }
}

void magnitudes(std::vector<float>& lBuffer, std::vector<float>& rBuffer) {
    std::complex<float> tmp[lBuffer.size()];
    for (size_t i = 0; i < lBuffer.size(); ++i) {
        tmp[i] = {lBuffer[i], rBuffer[i]};
    }

    fft(tmp, lBuffer.size());

    for (size_t i = 1; i < lBuffer.size()/2; ++i) {
        std::complex<float> val = (tmp[i] + std::conj(tmp[lBuffer.size() - i]))*0.5f;
        lBuffer[i]              = std::hypot(val.real(), val.imag());

        val                     = std::complex<float>(0, 1) * (val - tmp[i]);
        rBuffer[i]              = std::hypot(val.real(), val.imag());
    }
    lBuffer[0] = rBuffer[1];
    rBuffer[0] = lBuffer[1];

    lBuffer.resize(lBuffer.size()/2);
    rBuffer.resize(rBuffer.size()/2);
}

void windowFunction(std::vector<float>& lBuffer, std::vector<float>& rBuffer) {
    const float coeff = M_PI/(lBuffer.size()-1);
    for (uint32_t n = 0; n < lBuffer.size(); ++n) {
        float weight = sinf(coeff*n);
        weight *= weight;
        lBuffer[n] *= weight;
        rBuffer[n] *= weight;
    }
}

void equalise(std::vector<float>& lBuffer, std::vector<float>& rBuffer) {
    for (uint32_t n = 0; n < lBuffer.size(); ++n) {
        float weight = std::max(log10(n*0.5f), 1.0f) + 20.f*n/lBuffer.size();
        weight *= 0.002f;
        lBuffer[n] *= weight;
        rBuffer[n] *= weight;
    }
}
