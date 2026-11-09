#pragma once
#include <algorithm>
#include <cmath>
#include "vec3.h"
#include "ray.h"
#include "rng.h"

// Milieu participant homogene : une sphere de brouillard / fumee a densite constante.
//
// Loi de Beer-Lambert : sur une distance d dans le milieu, la fraction de lumiere
// qui traverse SANS interagir est  T = exp(-density * d).
// Methode Monte Carlo : on tire une distance d_int dont la loi est exactement celle-la
//     d_int = -ln(u) / density      (u uniforme dans ]0,1])
// - si d_int < distance parcourue dans la sphere : le rayon interagit (diffusion)
// - sinon : il traverse le volume sans etre devie.
// Sur beaucoup d'echantillons, la proportion de rayons qui traversent vaut bien exp(-density * d).
struct Volume {
    Vec3 center;
    double radius;
    double density;   // sigma_t : plus c'est grand, plus le milieu est opaque
    Vec3 albedo;      // couleur de la lumiere diffusee (blanc = brouillard, sombre = fumee)

    Volume(const Vec3& center, double radius, double density, const Vec3& albedo)
        : center(center), radius(radius), density(density), albedo(albedo) {}

    // true si le rayon interagit avec le volume avant t_max.
    // t_out = position de l'interaction le long du rayon.
    bool sample_interaction(const Ray& r, double t_max, double& t_out) const {
        Vec3 oc = r.origin - center;
        double a = r.direction.dot(r.direction);
        double b = 2.0 * oc.dot(r.direction);
        double c = oc.dot(oc) - radius * radius;
        double discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        double sqrt_d = std::sqrt(discriminant);
        double t0 = (-b - sqrt_d) / (2.0 * a);
        double t1 = (-b + sqrt_d) / (2.0 * a);

        // On ne garde que la portion du rayon situee devant l'origine et avant t_max
        // (t_max = objet solide ou autre volume deja plus proche)
        t0 = std::max(t0, 0.001);
        t1 = std::min(t1, t_max);
        if (t0 >= t1) return false;

        double len = r.direction.length();
        double distance_inside = (t1 - t0) * len;

        // 1 - u pour eviter log(0)
        double interaction_distance = -std::log(1.0 - random_double()) / density;
        if (interaction_distance >= distance_inside) return false;

        t_out = t0 + interaction_distance / len;
        return true;
    }
};
