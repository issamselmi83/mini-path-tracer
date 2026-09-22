#pragma once
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"

class Sphere {
public:
    Vec3 center;
    double radius;

    Sphere(const Vec3& center, double radius) : center(center), radius(radius) {}

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
        Vec3 oc = r.origin - center;
        double a = r.direction.dot(r.direction);
        double b = 2.0 * oc.dot(r.direction);
        double c = oc.dot(oc) - radius * radius;
        double discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return false;

        double sqrt_d = std::sqrt(discriminant);
        double root = (-b - sqrt_d) / (2.0 * a);

        // Si la racine la plus proche est hors de l'intervalle valide, on essaie l'autre
        if (root < t_min || root > t_max) {
            root = (-b + sqrt_d) / (2.0 * a);
            if (root < t_min || root > t_max) return false;
        }

        rec.t = root;
        rec.point = r.at(root);
        rec.normal = (rec.point - center) / radius;
        rec.hit_anything = true;
        return true;
    }
};