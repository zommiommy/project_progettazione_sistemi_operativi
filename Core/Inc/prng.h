#pragma once

#include <stdint.h>

uint64_t splitmix64_seed = 0xbad5eed;

uint64_t nextSplitmix64() {
	uint64_t z = (splitmix64_seed += UINT64_C(0x9E3779B97F4A7C15));
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}

static inline uint32_t rotl(const uint32_t x, int k) {
	return (x << k) | (x >> (32 - k));
}

/// prng state, it's a prng so everything is deterministic and
/// I can debug it, for a real application you might want a real rng.
static uint32_t s[4];

/// xoroshiro128+ from https://prng.di.unimi.it/xoshiro128plus.c
uint32_t nextRand(void) {
	const uint32_t result = s[0] + s[3];

	const uint32_t t = s[1] << 9;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;

	s[3] = rotl(s[3], 11);

	return result;
}

static inline void initRand() {
  for (int i = 0; i < 4; i++) {
    s[i] = nextSplitmix64();
  }
}
