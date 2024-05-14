#ifndef VIEW_H
#define VIEW_H

#include "random.h"
#include "vec3.h"

#include <cmath>

struct View
{
    static constexpr auto lookat = point3(0, 0, -1); // Point camera is looking at
    static constexpr auto vup    = vec3(0, 1, 0);    // Camera-relative "up" direction

    float fieldOfView = 90.f;
    float focalLength;
    float viewportHeight;
    float viewportWidth;

    point3 camera;
    vec3 viewportX;
    vec3 viewportY;
    vec3 pixelDX;
    vec3 pixelDY;
    vec3 viewportUL;
    vec3 pixelUL;

    View() {
        recalculate();
    }

    void recalculate() {
        focalLength = (camera - lookat).length();
        viewportHeight = 2 * std::tan(fieldOfView * 3.14159265 / 180.0 / 2.0) * focalLength;
        viewportWidth  = viewportHeight * Aspect;

        const auto w = (camera - lookat).normalize();
        const auto u = cross(vup, w).normalize();
        const auto v = cross(w, u);

        viewportX = viewportWidth * u;
        viewportY = -viewportHeight * v;

        pixelDX = viewportX / Width;
        pixelDY = viewportY / Height;
        viewportUL = camera - focalLength * w - viewportX / 2 - viewportY / 2;
        pixelUL = viewportUL + 0.5 * (pixelDX + pixelDY);
    }

    ray getRay(int x, int y, bool addRandom = false) const {
        double X = x;
        double Y = y;

        if (addRandom) {
            X += randomN() - 0.5;
            Y += randomN() - 0.5;
        }

        auto pixel = pixelUL + X * pixelDX + Y * pixelDY;
        return ray(camera, pixel - camera);
    }
};

#endif // VIEW_H

