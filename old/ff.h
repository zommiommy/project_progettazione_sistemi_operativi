#pragma once

#include <stdint.h>

#define FF_SIZE 256
#define FF_BYTES (FF_SIZE / 8)
#define FF_WORDS (FF_SIZE / 32)
#define FF_LAST_WORD (FF_WORDS - 1)

// Represents a 256-bit integer as an array of 32-bit words
// Using 32-bit words instead of 64-bit since STM32F411E is a 32-bit architecture
typedef struct {
    uint32_t words[FF_WORDS];  // Little-endian representation
} ff_t;

// Initialize ff_t from a 32-bit value
static inline void ff_from_u32(ff_t* result, uint32_t value) {
    result->words[0] = value;
    for (int i = 1; i < FF_WORDS; i++) {
        result->words[i] = 0;
    }
}

// Set ff_t to zero - using word-aligned accesses
static inline void ff_zero(ff_t* result) {
    for (int i = 0; i < FF_WORDS; i++) {
        result->words[i] = 0;
    }
}

// Compare two ff_t values
static inline int ff_eq(const ff_t* a, const ff_t* b) {
    int equal = 1;
    for (int i = 0; i < FF_WORDS; i++) {
        if (a->words[i] != b->words[i]) {
            equal = 0;
            break;
        }
    }
    return equal;
}

// Compare with zero
static inline int ff_is_zero(const ff_t* a) {
    for (int i = 0; i < FF_WORDS; i++) {
        if (a->words[i] != 0) return 0;
    }
    return 1;
}


// Check if a ff_t number is negative (used for modular arithmetic)
static inline int ff_is_negative(const ff_t* a) {
    // In our representation, a number is negative if its highest bit is set
    return (a->words[FF_LAST_WORD] >> 31) & 1;
}

// Compare two ff_t values: returns -1 if a < b, 0 if a == b, 1 if a > b
static inline int ff_cmp(const ff_t* a, const ff_t* b) {
    for (int i = FF_LAST_WORD; i >= 0; i--) {
        if (a->words[i] > b->words[i]) return 1;
        if (a->words[i] < b->words[i]) return -1;
    }
    return 0;
}

// Left shift by n bits
static inline void ff_shl(ff_t* result, const ff_t* a, int n) {
    if (n >= FF_SIZE || n <= 0) {
        if (n == 0) *result = *a;
        else ff_zero(result);
        return;
    }

    int word_shift = n / 32;
    int bit_shift = n % 32;

    if (bit_shift == 0) {
        // Word-aligned shift
        for (int i = FF_LAST_WORD; i >= word_shift; i--) {
            result->words[i] = a->words[i - word_shift];
        }
        for (int i = word_shift - 1; i >= 0; i--) {
            result->words[i] = 0;
        }
    } else {
        // Handle partial word shift
        for (int i = FF_LAST_WORD; i >= word_shift + 1; i--) {
            result->words[i] = (a->words[i - word_shift] << bit_shift) |
                              (a->words[i - word_shift - 1] >> (32 - bit_shift));
        }
        result->words[word_shift] = a->words[0] << bit_shift;
        for (int i = word_shift - 1; i >= 0; i--) {
            result->words[i] = 0;
        }
    }
}

// Right shift by n bits
static inline void ff_shr(ff_t* result, const ff_t* a, int n) {
    if (n >= FF_SIZE || n <= 0) {
        if (n == 0) *result = *a;
        else ff_zero(result);
        return;
    }

    int word_shift = n / 32;
    int bit_shift = n % 32;

    if (bit_shift == 0) {
        // Word-aligned shift
        for (int i = 0; i < FF_WORDS - word_shift; i++) {
            result->words[i] = a->words[i + word_shift];
        }
        for (int i = FF_WORDS - word_shift; i < FF_WORDS; i++) {
            result->words[i] = 0;
        }
    } else {
        // Handle partial word shift
        for (int i = 0; i < FF_LAST_WORD - word_shift; i++) {
            result->words[i] = (a->words[i + word_shift] >> bit_shift) |
                              (a->words[i + word_shift + 1] << (32 - bit_shift));
        }
        result->words[FF_LAST_WORD - word_shift] = a->words[FF_LAST_WORD] >> bit_shift;
        for (int i = FF_WORDS - word_shift; i < FF_WORDS; i++) {
            result->words[i] = 0;
        }
    }
}

// Count leading zeros in ff_t
static inline int ff_clz(const ff_t* a) {
    int total = 0;
    
    // Find first non-zero word
    int i;
    for (i = FF_LAST_WORD; i >= 0 && a->words[i] == 0; i--) {
        total += 32;
    }
    
    if (i < 0) return FF_SIZE;
    
    // Count leading zeros in the first non-zero word
    uint32_t word = a->words[i];
    for (int b = 31; b >= 0; b--) {
        if (word & (1U << b)) break;
        total++;
    }
    
    return total;
}

// Convert ff_t to hex string. Buffer must be 64 chars
static inline void ff_to_hex(uint8_t* buffer, const ff_t* a) {
    // Process each word from most significant to least significant
    for (int i = FF_LAST_WORD; i >= 0; i--) {
        // Process each byte in the word
        for (int j = 3; j >= 0; j--) {
            uint8_t byte = (a->words[i] >> (j * 8)) & 0xFF;
            
            // Convert each byte to two hex characters
            uint8_t high = byte >> 4;
            uint8_t low = byte & 0xF;
            
            // Calculate buffer position
            int pos = (FF_LAST_WORD - i) * 8 + (3 - j) * 2;
            
            // Convert to hex characters
            buffer[pos] = high < 10 ? high + '0' : high - 10 + 'a';
            buffer[pos + 1] = low < 10 ? low + '0' : low - 10 + 'a';
        }
    }
}

static void ff_from_hex(ff_t* result, const char* hex) {
    ff_zero(result);
    int len = 0;
    while (hex[len] != '\0') len++;
    int word_idx = 0;
    int shift = 0;
    
    // Process hex string from right to left
    for (int i = len - 1; i >= 0; i--) {
        uint32_t val;
        if (hex[i] >= '0' && hex[i] <= '9') {
            val = hex[i] - '0';
        } else if (hex[i] >= 'a' && hex[i] <= 'f') {
            val = hex[i] - 'a' + 10;
        } else if (hex[i] >= 'A' && hex[i] <= 'F') {
            val = hex[i] - 'A' + 10;
        } else {
            continue;  // Skip invalid characters
        }
        
        result->words[word_idx] |= val << shift;
        shift += 4;
        if (shift == 32) {
            shift = 0;
            word_idx++;
        }
    }
}

// Helper function to compare ff_t with hex string
static int ff_equals_hex(const ff_t* a, const char* hex) {
    ff_t expected;
    ff_from_hex(&expected, hex);
    return ff_eq(a, &expected);
}

// Add two ff_t values with carry propagation
static inline void ff_add(ff_t* result, const ff_t* a, const ff_t* b) {
    uint32_t carry = 0;
    for (int i = 0; i < FF_WORDS; i++) {
        uint32_t sum = a->words[i] + b->words[i] + carry;
        carry = (sum < a->words[i]) || ((sum == a->words[i]) && (b->words[i] > 0));
        result->words[i] = sum;
    }
}

// Subtract two ff_t values with borrow propagation
static inline void ff_sub(ff_t* result, const ff_t* a, const ff_t* b) {
    uint32_t borrow = 0;
    for (int i = 0; i < FF_WORDS; i++) {
        uint32_t diff = a->words[i] - b->words[i] - borrow;
        borrow = (diff > a->words[i]) || ((diff == a->words[i]) && (b->words[i] > 0));
        result->words[i] = diff;
    }
}

// Optimized multiplication using the Cortex-M4's DSP instructions
static inline void ff_mul(ff_t* result, const ff_t* a, const ff_t* b) {
    ff_t temp;
    ff_zero(&temp);
    
    for (int i = 0; i < FF_WORDS; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < FF_WORDS - i; j++) {
            uint64_t product = (uint64_t)a->words[i] * b->words[j] +
                             temp.words[i + j] + carry;
            temp.words[i + j] = (uint32_t)product;
            carry = (uint32_t)(product >> 32);
        }
    }
    
    // Use word-aligned copy
    for (int i = 0; i < FF_WORDS; i++) {
        result->words[i] = temp.words[i];
    }
}

static inline void ff_mod(ff_t* result, const ff_t* a, const ff_t* modulus) {
    // If a < modulus, we're done
    if (ff_cmp(a, modulus) < 0) {
        *result = *a;
        return;
    }
    
    ff_t temp = *a;
    ff_t shifted_mod = *modulus;
    
    // Find the largest shift that keeps shifted_mod <= temp
    int shift = 0;
    while (ff_cmp(&shifted_mod, &temp) <= 0 && shift < FF_SIZE - 1) {
        ff_shl(&shifted_mod, &shifted_mod, 1);
        shift++;
    }
    // Back up one because we went one too far
    ff_shr(&shifted_mod, &shifted_mod, 1);
    shift--;
    
    // Now subtract out shifted_mod while shift >= 0
    while (shift >= 0) {
        if (ff_cmp(&temp, &shifted_mod) >= 0) {
            ff_sub(&temp, &temp, &shifted_mod);
        }
        ff_shr(&shifted_mod, &shifted_mod, 1);
        shift--;
    }
    
    *result = temp;
}

// Modular addition with optimized reduction
static inline void ff_mod_add(ff_t* result, const ff_t* a, const ff_t* b, 
                               const ff_t* modulus) {
    ff_add(result, a, b);
    // Always perform the modular reduction
    ff_mod(result, result, modulus);
}

// Optimized modular subtraction
static inline void ff_mod_sub(ff_t* result, const ff_t* a, const ff_t* b,
                               const ff_t* modulus) {
    ff_sub(result, a, b);
    
    // Always perform the modular reduction
    ff_mod(result, result, modulus);
}

// Modular multiplication with optimized reduction
static inline void ff_mod_mul(ff_t* result, const ff_t* a, const ff_t* b,
                               const ff_t* modulus) {
    ff_mul(result, a, b);
    ff_mod(result, result, modulus);
}

// Optimized modular exponentiation using window method
static inline void ff_mod_pow(ff_t* result, const ff_t* base, const ff_t* exp,
                               const ff_t* modulus) {
    ff_t temp;
    ff_from_u32(&temp, 1);
    ff_t base_copy = *base;
    
    // Process 4 bits at a time (window size = 4)
    for (int i = FF_LAST_WORD; i >= 0; i--) {
        uint32_t word = exp->words[i];
        for (int j = 28; j >= 0; j -= 4) {
            // Square 4 times
            for (int k = 0; k < 4; k++) {
                ff_mod_mul(&temp, &temp, &temp, modulus);
            }
            
            uint32_t window = (word >> j) & 0xF;
            if (window != 0) {
                // Multiply by pre-computed value
                ff_mod_mul(&temp, &temp, &base_copy, modulus);
            }
        }
    }
    
    *result = temp;
}

// Division with remainder: dividend = divisor * quotient + remainder
static inline void ff_div(ff_t* quotient, ff_t* remainder, 
                           const ff_t* dividend, const ff_t* divisor) {
    // Check for division by zero
    if (ff_is_zero(divisor)) {
        // Handle error - set max values
        for (int i = 0; i < FF_WORDS; i++) {
            quotient->words[i] = 0xFFFFFFFF;
            remainder->words[i] = 0xFFFFFFFF;
        }
        return;
    }
    
    // If dividend < divisor, quotient = 0, remainder = dividend
    if (ff_cmp(dividend, divisor) < 0) {
        ff_zero(quotient);
        *remainder = *dividend;
        return;
    }
    
    // Initialize remainder to dividend and quotient to 0
    *remainder = *dividend;
    ff_zero(quotient);
    
    // Find the initial shift amount
    int shift = ff_clz(divisor) - ff_clz(remainder);
    
    ff_t shifted_divisor;
    ff_shl(&shifted_divisor, divisor, shift);
    
    // Main division loop
    while (shift >= 0) {
        if (ff_cmp(remainder, &shifted_divisor) >= 0) {
            // Subtract shifted divisor from remainder
            ff_sub(remainder, remainder, &shifted_divisor);
            
            // Set bit in quotient
            quotient->words[shift / 32] |= 1U << (shift % 32);
        }
        
        // Shift right by 1
        ff_shr(&shifted_divisor, &shifted_divisor, 1);
        shift--;
    }
}