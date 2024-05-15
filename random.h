#ifndef RANDOM_H
#define RANDOM_H

#include <random>

inline double randomN()
{
    static std::uniform_real_distribution<double> distribution (0.0, 1.0);
    static std::mt19937 generator (std::random_device{}());
    return distribution(generator);
}

#endif // RANDOM_H

