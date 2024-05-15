#ifndef SPHERE_H
#define SPHERE_H

#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <cmath>
#include <optional>
#include <tuple>

enum class Material : int {
    Lambertian = 0,
    Metal,
    Dielectric,
    Undefined
};

struct Sphere
{
    point3 center;
    double radius;
    Material M;
    color tint;

    std::pair<color, ray> scatter(const ray& r, double root) const {
        const auto p = r.at(root);
        auto normal = (p - center) / radius;

        if (M == Material::Lambertian) {
            return {tint, ray(p, normal + randomUnitSphere())};
        } else if (M == Material::Metal) {
            return {tint, ray(p, r.direction().reflect(normal))};
        } else if (M == Material::Dielectric) {
            constexpr auto index = 1.0 / 1.33;

            const bool front = r.direction().dot(normal) < 0;
            const auto ri = front ? 1.0 / index : index;
            if (!front)
                normal *= -1;

            const auto dir = r.direction().normalize();
            const double costh = std::fmin((-dir).dot(normal), 1);
            const double sinth = std::sqrt(1 - costh * costh);

            if (ri * sinth > 1)
                return {color(1, 1, 1), ray(p, dir.reflect(normal))};
            else
                return {color(1, 1, 1), ray(p, dir.refract(normal, ri))};
        } else {
            return {};
        }
    }

    std::optional<double> hit(const ray& r, double tmin, double tmax) const {
        const vec3 oc = center - r.origin();
        const auto a = r.direction().length_squared();
        const auto h = r.direction().dot(oc);
        const auto c = oc.length_squared() - radius * radius;
        const auto discriminant = h * h - a * c;

        if (discriminant < 0) {
            return {}; // No hit
        } else {
            const auto sqrtd = std::sqrt(discriminant);

            // Find the nearest root that lies in the acceptable range.
            auto root = (h - sqrtd) / a;
            if (root <= tmin || tmax <= root) {
                root = (h + sqrtd) / a;
                if (root <= tmin || tmax <= root)
                    return {};
            }

            return root;
        }
    }
};

#endif // SPHERE_H

