#ifndef TAKTFUL_SIMD_H
#define TAKTFUL_SIMD_H

#if defined(TAKTFUL_ENABLE_NEON) && (TAKTFUL_ENABLE_NEON == 0)
#define TAKTFUL_NEON 0
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define TAKTFUL_NEON 1
#include <arm_neon.h>
#else
#define TAKTFUL_NEON 0
#endif

#endif
