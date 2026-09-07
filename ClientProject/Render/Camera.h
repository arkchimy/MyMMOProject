#pragma once

namespace actors { class Actor; }

namespace render
{
    class Camera final
    {
    public:
        Camera();

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;
        Camera(Camera&&) = delete;
        Camera& operator=(Camera&&) = delete;

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
