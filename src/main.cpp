#include <fstream>
#include <iostream>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include "sphere.h"

Vec3 ray_color(const Ray& r, const std::vector<Sphere>& world) {
    HitRecord closest_rec;
    double closest_so_far = 1e9; // "infini" pour l'instant

    for (const auto& obj : world) {
        HitRecord temp_rec;
        if (obj.hit(r, 0.001, closest_so_far, temp_rec)) {
            closest_so_far = temp_rec.t;
            closest_rec = temp_rec;
        }
    }

    if (closest_rec.hit_anything) {
        Vec3 n = closest_rec.normal;
        return 0.5 * Vec3(n.x + 1, n.y + 1, n.z + 1);
    }

    // Fond : dégradé du rayon (comme avant)
    Vec3 unit_direction = r.direction.normalized();
    double t = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - t) * Vec3(1.0, 1.0, 1.0) + t * Vec3(0.5, 0.7, 1.0);
}

int main() {
    // Image
    const double aspect_ratio = 16.0 / 9.0;
    const int width = 400;
    const int height = static_cast<int>(width / aspect_ratio);

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
            double u = double(i) / (width - 1);
            double v = double(j) / (height - 1);

            Ray r(origin, lower_left_corner + horizontal * u + vertical * v - origin);
            Vec3 color = ray_color(r, world);

            int ir = static_cast<int>(255.999 * color.x);
            int ig = static_cast<int>(255.999 * color.y);
            int ib = static_cast<int>(255.999 * color.z);

            out << ir << ' ' << ig << ' ' << ib << '\n';
        }
    }

    out.close();
    std::cout << "Image generated: output.ppm\n";
    return 0;
}