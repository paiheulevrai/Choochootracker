#include <taktful/key.h>

#include "ref_profiles.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace taktful {
namespace {

const char* k_key_names[25] = {
    "C major", "C# major", "D major", "D# major", "E major", "F major",
    "F# major", "G major", "G# major", "A major", "A# major", "B major",
    "C minor", "C# minor", "D minor", "D# minor", "E minor", "F minor",
    "F# minor", "G minor", "G# minor", "A minor", "A# minor", "B minor",
    "Unknown"
};

const char* k_camelot[25] = {
    "8B", "3B", "10B", "5B", "12B", "7B",
    "2B", "9B", "4B", "11B", "6B", "1B",
    "5A", "12A", "7A", "2A", "9A", "4A",
    "11A", "6A", "1A", "8A", "3A", "10A",
    ""
};

void rotate_profile(const float* src, int shift, float* dst) {
    for (int i = 0; i < 12; ++i) {
        dst[i] = src[(i - shift + 12) % 12];
    }
}

float pearson(const float* a, const float* b, int n) {
    double ma = 0.0;
    double mb = 0.0;
    for (int i = 0; i < n; ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= n;
    mb /= n;
    double num = 0.0;
    double da = 0.0;
    double db = 0.0;
    for (int i = 0; i < n; ++i) {
        const double xa = a[i] - ma;
        const double xb = b[i] - mb;
        num += xa * xb;
        da += xa * xa;
        db += xb * xb;
    }
    const double den = std::sqrt(da * db);
    if (den < 1.0e-12) {
        return 0.0f;
    }
    return static_cast<float>(num / den);
}

} // namespace

const char* to_string(Key key) {
    const int i = static_cast<int>(key);
    if (i < 0 || i > 24) {
        return "Unknown";
    }
    return k_key_names[i];
}

const char* to_camelot(Key key) {
    const int i = static_cast<int>(key);
    if (i < 0 || i > 24) {
        return "";
    }
    return k_camelot[i];
}

KeyResult estimate_key(const std::vector<float>& pcp, const Params& params) {
    KeyResult result;
    if (pcp.size() < 12) {
        return result;
    }

    float v[12];
    double sum = 0.0;
    for (int i = 0; i < 12; ++i) {
        v[i] = pcp[static_cast<size_t>(i)];
        sum += std::fabs(static_cast<double>(v[i]));
    }
    if (sum < 1.0e-12) {
        return result;
    }

    const float* maj = profiles::temperley1999_major;
    const float* minr = profiles::temperley1999_minor;
    switch (params.key_profiles) {
    case Params::KeyProfiles::KrumhanslOriginal:
        maj = profiles::krumhansl_major;
        minr = profiles::krumhansl_minor;
        break;
    case Params::KeyProfiles::Temperley2007:
        maj = profiles::temperley2007_major;
        minr = profiles::temperley2007_minor;
        break;
    case Params::KeyProfiles::Temperley1999:
    default:
        break;
    }

    float scores[24];
    float rot[12];
    for (int k = 0; k < 12; ++k) {
        rotate_profile(maj, k, rot);
        scores[k] = pearson(v, rot, 12);
        rotate_profile(minr, k, rot);
        scores[12 + k] = pearson(v, rot, 12);
    }

    int order[24];
    for (int i = 0; i < 24; ++i) {
        order[i] = i;
    }
    std::sort(order, order + 24, [&](int a, int b) {
        return scores[a] > scores[b];
    });

    result.key = static_cast<Key>(order[0]);
    result.camelot = to_camelot(result.key);
    const float r1 = scores[order[0]];
    const float r2 = scores[order[1]];
    float conf = (r1 - r2) * 0.5f + 0.5f;
    if (conf < 0.0f) conf = 0.0f;
    if (conf > 1.0f) conf = 1.0f;
    if (r1 < 0.0f) {
        conf *= 0.5f;
    }
    result.confidence = conf;

    if (params.return_alternatives) {
        int n = static_cast<int>(params.num_alternatives);
        if (n < 0) n = 0;
        if (n > 24) n = 24;
        result.alternatives.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            KeyResult::Alternative a;
            a.key = static_cast<Key>(order[i]);
            float c = scores[order[i]];
            if (c < 0.0f) c = 0.0f;
            if (c > 1.0f) c = 1.0f;
            a.confidence = c * result.confidence / (scores[order[0]] > 1.0e-6f ? scores[order[0]] : 1.0f);
            if (a.confidence > 1.0f) a.confidence = 1.0f;
            if (a.confidence < 0.0f) a.confidence = 0.0f;
            a.camelot = to_camelot(a.key);
            result.alternatives.push_back(a);
        }
    }
    return result;
}

} // namespace taktful
