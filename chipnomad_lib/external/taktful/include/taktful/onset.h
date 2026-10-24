#ifndef TAKTFUL_ONSET_H
#define TAKTFUL_ONSET_H

#include <cstdint>
#include <vector>

namespace taktful {

std::vector<float> compute_onset_envelope(const std::vector<std::vector<float> >& mag_frames,
                                          bool hfc_weight);

} // namespace taktful

#endif
