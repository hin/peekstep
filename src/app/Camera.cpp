// Peekstep - orbit camera.
#include "app/Camera.h"

#include <algorithm>

namespace peekstep {

namespace {
constexpr float kHalfPi = 1.5707963f;
}

void Camera::fit(const Vec3& bboxMin, const Vec3& bboxMax)
{
    target_ = (bboxMin + bboxMax) * 0.5f;
    radius_ = std::max(length(bboxMax - bboxMin) * 0.5f, 1e-3f);
    distance_ = radius_ / std::sin(kFovY * 0.5f) * 1.05f;
}

void Camera::setView(View view)
{
    switch (view) {
    case View::Iso:    yaw_ = -0.785398f; pitch_ = 0.615480f; break;
    case View::Front:  yaw_ = -kHalfPi;   pitch_ = 0; break;
    case View::Back:   yaw_ = kHalfPi;    pitch_ = 0; break;
    case View::Right:  yaw_ = 0;          pitch_ = 0; break;
    case View::Left:   yaw_ = 2 * kHalfPi; pitch_ = 0; break;
    case View::Top:    yaw_ = -kHalfPi;   pitch_ = kHalfPi; break;
    case View::Bottom: yaw_ = -kHalfPi;   pitch_ = -kHalfPi; break;
    }
}

void Camera::orbit(float dx, float dy)
{
    yaw_ -= dx * 0.008f;
    pitch_ = std::clamp(pitch_ + dy * 0.008f, -kHalfPi, kHalfPi);
}

void Camera::pan(float dx, float dy, float viewportHeight)
{
    // Move so that the point under the cursor follows the mouse at target depth.
    const float worldPerPixel = 2.0f * distance_ * std::tan(kFovY * 0.5f) / std::max(viewportHeight, 1.0f);
    const float sp = std::sin(pitch_);
    const Vec3 up(-sp * std::cos(yaw_), -sp * std::sin(yaw_), std::cos(pitch_));
    const Vec3 right = normalize(cross(forward(), up));
    target_ = target_ - right * (dx * worldPerPixel) + up * (dy * worldPerPixel);
}

void Camera::zoom(float steps)
{
    distance_ = std::clamp(distance_ * std::pow(0.85f, steps), radius_ * 0.01f, radius_ * 100.0f);
}

Vec3 Camera::forward() const
{
    // Direction from the eye to the target.
    const float cp = std::cos(pitch_);
    return -Vec3(cp * std::cos(yaw_), cp * std::sin(yaw_), std::sin(pitch_));
}

Vec3 Camera::eye() const
{
    return target_ - forward() * distance_;
}

Mat4 Camera::viewProj(float aspect) const
{
    // Camera "up" follows the orbit, so straight top and bottom views stay defined.
    const float sp = std::sin(pitch_);
    const Vec3 up(-sp * std::cos(yaw_), -sp * std::sin(yaw_), std::cos(pitch_));
    const float zNear = std::max(distance_ - radius_ * 1.5f, distance_ * 0.01f);
    const float zFar = distance_ + radius_ * 1.5f;
    return perspective(kFovY, aspect, zNear, zFar) * lookAt(eye(), target_, up);
}

} // namespace peekstep
