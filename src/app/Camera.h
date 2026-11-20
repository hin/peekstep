// Peekstep - orbit camera (Z up, like most CAD tools).
#pragma once

#include "app/Math.h"

namespace peekstep {

class Camera {
public:
    enum class View { Iso, Front, Back, Left, Right, Top, Bottom };

    void fit(const Vec3& bboxMin, const Vec3& bboxMax);
    void setView(View view);

    void orbit(float dxPixels, float dyPixels);
    void pan(float dxPixels, float dyPixels, float viewportHeight);
    void zoom(float steps);

    Vec3 eye() const;
    Vec3 forward() const;
    Mat4 viewProj(float aspect) const;

private:
    Vec3 target_{0, 0, 0};
    float radius_ = 1.0f;   // bounding sphere radius of the model
    float distance_ = 3.0f; // eye to target
    float yaw_ = -0.785398f;  // around Z
    float pitch_ = 0.615480f; // above the XY plane
    static constexpr float kFovY = 0.6f;
};

} // namespace peekstep
