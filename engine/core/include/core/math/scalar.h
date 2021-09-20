#pragma once

#include <cmath>
#include <cstdint>

namespace kw {

constexpr float PI      = 3.14159265359f;
constexpr float EPSILON = 1e-6f;
constexpr float SQRT_2  = 1.41421356237f;

constexpr float sqr(float value) {
    return value * value;
}

constexpr float lerp(float from, float to, float factor) {
    return from + (to - from) * factor;
}

constexpr float clamp(float value, float min, float max) {
    return value < min ? min : (max < value ? max : value);
}

constexpr float equal(float a, float b, float epsilon = EPSILON) {
    return a - b > -epsilon && a - b < epsilon;
}

constexpr float degrees(float radians) {
    return radians / PI * 180.f;
}

constexpr float radians(float degrees) {
    return degrees / 180.f * PI;
}

template <typename T>
constexpr T align_up(T value, T alignment) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

template <typename T>
constexpr T align_down(T value, T alignment) {
    return value & ~(alignment - 1);
}

// False positive for 0.
template <typename T>
constexpr bool is_pow2(T value) {
    return (value & (value - 1)) == 0;
}

// Doesn't work for 0.
constexpr uint32_t next_pow2(uint32_t value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

// Doesn't work for 0.
constexpr uint64_t next_pow2(uint64_t value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    value++;
    return value;
}

// Doesn't work for 0.
constexpr uint32_t previous_pow2(uint32_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value = value ^ (value >> 1);
    return value;
}

// Doesn't work for 0.
constexpr uint64_t previous_pow2(uint64_t value) {
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    value = value ^ (value >> 1);
    return value;
}

constexpr uint32_t count_bits_set(uint32_t value) {
    uint32_t result = value - ((value >> 1) & 0x55555555);
    result = ((result >> 2) & 0x33333333) + (result & 0x33333333);
    result = ((result >> 4) + result) & 0x0F0F0F0F;
    result = ((result >> 8) + result) & 0x00FF00FF;
    result = ((result >> 16) + result) & 0x0000FFFF;
    return result;
}

uint32_t log2(uint32_t value);
uint64_t log2(uint64_t value);

inline float normalize_angle(float angle) {
    angle = std::fmodf(angle, 2 * PI);
    if (angle < 0) {
        angle += 2 * PI;
    }
    return angle;
}

// Both `from` and `to` are expected to be normalized.
inline float shortest_angle(float from, float to) {
    float result = std::fmodf(to - from + 2 * PI, 2 * PI);
    if (result > PI) {
        result = result - 2 * PI;
    }
    return result;
}

} // namespace kw
