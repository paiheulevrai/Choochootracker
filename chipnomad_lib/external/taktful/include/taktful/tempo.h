#ifndef TAKTFUL_TEMPO_H
#define TAKTFUL_TEMPO_H

#include <taktful/api.h>

namespace taktful {

struct TempoDebug {
    TempoResult result;
    std::vector<float> resampled_onset;
    std::vector<float> acorr;
    float acorr_dt = 0.01f;
    int tau_min = 0;
    int tau_max = 0;
};

TempoDebug estimate_tempo_debug(const std::vector<float>& onset_env,
                                float hop_sec,
                                const Params& params);

TempoResult estimate_tempo(const std::vector<float>& onset_env,
                           float hop_sec,
                           const Params& params);

} // namespace taktful

#endif
