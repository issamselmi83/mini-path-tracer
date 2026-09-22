#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include "sphere.h"

Vec3 ray_color(const Ray& r, const std::vector<Sphere>& world, int depth) {
    // Limite de rebonds : si on en a fait trop, plus de lumière ajoutée
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
        // Nouvelle direction aléatoire autour de la normale
        Vec3 target = closest_rec.point + closest_rec.normal + Vec3::random_in_unit_sphere();
        Ray bounced_ray(closest_rec.point, target - closest_rec.point);

        // Récursion : la couleur de ce point dépend de la couleur du rayon rebondi, atténuée
        return 0.5 * ray_color(bounced_ray, world, depth - 1);
    }

    // Fond (ciel)
    Vec3 unit_direction = r.direction.normalized();
    double t = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - t) * Vec3(1.0, 1.0, 1.0) + t * Vec3(0.5, 0.7, 1.0);
}

int main() {
    // Image
    const double aspect_ratio = 16.0 / 9.0;
    const int width = 400;
    const int height = static_cast<int>(width / aspect_ratio);
    const int samples_per_pixel = 50;
    const int max_depth = 10;

    // Scène : une sphère + un "sol" (grosse sphère en dessous)
    std::vector<Sphere> world;
    world.push_back(Sphere(Vec3(0, 0, -1), 0.5));
    world.push_back(Sphere(Vec3(0, -100.5, -1), 100));

    // Caméra
    double viewport_height = 2.0;
    double viewport_width = aspect_ratio * viewport_height;
    double focal_length = 1.0;

    Vec3 origin(0, 0, 0);
    Vec3 horizontal(viewport_width, 0, 0);
    Vec3 vertical(0, viewport_height, 0);
    Vec3 lower_left_corner = origin - horizontal / 2 - vertical / 2 - Vec3(0, 0, focal_length);

    // Rendu
    std::ofstream out("output.ppm");
    out << "P3\n" << width << " " << height << "\n255\n";

    for (int j = height - 1; j >= 0; --j) {
        for (int i = 0; i < width; ++i) {
            Vec3 color(0, 0, 0);

            for (int s = 0; s < samples_per_pixel; ++s) {
                double u = (i + double(rand()) / RAND_MAX) / (width - 1);
                double v = (j + double(rand()) / RAND_MAX) / (height - 1);

                Ray r(origin, lower_left_corner + horizontal * u + vertical * v - origin);
                color = color + ray_color(r, world, max_depth);
            }

            // Moyenne des échantillons + correction gamma (racine carrée)
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