#ifndef WORLD_H
#define WORLD_H

#include "sphere.h"

#include <limits>
#include <optional>
#include <tuple>
#include <vector>

struct World
{
    std::vector<Sphere> objects;

    void add(auto&&... args) {
        objects.emplace_back(args...);
    }

    std::optional<std::pair<double, Sphere>> hit(const ray& r) const {
        double closest = std::numeric_limits<double>::infinity();
        Sphere sphere;

        for (const auto& o : objects) {
            if (auto t = o.hit(r, 0.001, closest); t) {
                closest = *t;
                sphere = o;
            }
        }

        if (closest != std::numeric_limits<double>::infinity())
            return std::pair {closest, sphere};
        else
            return {};
    }
};

#endif // WORLD_H

