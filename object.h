#ifndef OBJECT_H
#define OBJECT_H

#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <optional>
#include <tuple>

enum class Material : int {
    Lambertian = 0,
    Metal,
    Dielectric,
    Undefined
};

struct Object
{
    point3 center;
    Material M;
    color tint;

    Object(point3 center_, Material M_, color tint_):
        center(center_), M(M_), tint(tint_) {}

    virtual std::pair<color, ray> scatter(const ray& r, double root) const = 0;
    virtual std::optional<double> hit(const ray& r, double tmin, double tmax) const = 0;
};

#endif // OBJECT_H

