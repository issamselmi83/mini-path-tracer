#pragma once
#include <random>

// Un generateur par thread : pas de verrou partage (rand() en a un),
// donc pas de contention quand 8 threads tirent des nombres en meme temps.
inline double random_double() {
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}
