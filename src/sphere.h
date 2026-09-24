#pragma once
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include "material.h"

class Sphere {
public:
    Vec3 center;
    double radius;
    const Material* mat_ptr;

    Sphere(const Vec3& center, double radius, const Material* mat_ptr)
        : center(center), radius(radius), mat_ptr(mat_ptr) {}

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
        Vec3 oc = r.origin - center;
        double a = r.direction.dot(r.direction);
        double b = 2.0 * oc.dot(r.direction);
        double c = oc.dot(oc) - radius * radius;
        double discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return false;

        double sqrt_d = std::sqrt(discriminant);
        double root = (-b - sqrt_d) / (2.0 * a);

        if (root < t_min || root > t_max) {
            root = (-b + sqrt_d) / (2.0 * a);
            if (root < t_min || root > t_max) return false;
        }

        rec.t = root;
        rec.point = r.at(root);
        rec.normal = (rec.point - center) / radius;
        rec.mat_ptr = mat_ptr;
        rec.hit_anything = true;
        return true;
    }
};