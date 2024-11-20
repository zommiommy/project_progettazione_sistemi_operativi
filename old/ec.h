#pragma once

#include <stdint.h>
#include "ff.h"
#include "prng.h"

// Structure to represent a point on the curve
typedef struct {
    ff_t x;
    ff_t y;
    int is_infinity;  // 1 if point is O (point at infinity), 0 otherwise
} ECPoint;


// the curve is y^2 = x^3 + ax + b mod p

// raw: 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
const ff_t p = { .words = { 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xffffffff } };
// raw: 0xffffffff00000001000000000000000000000000fffffffffffffffffffffffc
const ff_t a = { .words = { 0xfffffffc, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xffffffff } };
// raw: 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
const ff_t b = { .words = { 0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0, 0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8 } };

// generator point coordinates
// raw: 0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
const ff_t gx = { .words = { 0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81, 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2 } };
// raw: 0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
const ff_t gy = { .words = { 0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357, 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2 } };
// the generator point
const ECPoint g = { .x = gx, .y = gy, .is_infinity = 0 };

// generator order
// raw: 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
const ff_t n = { .words = { 0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, 0xffffffff, 0xffffffff, 0x00000000, 0xffffffff } };

// Extended Euclidean Algorithm for modular inverse
static inline void ff_mod_inv(ff_t* result, const ff_t* a) {
    // Handle zero input
    if (ff_is_zero(a)) {
        ff_zero(result);
        return;
    }

    ff_t reduced_a;
    ff_mod(&reduced_a, a, &p);  // Reduce input modulo p first
    
    ff_t r, old_r;
    ff_t s, old_s;
    ff_t t, old_t;
    ff_t quotient, remainder, temp;
    
    // Initialize values
    r = p;
    old_r = reduced_a;  // Use reduced input
    ff_from_u32(&s, 0);
    ff_from_u32(&old_s, 1);
    ff_from_u32(&t, 1);
    ff_from_u32(&old_t, 0);
    
    while (!ff_is_zero(&r)) {
        ff_t temp_r = r;
        ff_t temp_s = s;
        ff_t temp_t = t;
        
        ff_div(&quotient, &remainder, &old_r, &r);
        
        r = remainder;
        old_r = temp_r;
        
        ff_mul(&temp, &quotient, &s);
        ff_sub(&s, &old_s, &temp);
        old_s = temp_s;
        
        ff_mul(&temp, &quotient, &t);
        ff_sub(&t, &old_t, &temp);
        old_t = temp_t;
    }
    
    while (ff_is_negative(&old_s)) {
        ff_add(&old_s, &old_s, &p);
    }
    
    while (ff_cmp(&old_s, &p) >= 0) {
        ff_sub(&old_s, &old_s, &p);
    }
    
    ff_mul(&temp, &reduced_a, &old_s);
    ff_mod(&temp, &temp, &p);
    ff_t one;
    ff_from_u32(&one, 1);
    
    if (!ff_eq(&temp, &one)) {
        ff_zero(result);
        return;
    }
    
    *result = old_s;
}

// Initialize a point
static inline void ec_init_point(ECPoint* P, const ff_t* x, const ff_t* y) {
    P->x = *x;
    P->y = *y;
    P->is_infinity = 0;
}

// Set point to infinity
static inline void ec_set_infinity(ECPoint* P) {
    ff_zero(&P->x);
    ff_zero(&P->y);
    P->is_infinity = 1;
}
// Add two points on the curve
static inline void ec_add(ECPoint* result, const ECPoint* P1, const ECPoint* P2) {
    if (P1->is_infinity) {
        *result = *P2;
        return;
    }
    if (P2->is_infinity) {
        *result = *P1;
        return;
    }
    
    ff_t neg_y;
    ff_sub(&neg_y, &p, &P2->y);
    if (ff_eq(&P1->x, &P2->x) && ff_eq(&P1->y, &neg_y)) {
        ec_set_infinity(result);
        return;
    }
    
    ff_t m;
    
    if (ff_eq(&P1->x, &P2->x) && ff_eq(&P1->y, &P2->y)) {
        // Point doubling: m = (3x^2 + a)/(2y)
        ff_t num, denom, temp;
        
        // Calculate numerator: 3x^2 + a
        ff_mul(&temp, &P1->x, &P1->x);  // x^2
        ff_mod(&temp, &temp, &p);       // Reduce mod p
        ff_from_u32(&num, 3);
        ff_mod_mul(&num, &num, &temp, &p);  // 3x^2
        ff_mod_add(&num, &num, &a, &p);     // 3x^2 + a

        // Calculate denominator: 2y
        ff_from_u32(&denom, 2);
        ff_mod_mul(&denom, &denom, &P1->y, &p);  // 2y
        
        // Calculate slope m = (3x^2 + a)/(2y)
        ff_mod_inv(&temp, &denom);         // Modular inverse of 2y
        ff_mod_mul(&m, &num, &temp, &p);   // Multiply by numerator 
    } else {
        // Point addition: m = (y2 - y1)/(x2 - x1)
        ff_t num, denom, temp;
        
        ff_sub(&num, &P2->y, &P1->y);      // y2 - y1
        ff_sub(&denom, &P2->x, &P1->x);    // x2 - x1
        
        ff_mod_inv(&temp, &denom);         // Modular inverse of denominator
        ff_mod_mul(&m, &num, &temp, &p);   // Multiply by numerator mod p
    }
    
    // Calculate x3 = m^2 - x1 - x2
    ff_t x3;
    ff_mod_mul(&x3, &m, &m, &p);          // m^2
    ff_mod_sub(&x3, &x3, &P1->x, &p);     // m^2 - x1
    ff_mod_sub(&x3, &x3, &P2->x, &p);     // m^2 - x1 - x2
    
    // Calculate y3 = m(x1 - x3) - y1
    ff_t y3, temp;
    ff_sub(&temp, &P1->x, &x3);          // x1 - x3
    ff_mod_mul(&y3, &m, &temp, &p);      // m(x1 - x3)
    ff_mod_sub(&y3, &y3, &P1->y, &p);    // m(x1 - x3) - y1
    
    result->x = x3;
    result->y = y3;
    result->is_infinity = 0;
}

// Scalar multiplication using double-and-add algorithm
static inline void ec_scalar_mul(ECPoint* result, const ECPoint* P, const ff_t* k) {
    ECPoint R;
    ec_set_infinity(&R);
    ECPoint temp = *P;
    
    // Process from LSB to MSB
    for (int i = 0; i < FF_SIZE; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;
        
        ec_add(&R, &R, &R);
        
        if ((k->words[word_idx] >> bit_idx) & 1) {
            ec_add(&R, &R, &temp);
        }
    }
    
    *result = R;
}

static inline void ec_init_random_k(ff_t *result) {
    ff_t temp;
    for (int i = 0; i < FF_WORDS; i++) {
        temp.words[i] = nextRand();
    }
    ff_mod(result, &temp, &n);
}
