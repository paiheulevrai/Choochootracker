#ifndef TAKTFUL_REF_PROFILES_H
#define TAKTFUL_REF_PROFILES_H

namespace taktful {
namespace profiles {

/* Krumhansl & Kessler (1982) probe-tone ratings, C major / C minor. */
static const float krumhansl_major[12] = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
    2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};
static const float krumhansl_minor[12] = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
    2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

/* Temperley (1999) revised profiles. */
static const float temperley1999_major[12] = {
    5.0f, 2.0f, 3.5f, 2.0f, 4.5f, 4.0f,
    2.0f, 4.5f, 2.0f, 3.5f, 1.5f, 4.0f
};
static const float temperley1999_minor[12] = {
    5.0f, 2.0f, 3.5f, 4.5f, 2.0f, 4.0f,
    2.0f, 4.5f, 3.5f, 2.0f, 1.5f, 4.0f
};

/* Temperley (2007) Music and Probability profiles (normalized-style weights). */
static const float temperley2007_major[12] = {
    0.748f, 0.060f, 0.488f, 0.082f, 0.670f, 0.460f,
    0.096f, 0.715f, 0.104f, 0.366f, 0.057f, 0.400f
};
static const float temperley2007_minor[12] = {
    0.712f, 0.084f, 0.474f, 0.618f, 0.049f, 0.460f,
    0.105f, 0.747f, 0.404f, 0.067f, 0.133f, 0.330f
};

} // namespace profiles
} // namespace taktful

#endif
