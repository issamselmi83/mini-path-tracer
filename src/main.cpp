#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include "material.h"
#include "sphere.h"

Vec3 ray_color(const Ray& r, const std::vector<Sphere>& world, int depth) {
    if (depth <= 0) return Vec3(0, 0, 0);

    HitRecord closest_rec;
    double closest_so_far = 1e9;

    for (const auto& obj : world) {
        HitRecord temp_rec;
        if (obj.hit(r, 0.001, closest_so_far, temp_rec)) {
            closest_so_far = temp_rec.t;
            closest_rec = temp_rec;
        }
    }

    if (closest_rec.hit_anything) {
        Ray scattered;
        Vec3 attenuation;
        if (closest_rec.mat_ptr->scatter(r, closest_rec, attenuation, scattered)) {
            return attenuation * ray_color(scattered, world, depth - 1);
        }
        return Vec3(0, 0, 0);
    }

    Vec3 unit_direction = r.direction.normalized();
    double t = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - t) * Vec3(1.0, 1.0, 1.0) + t * Vec3(0.5, 0.7, 1.0);
}

int main() {
    // Image
    const double aspect_ratio = 16.0 / 9.0;
    const int width = 400;
    const int height = static_cast<int>(width / aspect_ratio);
    const int samples_per_pixel = 100;
    const int max_depth = 10;

    // Matériaux
    std::vector<std::unique_ptr<Material>> materials;
    materials.push_back(std::make_unique<Lambertian>(Vec3(0.3, 0.8, 0.3)));  // sol vert
    materials.push_back(std::make_unique<Lambertian>(Vec3(0.8, 0.2, 0.2)));  // sphère rouge mate
    materials.push_back(std::make_unique<Metal>(Vec3(0.8, 0.8, 0.8), 0.05)); // sphère métal (presque miroir)
    materials.push_back(std::make_unique<Dielectric>(1.5));                  // verre

    // Scène : sphères plus espacées
    std::vector<Sphere> world;
    world.push_back(Sphere(Vec3(0, -100.5, -1), 100, materials[0].get()));   // sol
    world.push_back(Sphere(Vec3(-1.3, 0, -1), 0.5, materials[1].get()));     // mate
    world.push_back(Sphere(Vec3(0, 0, -1), 0.5, materials[2].get()));        // métal
    world.push_back(Sphere(Vec3(1.3, 0, -1), 0.5, materials[3].get()));      // verre (extérieur)
    world.push_back(Sphere(Vec3(1.3, 0, -1), -0.45, materials[3].get()));    // verre (intérieur, rayon négatif = coque creuse)

    // Caméra : position surélevée et légèrement de côté, plus intéressante qu'une vue frontale plate
    double vfov = 30.0; // champ de vision vertical en degrés
    Vec3 lookfrom(0, 1.2, 2.5);
    Vec3 lookat(0, 0, -1);
    Vec3 vup(0, 1, 0);

    double theta = vfov * M_PI / 180.0;
    double h = std::tan(theta / 2);
    double viewport_height = 2.0 * h;
    double viewport_width = aspect_ratio * viewport_height;

    Vec3 w = (lookfrom - lookat).normalized();
    Vec3 u = vup.cross(w).normalized();
    Vec3 v = w.cross(u);

    Vec3 origin = lookfrom;
    Vec3 horizontal = viewport_width * u;
    Vec3 vertical = viewport_height * v;
    Vec3 lower_left_corner = origin - horizontal / 2 - vertical / 2 - w;

    // Rendu
    std::ofstream out("output.ppm");
    out << "P3\n" << width << " " << height << "\n255\n";

    for (int j = height - 1; j >= 0; --j) {
        for (int i = 0; i < width; ++i) {
            Vec3 color(0, 0, 0);

            for (int s = 0; s < samples_per_pixel; ++s) {
                double su = (i + double(rand()) / RAND_MAX) / (width - 1);
                double sv = (j + double(rand()) / RAND_MAX) / (height - 1);

                Ray r(origin, lower_left_corner + horizontal * su + vertical * sv - origin);
                color = color + ray_color(r, world, max_depth);
            }

            double scale = 1.0 / samples_per_pixel;
            double r_c = std::sqrt(color.x * scale);
            double g_c = std::sqrt(color.y * scale);
            double b_c = std::sqrt(color.z * scale);

            int ir = static_cast<int>(256 * std::clamp(r_c, 0.0, 0.999));
            int ig = static_cast<int>(256 * std::clamp(g_c, 0.0, 0.999));
            int ib = static_cast<int>(256 * std::clamp(b_c, 0.0, 0.999));

            out << ir << ' ' << ig << ' ' << ib << '\n';
        }
    }

    out.close();
    std::cout << "Image generated: output.ppm\n";
    return 0;
}