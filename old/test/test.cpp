#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ff.h"
#include "ec.h"

// Helper function to initialize ff_t from hex string
// Test basic initialization and comparison
static void test_init_and_compare(void) {
    printf("Testing initialization and comparison...\n");
    
    ff_t a, b;
    ff_from_u32(&a, 42);
    ff_from_u32(&b, 42);
    assert(ff_eq(&a, &b));
    
    ff_from_u32(&b, 43);
    assert(!ff_eq(&a, &b));
    
    ff_zero(&a);
    assert(ff_is_zero(&a));
    
    ff_from_hex(&a, "deadbeef");
    assert(ff_equals_hex(&a, "deadbeef"));
    
    printf("Initialization and comparison tests passed!\n");
}

// Test addition and subtraction
static void test_add_sub(void) {
    printf("Testing addition and subtraction...\n");
    
    ff_t a, b, result;
    
    // Test basic addition
    ff_from_hex(&a, "ffffffff");
    ff_from_hex(&b, "1");
    ff_add(&result, &a, &b);
    assert(ff_equals_hex(&result, "100000000"));
    
    // Test addition with carry propagation
    ff_from_hex(&a, "ffffffffffffffff");
    ff_from_hex(&b, "1");
    ff_add(&result, &a, &b);
    assert(ff_equals_hex(&result, "10000000000000000"));
    
    // Test subtraction
    ff_from_hex(&a, "100000000");
    ff_from_hex(&b, "1");
    ff_sub(&result, &a, &b);
    assert(ff_equals_hex(&result, "ffffffff"));
    
    // Test subtraction with borrow
    ff_from_hex(&a, "10000000000000000");
    ff_from_hex(&b, "1");
    ff_sub(&result, &a, &b);
    assert(ff_equals_hex(&result, "ffffffffffffffff"));
    
    printf("Addition and subtraction tests passed!\n");
}

// Test multiplication
static void test_multiplication(void) {
    printf("Testing multiplication...\n");
    
    ff_t a, b, result;
    
    // Test basic multiplication
    ff_from_hex(&a, "2");
    ff_from_hex(&b, "3");
    ff_mul(&result, &a, &b);
    assert(ff_equals_hex(&result, "6"));
    
    // Test larger multiplication
    ff_from_hex(&a, "deadbeef");
    ff_from_hex(&b, "cafebabe");
    ff_mul(&result, &a, &b);
    assert(ff_equals_hex(&result, "b092ab7b88cf5b62"));
    
    printf("Multiplication tests passed!\n");
}

// Test modular operations
static void test_modular_ops(void) {
    printf("Testing modular operations...\n");
    
    ff_t a, b, modulus, result;

    // Set modulus to a prime number
    ff_from_hex(&modulus, "17");  // 23 in decimal
    
    // Additional test cases for modular addition
    ff_from_hex(&a, "5");
    ff_from_hex(&b, "3");
    ff_mod_add(&result, &a, &b, &modulus);
    assert(ff_equals_hex(&result, "8")); // (5 + 3) mod 23 = 8
    
    // Test modular addition
    ff_from_hex(&a, "15");  // 21 in decimal
    ff_from_hex(&b, "14");  // 20 in decimal
    ff_mod_add(&result, &a, &b, &modulus);
    assert(ff_equals_hex(&result, "12")); // (21 + 20) = 41 mod 23 = 18 = 0x12
    
    // Test modular subtraction
    ff_from_hex(&a, "5");
    ff_from_hex(&b, "8");
    ff_mod_sub(&result, &a, &b, &modulus);

    assert(ff_equals_hex(&result, "14")); // (5 - 8) mod 23 = 20 = 0x14
    
    // Test modular multiplication
    ff_from_hex(&a, "4");
    ff_from_hex(&b, "5");
    ff_mod_mul(&result, &a, &b, &modulus);
    assert(ff_equals_hex(&result, "14")); // (4 * 5) mod 23 = 20 = 0x14
    
    printf("Modular operations tests passed!\n");
}

// Test bit operations
static void test_bit_ops(void) {
    printf("Testing bit operations...\n");
    
    ff_t a, result;
    
    // Test left shift
    ff_from_hex(&a, "1");
    ff_shl(&result, &a, 4);
    assert(ff_equals_hex(&result, "10"));
    
    // Test right shift
    ff_from_hex(&a, "10");
    ff_shr(&result, &a, 4);
    assert(ff_equals_hex(&result, "1"));
    
    // Test leading zeros count
    ff_from_hex(&a, "10");
    assert(ff_clz(&a) == FF_SIZE - 5); // "10" hex has 5 bits
    
    printf("Bit operations tests passed!\n");
}

// Test division
static void test_division(void) {
    printf("Testing division...\n");
    
    ff_t dividend, divisor, quotient, remainder;
    
    // Test basic division
    ff_from_hex(&dividend, "64"); // 100 in decimal
    ff_from_hex(&divisor, "a");   // 10 in decimal
    ff_div(&quotient, &remainder, &dividend, &divisor);
    assert(ff_equals_hex(&quotient, "a"));     // 10
    assert(ff_equals_hex(&remainder, "0"));    // 0
    
    // Test division with remainder
    ff_from_hex(&dividend, "65"); // 101 in decimal
    ff_from_hex(&divisor, "a");   // 10 in decimal
    ff_div(&quotient, &remainder, &dividend, &divisor);
    assert(ff_equals_hex(&quotient, "a"));     // 10
    assert(ff_equals_hex(&remainder, "1"));    // 1
    
    // Test division by zero
    ff_from_hex(&dividend, "64");
    ff_zero(&divisor);
    ff_div(&quotient, &remainder, &dividend, &divisor);
    // Should set both quotient and remainder to max value
    for (int i = 0; i < FF_WORDS; i++) {
        assert(quotient.words[i] == 0xFFFFFFFF);
        assert(remainder.words[i] == 0xFFFFFFFF);
    }
    
    printf("Division tests passed!\n");
}

// Test hex conversion
static void test_hex_conversion(void) {
    printf("Testing hex conversion...\n");
    
    ff_t value;
    uint8_t buffer[65] = {0};  // 64 chars + null terminator
    
    // Test conversion of simple value
    ff_from_hex(&value, "deadbeef");
    ff_to_hex(buffer, &value);
    assert(strcmp((char*)buffer, "00000000000000000000000000000000000000000000000000000000deadbeef") == 0);
    
    // Test conversion of zero
    ff_zero(&value);
    ff_to_hex(buffer, &value);
    assert(strcmp((char*)buffer, "0000000000000000000000000000000000000000000000000000000000000000") == 0);
    
    printf("Hex conversion tests passed!\n");
}

// Helper function to print points for debugging
static void print_point(const char* prefix, const ECPoint* P) {
    uint8_t buffer[65] = {0};
    printf("%s: ", prefix);
    if (P->is_infinity) {
        printf("Point at infinity\n");
        return;
    }
    ff_to_hex(buffer, &P->x);
    printf("x = %s, ", buffer);
    ff_to_hex(buffer, &P->y);
    printf("y = %s\n", buffer);
}

// Test point initialization and comparison
static void test_point_init(void) {
    printf("Testing point initialization...\n");
    
    ECPoint P;
    
    // Test generator point initialization
    assert(!g.is_infinity);
    assert(ff_eq(&g.x, &gx));
    assert(ff_eq(&g.y, &gy));
    
    // Test point at infinity
    ec_set_infinity(&P);
    assert(P.is_infinity);
    assert(ff_is_zero(&P.x));
    assert(ff_is_zero(&P.y));
    
    // Test regular point initialization
    ff_t x, y;
    ff_from_hex(&x, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296");
    ff_from_hex(&y, "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");
    ec_init_point(&P, &x, &y);
    assert(!P.is_infinity);
    assert(ff_eq(&P.x, &x));
    assert(ff_eq(&P.y, &y));
    
    printf("Point initialization tests passed!\n");
}

// Test point addition
static void test_point_addition(void) {
    printf("Testing point addition...\n");
    
    ECPoint P1, P2, result;
    
    // Test P + O = P
    ec_init_point(&P1, &gx, &gy);
    ec_set_infinity(&P2);
    ec_add(&result, &P1, &P2);
    assert(!result.is_infinity);
    assert(ff_eq(&result.x, &P1.x));
    assert(ff_eq(&result.y, &P1.y));
    
    // Test O + P = P
    ec_set_infinity(&P1);
    ec_init_point(&P2, &gx, &gy);
    ec_add(&result, &P1, &P2);
    assert(!result.is_infinity);
    assert(ff_eq(&result.x, &P2.x));
    assert(ff_eq(&result.y, &P2.y));
    
    // Test P + (-P) = O
    ec_init_point(&P1, &gx, &gy);
    ec_init_point(&P2, &gx, &gy);
    ff_sub(&P2.y, &p, &P2.y);  // Negate y-coordinate
    ec_add(&result, &P1, &P2);
    assert(result.is_infinity);
    
    // Test point doubling (P + P)
    ec_init_point(&P1, &gx, &gy);
    ec_add(&result, &P1, &P1);
    
    // Known values for 2G (can be computed with external tools)
    ff_t expected_x, expected_y;
    ff_from_hex(&expected_x, "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978");
    ff_from_hex(&expected_y, "07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1");
    assert(ff_eq(&result.x, &expected_x));
    assert(ff_eq(&result.y, &expected_y));
    
    printf("Point addition tests passed!\n");
}

// Test scalar multiplication
static void test_scalar_multiplication(void) {
    printf("Testing scalar multiplication...\n");
    
    ECPoint result;
    ff_t k;
    
    // Test 0 * P = O
    ff_zero(&k);
    ec_scalar_mul(&result, &g, &k);
    assert(result.is_infinity);
    
    // Test 1 * P = P
    ff_from_u32(&k, 1);
    ec_scalar_mul(&result, &g, &k);
    assert(ff_eq(&result.x, &gx));
    assert(ff_eq(&result.y, &gy));
    
    // Test 2 * P
    ff_from_u32(&k, 2);
    ec_scalar_mul(&result, &g, &k);
    
    // Known values for 2G
    ff_t expected_x, expected_y;
    ff_from_hex(&expected_x, "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978");
    ff_from_hex(&expected_y, "07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1");
    assert(ff_eq(&result.x, &expected_x));
    assert(ff_eq(&result.y, &expected_y));
    
    // Test that n * G = O (where n is the group order)
    ec_scalar_mul(&result, &g, &n);
    assert(result.is_infinity);
    
    printf("Scalar multiplication tests passed!\n");
}

// Test random k generation
static void test_random_k(void) {
    printf("Testing random k generation...\n");
    
    ff_t k;
    
    // Test multiple random values
    for (int i = 0; i < 10; i++) {
        ec_init_random_k(&k);
        
        // Verify k is in range [1, n-1]
        assert(!ff_is_zero(&k));
        assert(ff_cmp(&k, &n) < 0);
    }
    
    printf("Random k generation tests passed!\n");
}

// Test point validation
static void test_point_validation(void) {
    printf("Testing point validation...\n");
    
    ECPoint P;
    ff_t temp1, temp2, temp3;
    
    // Test generator point satisfies curve equation y^2 = x^3 + ax + b
    
    // Calculate right side: x^3 + ax + b
    ff_mul(&temp1, &gx, &gx);       // x^2
    ff_mod_mul(&temp1, &temp1, &gx, &p);  // x^3
    ff_mod_mul(&temp2, &a, &gx, &p);      // ax
    ff_mod_add(&temp1, &temp1, &temp2, &p); // x^3 + ax
    ff_mod_add(&temp1, &temp1, &b, &p);     // x^3 + ax + b
    
    // Calculate left side: y^2
    ff_mul(&temp2, &gy, &gy);
    ff_mod(&temp2, &temp2, &p);
    
    // Verify equation
    assert(ff_eq(&temp1, &temp2));
    
    printf("Point validation tests passed!\n");
}

int main(void) {
    printf("Starting FF library tests...\n");
    
    test_init_and_compare();
    test_add_sub();
    test_multiplication();
    test_modular_ops();
    test_bit_ops();
    test_division();
    test_hex_conversion();
    
    printf("\nAll tests passed successfully!\n");
    printf("Starting elliptic curve tests...\n");

    initRand();
    test_point_init();
    test_point_addition();
    test_scalar_multiplication();
    test_random_k();
    test_point_validation();
    
    printf("\nAll elliptic curve tests passed successfully!\n");
    return 0;
}