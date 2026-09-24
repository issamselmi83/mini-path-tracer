#pragma once
#include "vec3.h"
#include "ray.h"

struct HitRecord; // déclaration anticipée, car hit_record.h va inclure material.h
inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * v.dot(n) * n;
}
class Material {
public:
    virtual ~Material() = default;
    // Retourne true si le rayon rebondit, remplit attenuation (couleur) et scattered (nouveau rayon)
    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const = 0;
};

class Lambertian : public Material {
public:
    Vec3 albedo; // la couleur "propre" du matériau

    Lambertian(const Vec3& albedo) : albedo(albedo) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const override;
};

class Metal : public Material {
public:
    Vec3 albedo;
    double fuzz; // 0 = miroir parfait, plus grand = plus flou

    Metal(const Vec3& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec, Vec3& attenuation, Ray& scattered) const override;
};
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
