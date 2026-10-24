#ifndef TAKTFUL_STREAM_H
#define TAKTFUL_STREAM_H

#include <cstddef>
#include <taktful/api.h>

namespace taktful {

class StreamAnalyzer {
public:
    explicit StreamAnalyzer(const Params& params = Params());

    void reset();
    void push(const float* samples, uint32_t n, uint32_t sample_rate);
    AnalysisResult finish();
    IntermediateFeatures features() const;

    uint32_t frames_processed() const;
    uint32_t sample_rate() const;

private:
    Params params_;
    uint32_t input_sr_;
    uint32_t analysis_sr_;
    std::vector<float> in_hold_;
    std::vector<float> pending_;
    size_t pending_pos_;
    std::vector<float> window_;
    std::vector<float> prev_db_;
    std::vector<float> onset_;
    std::vector<std::vector<float> > chroma_;
    bool have_prev_;
    bool finished_;

    void ensure_window();
    void process_pending();
};

AnalysisResult analyze_buffer(const float* samples,
                              uint32_t n_samples,
                              uint32_t sample_rate,
                              const Params& params = Params());

std::vector<AnalysisResult> analyze_sections(const WavData& audio,
                                             float section_sec,
                                             const Params& params = Params());

} // namespace taktful

#endif
