#pragma once

namespace actors { class Actor; }

namespace render
{
    class Camera
    {
    public:
        Camera();
        void SetTarget(actors::Actor* target);
        void Update();
        float GetX() const { return mX; }
        float GetY() const { return mY; }

    private:
        float mX;
        float mY;
        actors::Actor* mTarget;
    };
}
