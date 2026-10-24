#ifndef TAKTFUL_KEY_H
#define TAKTFUL_KEY_H

#include <taktful/api.h>

namespace taktful {

KeyResult estimate_key(const std::vector<float>& pcp, const Params& params);

} // namespace taktful

#endif
