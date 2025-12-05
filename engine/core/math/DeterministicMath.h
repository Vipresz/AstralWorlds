#pragma once

#include <cstdint>
#include <cmath>
#include <limits>

namespace Engine {
namespace Math {

// Deterministic floating-point operations
// Key principle: Always perform operations in the same order

// Fixed-point math for absolute determinism (optional, more complex)
// For now, we'll use deterministic floating-point with strict ordering

// Deterministic random number generator (seeded per tick)
class DeterministicRNG {
public:
    DeterministicRNG(uint64_t seed = 0) : m_seed(seed) {}
    
    void SetSeed(uint64_t seed) {
        m_seed = seed;
    }
    
    uint32_t Next() {
        // Linear congruential generator (deterministic)
        m_seed = (m_seed * 1103515245 + 12345) & 0x7fffffff;
        return static_cast<uint32_t>(m_seed);
    }
    
    float NextFloat() {
        return Next() / static_cast<float>(0x7fffffff);
    }
    
    uint64_t GetSeed() const { return m_seed; }
    
private:
    uint64_t m_seed;
};

// Vector3 - Deterministic 3D vector
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    float Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    float LengthSquared() const {
        return x * x + y * y + z * z;
    }
    
    Vec3 Normalized() const {
        float len = Length();
        if (len > std::numeric_limits<float>::epsilon()) {
            return *this * (1.0f / len);
        }
        return Vec3();
    }
    
    float Dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    
    Vec3 Cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};

// AABB - Axis-Aligned Bounding Box
struct AABB {
    Vec3 min;
    Vec3 max;
    
    AABB() {}
    AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}
    
    bool Intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
    
    Vec3 GetCenter() const {
        return Vec3(
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f
        );
    }
    
    Vec3 GetSize() const {
        return Vec3(
            max.x - min.x,
            max.y - min.y,
            max.z - min.z
        );
    }
};

} // namespace Math
} // namespace Engine

