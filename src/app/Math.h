// Peekstep - minimal vector and matrix math (column-major, like GLSL).
#pragma once

#include <cmath>

namespace peekstep {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v)
{
    const float l = length(v);
    return l > 0 ? v * (1.0f / l) : v;
}

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // m[col * 4 + row]
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a.at(row, k) * b.at(k, col);
            r.at(row, col) = s;
        }
    return r;
}

// Right-handed view matrix.
inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z; r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z; r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
}

// Perspective projection for Vulkan: depth 0..1, Y pointing down in clip space.
inline Mat4 perspective(float fovY, float aspect, float zNear, float zFar)
{
    const float t = 1.0f / std::tan(fovY * 0.5f);
    Mat4 r;
    r.at(0, 0) = t / aspect;
    r.at(1, 1) = -t;
    r.at(2, 2) = zFar / (zNear - zFar);
    r.at(2, 3) = zNear * zFar / (zNear - zFar);
    r.at(3, 2) = -1;
    r.at(3, 3) = 0;
    return r;
}

} // namespace peekstep
