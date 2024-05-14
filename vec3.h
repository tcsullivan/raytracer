#ifndef VEC3_H
#define VEC3_H

#include <cmath>
#include <iostream>

using std::sqrt;

struct vec3 {
  public:
    double e[3];

    constexpr vec3() : e{0,0,0} {}
    constexpr vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

    constexpr double x() const { return e[0]; }
    constexpr double y() const { return e[1]; }
    constexpr double z() const { return e[2]; }
    double& x() { return e[0]; }
    double& y() { return e[1]; }
    double& z() { return e[2]; }

    constexpr vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
    constexpr double operator[](int i) const { return e[i]; }
    constexpr double& operator[](int i) { return e[i]; }

    constexpr vec3& operator+=(const vec3& v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    constexpr vec3& operator*=(double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    constexpr vec3& operator/=(double t) {
        return *this *= 1/t;
    }

    constexpr vec3 operator+(const vec3& v) const {
        return vec3(e[0] + v.e[0], e[1] + v.e[1], e[2] + v.e[2]);
    }
    
    constexpr vec3 operator-(const vec3& v) const {
        return vec3(e[0] - v.e[0], e[1] - v.e[1], e[2] - v.e[2]);
    }
    
    constexpr vec3 operator*(const vec3& v) const {
        return vec3(e[0] * v.e[0], e[1] * v.e[1], e[2] * v.e[2]);
    }
    
    constexpr vec3 operator*(double t) const {
        return vec3(t*e[0], t*e[1], t*e[2]);
    }

    constexpr vec3 operator/(double t) const {
        t = 1 / t;
        return vec3(t*e[0], t*e[1], t*e[2]);
    }
    
    constexpr double length() const {
        return sqrt(length_squared());
    }

    constexpr double length_squared() const {
        return e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
    }

    constexpr vec3 normalize() const {
        auto v = *this;
        v /= length();
        return v;
    }

    constexpr double dot(const vec3& v) const {
        return e[0] * v.e[0] + e[1] * v.e[1] + e[2] * v.e[2];
    }

    constexpr vec3 reflect(vec3 v) const {
        v *= -2 * dot(v);
        v += *this;
        return v;
    }

    constexpr vec3 refract(vec3 v, double etaietat) const {
        auto inv = -(*this);
        auto cos_theta = std::min(inv.dot(v), 1.0);
        auto rperp = (*this + v * cos_theta) * etaietat;
        auto rpara = v * -std::sqrt(std::fabs(1.0 - rperp.length_squared()));
        return rperp + rpara;
    }

    static vec3 random() {
        return vec3(randomN(), randomN(), randomN());
    }

    //static vec3 random(double min, double max) {
    //    return vec3(randomN(min,max), randomN(min,max), randomN(min,max));
    //}
};

// point3 is just an alias for vec3, but useful for geometric clarity in the code.
using point3 = vec3;


// Vector Utility Functions

inline std::ostream& operator<<(std::ostream& out, const vec3& v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

constexpr inline vec3 operator*(double t, const vec3& v) {
    return v * t;
}
    
constexpr inline vec3 operator/(double t, const vec3& v) {
    return v * (1 / t);
}


inline constexpr vec3 cross(const vec3& u, const vec3& v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline constexpr vec3 unit_vector(const vec3& v) {
    return v / v.length();
}

inline vec3 randomUnitSphere() {
    for (;;) {
        if (auto p = vec3::random() * 2 - vec3(1, 1, 1); p.length_squared() < 1)
            return p;
    }
}

inline vec3 randomHemisphere(const vec3& normal) {
    vec3 unit = randomUnitSphere().normalize();
    return unit.dot(normal) > 0 ? unit : -unit;
}

#endif

