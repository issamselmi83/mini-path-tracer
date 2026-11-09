#include <iostream>
#include <iomanip>
#include "../src/grid_volume.h"

// Mesure la proportion de rayons qui traversent le volume sans collision (Monte Carlo)
// et la compare a exp(-integrale de sigma le long du rayon), integrale calculee par quadrature.
static void check(const GridVolume& g, const Ray& r, const char* name) {
    // quadrature (pas fin) sur la portion du rayon dans la boite
    double len = r.direction.length(), optical = 0.0, ds = 1e-4;
    for (double t = 0; t < 20; t += ds / len) optical += g.sigma(r.at(t)) * ds;
    double theory = std::exp(-optical);
    const int N = 1000000; int through = 0;
    for (int i = 0; i < N; ++i) { double t; if (!g.sample_interaction(r, 1e9, t)) ++through; }
    double meas = double(through) / N;
    double sigma_err = std::sqrt(theory * (1 - theory) / N);
    std::cout << std::fixed << std::setprecision(5) << name
              << " | profondeur optique = " << optical
              << " | theorie = " << theory << " | mesure = " << meas
              << " | ecart = " << std::abs(meas - theory) / sigma_err << " sigma\n";
}

int main() {
    // 1. densite constante : doit retrouver Beer-Lambert (L = 2, sigma = 1.5)
    GridVolume a(Vec3(-1,-1,-1), Vec3(1,1,1), 16,16,16, 1.5, Vec3(1,1,1));
    a.fill_constant(1.0f);
    check(a, Ray(Vec3(0,0,-5), Vec3(0,0,1)), "constant   ");
    check(a, Ray(Vec3(0,0,-5), Vec3(0,0,2.5)), "constant,dir non norm.");

    // 2. nuage procedural : densite variable
    GridVolume c(Vec3(-1,-1,-1), Vec3(1,1,1), 64,64,64, 6.0, Vec3(1,1,1));
    c.fill_cloud();
    std::cout << "sigma_max nuage = " << c.sigma_max() << "\n";
    check(c, Ray(Vec3(0,0,-5), Vec3(0,0,1)), "nuage axe z ");
    check(c, Ray(Vec3(0.3,-0.2,-5), Vec3(0,0,1)), "nuage decale ");
    check(c, Ray(Vec3(-5,0.1,0.2), Vec3(1,0,0)), "nuage axe x ");
    check(c, Ray(Vec3(-4,-4,-4), Vec3(1,1,1.1)), "nuage oblique");
}
