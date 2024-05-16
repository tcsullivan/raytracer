#ifndef WORLD_H
#define WORLD_H

#include "sphere.h"

#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

struct World
{
    std::vector<std::unique_ptr<Object>> objects;

    template<class T>
    void add(auto&&... args) {
        objects.emplace_back(new T(args...));
    }

    std::optional<std::pair<double, Object *>> hit(const ray& r) const {
        double closest = std::numeric_limits<double>::infinity();
        Object *sphere;

        for (const auto& o : objects) {
            if (auto t = o->hit(r, 0.001, closest); t) {
                closest = *t;
                sphere = o.get();
            }
        }

        if (closest != std::numeric_limits<double>::infinity())
            return std::pair {closest, sphere};
        else
            return {};
    }
};

#endif // WORLD_H

