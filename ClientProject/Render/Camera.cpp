#include "Camera.h"
#include "../Actors/Actor.h"

namespace render
{
    Camera::Camera()
        : mX(640.f)
        , mY(360.f)
        , mTarget(nullptr)
    {
    }

    void Camera::SetTarget(actors::Actor* target)
    {
        mTarget = target;
    }

    void Camera::Update()
    {
        if (mTarget != nullptr)
        {
            mX = mTarget->GetX();
            mY = mTarget->GetY();
        }
    }
}
