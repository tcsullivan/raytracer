#ifndef COLOR_H
#define COLOR_H

#include "vec3.h"

#include <algorithm>
#include <iostream>

#define GAMMA_CORRECT

using color = vec3;

void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

#ifdef GAMMA_CORRECT
    if (r > 0) r = sqrt(r);
    if (g > 0) g = sqrt(g);
    if (b > 0) b = sqrt(b);
#endif

    // Translate the [0,1] component values to the byte range [0,255].
    int rbyte = static_cast<int>(255.999 * std::clamp(r, 0.0, 1.0));
    int gbyte = static_cast<int>(255.999 * std::clamp(g, 0.0, 1.0));
    int bbyte = static_cast<int>(255.999 * std::clamp(b, 0.0, 1.0));

    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif

