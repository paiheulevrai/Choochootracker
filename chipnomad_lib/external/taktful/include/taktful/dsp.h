#ifndef TAKTFUL_DSP_H
#define TAKTFUL_DSP_H

#include <cstdint>
#include <vector>

namespace taktful {

enum class WindowType {
    Hann,
    Hamming,
    BlackmanHarris,
};

bool is_power_of_two(uint32_t n);

void make_window(WindowType type, float* dst, uint32_t n);
void apply_window(float* x, const float* w, uint32_t n);

void rfft_mag(const float* in, float* mag_out, uint32_t n);

struct StftFrames {
    std::vector<std::vector<float> > mag;
    uint32_t fft_size = 0;
    uint32_t hop_size = 0;
    uint32_t sample_rate = 0;
};

StftFrames compute_stft(const float* samples,
                        uint32_t n_samples,
                        uint32_t sample_rate,
                        uint32_t fft_size,
                        uint32_t hop_size,
                        WindowType window = WindowType::Hann);

void downsample_linear(const float* in,
                       uint32_t n_in,
                       uint32_t sr_in,
                       uint32_t sr_out,
                       std::vector<float>& out);

float hz_to_midi(float hz, float ref_freq);
int midi_to_pitch_class(float midi);

float signal_rms(const float* x, uint32_t n);

} // namespace taktful

#endif
