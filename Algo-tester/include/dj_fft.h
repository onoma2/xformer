// Minimal FFT implementation for spectrum analysis
// Based on the dj_fft.h from the existing Curve Analysis tool

#ifndef DJ_FFT_H
#define DJ_FFT_H

#include <vector>
#include <complex>
#include <cmath>

namespace dj {
    enum class fft_dir { DIR_FWD = 1, DIR_INV = -1 };

    template<typename T>
    struct fft_arg {
        std::vector<std::complex<T>> data;

        fft_arg(size_t size) : data(size) {}

        std::complex<T>& operator[](size_t idx) { return data[idx]; }
        const std::complex<T>& operator[](size_t idx) const { return data[idx]; }

        size_t size() const { return data.size(); }
    };

    template<typename T>
    fft_arg<T> fft1d(fft_arg<T>& input, fft_dir direction) {
        size_t n = input.size();
        fft_arg<T> output(n);

        T theta = 2.0 * M_PI / n;
        if (direction == fft_dir::DIR_INV) theta = -theta;

        for (size_t k = 0; k < n; ++k) {
            std::complex<T> sum(0, 0);
            for (size_t j = 0; j < n; ++j) {
                T real = cos(j * k * theta);
                T imag = sin(j * k * theta);
                std::complex<T> twiddle(real, imag);
                sum += input[j] * twiddle;
            }
            output[k] = sum;
        }

        if (direction == fft_dir::DIR_INV) {
            for (size_t i = 0; i < n; ++i) {
                output[i] /= n;
            }
        }

        return output;
    }
}

#endif // DJ_FFT_H