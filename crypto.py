from dataclasses import dataclass
from typing import Optional, Tuple


class PRNG:
    
    MASK64 = (1 << 64) - 1
    MASK32 = (1 << 32) - 1
    
    def __init__(self):
        self.splitmix64_seed = 0xbad5eed
        self.s = [0] * 4  # xoshiro128+ state
        
        # Initialize the xoshiro128+ state using splitmix64
        for i in range(4):
            # Take the lower 32 bits of splitmix64 output
            self.s[i] = self.next_splitmix64() & self.MASK32

    def next_splitmix64(self):
        # Python doesn't have unsigned 64-bit integers, so we use masking
        
        self.splitmix64_seed = (self.splitmix64_seed + 0x9E3779B97F4A7C15) & self.MASK64
        z = self.splitmix64_seed
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & self.MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & self.MASK64
        return (z ^ (z >> 31)) & self.MASK64

    def rotl(self, x, k):
        # 32-bit rotation
        return ((x << k) | (x >> (32 - k))) & self.MASK32

    def next_rand(self):
        # xoshiro128+ algorithm
        result = (self.s[0] + self.s[3]) & self.MASK32
        t = (self.s[1] << 9) & self.MASK32

        self.s[2] ^= self.s[0]
        self.s[3] ^= self.s[1]
        self.s[1] ^= self.s[2]
        self.s[0] ^= self.s[3]

        self.s[2] ^= t
        self.s[3] = self.rotl(self.s[3], 11)

        return result
    
    def next_ff(self):
        result = 0
        for i in range(8):
            x = self.next_rand()
            result |= x << (32 * i)
        return result

def mod_inverse(n: int, p: int) -> int:
    """
    Calculate modular multiplicative inverse using extended Euclidean algorithm
    Returns x such that (n * x) % p == 1
    """
    def extended_gcd(a: int, b: int) -> Tuple[int, int, int]:
        if a == 0:
            return b, 0, 1
        gcd, x1, y1 = extended_gcd(b % a, a)
        x = y1 - (b // a) * x1
        y = x1
        return gcd, x, y

    gcd, x, _ = extended_gcd(n, p)
    if gcd != 1:
        raise ValueError(f"Modular inverse does not exist for {n} mod {p}")
    return x % p

@dataclass
class Point:
    x: Optional[int]
    y: Optional[int]
    
    def __str__(self):
        if self.is_infinity():
            return "Point(∞)"
        return f"Point({self.x:x}, {self.y:x})"
    
    def is_infinity(self) -> bool:
        return self.x is None and self.y is None

class EllipticCurve:
    """
    Elliptic curve in Weierstrass form: y² = x³ + ax + b (mod p)
    All operations are performed in the finite field F_p
    """
    def __init__(self, a: int, b: int, p: int):
        """
        Initialize curve with coefficients a and b and prime modulus p
        y² = x³ + ax + b (mod p)
        """
        
        self.a = a % p
        self.b = b % p
        self.p = p
        
        # Check that 4a³ + 27b² ≠ 0 (mod p) (curve is non-singular)
        disc = (-16 * (4 * pow(a, 3, p) + 27 * pow(b, 2, p))) % p
        if disc == 0:
            raise ValueError("Invalid curve parameters: curve is singular")

    
    def is_on_curve(self, point: Point) -> bool:
        """Check if point lies on the curve"""
        if point.is_infinity():
            return True
            
        x, y = point.x % self.p, point.y % self.p
        return (pow(y, 2, self.p) - (pow(x, 3, self.p) + self.a * x + self.b)) % self.p == 0
    
    def add(self, P1: Point, P2: Point) -> Point:
        """Add two points on the curve"""
        # Handle point at infinity cases
        if P1.is_infinity():
            return P2
        if P2.is_infinity():
            return P1
            
        x1, y1 = P1.x % self.p, P1.y % self.p
        x2, y2 = P2.x % self.p, P2.y % self.p
        
        # P1 = -P2
        if x1 == x2 and (y1 + y2) % self.p == 0:
            return Point(None, None)  # Return point at infinity
            
        # Calculate slope
        if x1 == x2:  # P1 = P2
            if y1 == 0:
                return Point(None, None)  # Point at infinity
            # Tangent line
            num = (3 * pow(x1, 2, self.p) + self.a) % self.p
            den = mod_inverse(2 * y1, self.p)
            m = (num * den) % self.p
        else:
            # Line through P1 and P2
            num = (y2 - y1) % self.p
            den = mod_inverse((x2 - x1) % self.p, self.p)
            m = (num * den) % self.p
            
        # Calculate new point
        x3 = (pow(m, 2, self.p) - x1 - x2) % self.p
        y3 = (m * (x1 - x3) - y1) % self.p
        
        return Point(x3, y3)
    
    def multiply(self, k: int, P: Point) -> Point:
        """Multiply point P by scalar k using double-and-add algorithm"""
        if k < 0:
            k = -k
            P = Point(P.x, (-P.y) % self.p)
            
        result = Point(None, None)  # Point at infinity
        addend = P
        
        while k:
            if k & 1:  # If last bit is 1
                result = self.add(result, addend)
            addend = self.add(addend, addend)  # Double
            k >>= 1  # Shift right
            
        return result
    
    def get_y(self, x: int) -> Tuple[int, int]:
        """Calculate y coordinates for given x coordinate"""
        x = x % self.p
        y_squared = (pow(x, 3, self.p) + self.a * x + self.b) % self.p
        
        # Find square root modulo p using Tonelli-Shanks algorithm
        if pow(y_squared, (self.p - 1) // 2, self.p) != 1:
            raise ValueError(f"No y coordinate exists for x = {x}")
            
        # For prime p ≡ 3 (mod 4), we can use a simpler formula
        if self.p % 4 == 3:
            y = pow(y_squared, (self.p + 1) // 4, self.p)
            return (y, (-y) % self.p)
        
        raise NotImplementedError("General case Tonelli-Shanks algorithm not implemented")

    def __str__(self):
        return f"y² = x³ + {self.a}x + {self.b} (mod {self.p})"
    

p =  0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
a =  0xffffffff00000001000000000000000000000000fffffffffffffffffffffffc
b =  0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
gx = 0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
gy = 0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
n =  0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551

curve = EllipticCurve(a, b, p)
G = Point(gx, gy)

if __name__ == "__main__":
    print(hex((a + b) % p))
    
    prng = PRNG()
    
    # Generate some random numbers
    for _ in range(3):
        x = prng.next_ff() % n
        res = curve.multiply(x, G)
        print(res)