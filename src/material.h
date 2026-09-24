#pragma once
#include <cmath>
#include "vec3.h"
#include "ray.h"

struct HitRecord; // déclaration anticipée

inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * v.dot(n) * n;
}

class Material {
public:
    virtual ~Material() = default;
    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const = 0;
};

class Lambertian : public Material {
public:
    Vec3 albedo;

    Lambertian(const Vec3& albedo) : albedo(albedo) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const override;
};

class Metal : public Material {
public:
    Vec3 albedo;
    double fuzz;

    Metal(const Vec3& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const override;
};

class Dielectric : public Material {
public:
    double ref_idx;

    Dielectric(double ref_idx) : ref_idx(ref_idx) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const override;
};

inline double schlick(double cosine, double ref_idx) {
    double r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * std::pow((1 - cosine), 5);
}

inline bool refract(const Vec3& v, const Vec3& n, double ni_over_nt, Vec3& refracted) {
    Vec3 uv = v.normalized();
    double dt = uv.dot(n);
    double discriminant = 1.0 - ni_over_nt * ni_over_nt * (1 - dt * dt);
    if (discriminant > 0) {
        refracted = ni_over_nt * (uv - n * dt) - n * std::sqrt(discriminant);
        return true;
    }
    return false;
}

#include "hit_record.h"

inline bool Lambertian::scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const {
    Vec3 scatter_direction = rec.normal + Vec3::random_in_unit_sphere();
    scattered = Ray(rec.point, scatter_direction);
    attenuation = albedo;
    return true;
}

inline bool Metal::scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const {
    Vec3 reflected = reflect(r_in.direction.normalized(), rec.normal);
    scattered = Ray(rec.point, reflected + fuzz * Vec3::random_in_unit_sphere());
    attenuation = albedo;
    return scattered.direction.dot(rec.normal) > 0;
}

inline bool Dielectric::scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const {
    attenuation = Vec3(1.0, 1.0, 1.0);
    double ni_over_nt;
    Vec3 outward_normal;
    double cosine;

    if (r_in.direction.dot(rec.normal) > 0) {
        outward_normal = rec.normal * -1.0;
        ni_over_nt = ref_idx;
        cosine = ref_idx * r_in.direction.dot(rec.normal) / r_in.direction.length();
    } else {
        outward_normal = rec.normal;
        ni_over_nt = 1.0 / ref_idx;
        cosine = -r_in.direction.dot(rec.normal) / r_in.direction.length();
    }

    Vec3 refracted;
    double reflect_prob;

    if (refract(r_in.direction, outward_normal, ni_over_nt, refracted)) {
        reflect_prob = schlick(cosine, ref_idx);
    } else {
        reflect_prob = 1.0;
    }

    if (double(rand()) / RAND_MAX < reflect_prob) {
        Vec3 reflected = reflect(r_in.direction, rec.normal);
        scattered = Ray(rec.point, reflected);
    } else {
        scattered = Ray(rec.point, refracted);
    }

    return true;
}