#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <thread>
#include <chrono>
#include "vec3.h"
#include "ray.h"
#include "hit_record.h"
#include "material.h"
#include "sphere.h"
#include "volume.h"
#include "grid_volume.h"
#include "rng.h"
#include "envmap.h"

// Vrai si un objet ou un volume bloque le chemin de la lumiere le long de r (rayon d'ombre).
// Pour un volume, on tire "traverse / ne traverse pas" avec la bonne probabilite
// (Beer-Lambert) : en moyenne, on retrouve exactement la transmittance du volume.
static bool occluded(const Ray& r,
                     const std::vector<Sphere>& world,
                     const std::vector<Volume>& volumes,
                     const std::vector<GridVolume>& clouds) {
    HitRecord tmp;
    for (const auto& obj : world)
        if (obj.hit(r, 0.001, 1e9, tmp)) return true;
    double t;
    for (const auto& vol : volumes)
        if (vol.sample_interaction(r, 1e9, t)) return true;
    for (const auto& cloud : clouds)
        if (cloud.sample_interaction(r, 1e9, t)) return true;
    return false;
}

// prev_pdf : densite (par steradian) avec laquelle le DERNIER rebond a tire la direction de r,
// quand ce rebond a deja ete eclaire directement par l'envmap (surface diffuse ou volume).
// Si le rayon trouve l'envmap par hasard, on pondere sa contribution pour ne pas compter deux fois
// la meme lumiere (partage de poids entre les deux strategies : "multiple importance sampling").
// prev_pdf < 0 : pas d'eclairage direct au rebond precedent (camera, miroir, verre) -> poids 1.
Vec3 ray_color(const Ray& r,
               const std::vector<Sphere>& world,
               const std::vector<Volume>& volumes,
               const std::vector<GridVolume>& clouds,
               const EnvMap& env,
               int depth,
               double prev_pdf = -1.0) {
    if (depth <= 0) return Vec3(0, 0, 0);

    const double INV_4PI = 1.0 / (4.0 * M_PI);

    // 1. Objet solide le plus proche
    HitRecord closest_rec;
    double closest_so_far = 1e9;

    for (const auto& obj : world) {
        HitRecord temp_rec;
        if (obj.hit(r, 0.001, closest_so_far, temp_rec)) {
            closest_so_far = temp_rec.t;
            closest_rec = temp_rec;
        }
    }

    // 2. Interaction avec un volume, seulement si elle a lieu AVANT l'objet solide
    //    (on garde l'interaction la plus proche parmi tous les volumes)
    double t_limit = closest_so_far;
    bool volume_hit = false;
    Vec3 volume_albedo;
    for (const auto& vol : volumes) {          // brouillard homogene
        double t;
        if (vol.sample_interaction(r, t_limit, t)) {
            t_limit = t;
            volume_hit = true;
            volume_albedo = vol.albedo;
        }
    }
    for (const auto& cloud : clouds) {         // densite variable (delta tracking)
        double t;
        if (cloud.sample_interaction(r, t_limit, t)) {
            t_limit = t;
            volume_hit = true;
            volume_albedo = cloud.albedo;
        }
    }

    if (volume_hit) {
        Vec3 point = r.at(t_limit);

        // Eclairage direct par l'envmap : on tire une direction vers une zone lumineuse
        Vec3 direct(0, 0, 0);
        if (env.loaded) {
            Vec3 wi;
            double pe;
            env.sample_direction(wi, pe);
            if (pe > 0.0 && !occluded(Ray(point, wi), world, volumes, clouds)) {
                double w = pe * pe / (pe * pe + INV_4PI * INV_4PI);   // poids de cette strategie
                // albedo * phase(1/4pi) * Le / pdf * poids
                direct = volume_albedo * env.sample(wi) * (INV_4PI / pe * w);
            }
        }

        // Diffusion isotrope : nouvelle direction uniforme sur la sphere
        Ray scattered(point, Vec3::random_unit_vector());
        double next_pdf = env.loaded ? INV_4PI : -1.0;
        return direct + volume_albedo * ray_color(scattered, world, volumes, clouds, env, depth - 1, next_pdf);
    }

    // 3. Sinon : comportement habituel
    if (closest_rec.hit_anything) {
        Ray scattered;
        Vec3 attenuation;
        if (!closest_rec.mat_ptr->scatter(r, closest_rec, attenuation, scattered)) {
            return Vec3(0, 0, 0);
        }

        Vec3 direct(0, 0, 0);
        double next_pdf = -1.0;

        Vec3 albedo;
        if (env.loaded && closest_rec.mat_ptr->diffuse_albedo(albedo)) {
            const Vec3& n = closest_rec.normal;

            // Eclairage direct par l'envmap (surfaces diffuses)
            Vec3 wi;
            double pe;
            env.sample_direction(wi, pe);
            double cos_i = n.dot(wi);
            if (pe > 0.0 && cos_i > 0.0 &&
                !occluded(Ray(closest_rec.point, wi), world, volumes, clouds)) {
                double pdf_bsdf = cos_i / M_PI;
                double w = pe * pe / (pe * pe + pdf_bsdf * pdf_bsdf);
                // albedo/pi * cos * Le / pdf * poids
                direct = albedo * env.sample(wi) * (cos_i / M_PI / pe * w);
            }

            double cos_s = n.dot(scattered.direction.normalized());
            if (cos_s > 0.0) next_pdf = cos_s / M_PI;
        }

        return direct + attenuation * ray_color(scattered, world, volumes, clouds, env, depth - 1, next_pdf);
    }

    // 4. Le rayon s'echappe : lumiere venant de l'environnement
    if (env.loaded) {
        Vec3 le = env.sample(r.direction);   // IBL : couleur lue dans l'image HDR
        if (prev_pdf > 0.0) {
            double pe = env.pdf(r.direction);
            double w = prev_pdf * prev_pdf / (prev_pdf * prev_pdf + pe * pe);
            return le * w;
        }
        return le;
    }
    Vec3 unit_direction = r.direction.normalized();
    double t = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - t) * Vec3(1.0, 1.0, 1.0) + t * Vec3(0.5, 0.7, 1.0);
}

// Calcule les couleurs pour un intervalle de lignes [row_start, row_end)
void render_rows(
    int row_start, int row_end,
    int width, int height,
    int samples_per_pixel, int max_depth,
    const std::vector<Sphere>& world,
    const std::vector<Volume>& volumes,
    const std::vector<GridVolume>& clouds,
    const EnvMap& env, double max_radiance,
    const Vec3& origin, const Vec3& lower_left_corner,
    const Vec3& horizontal, const Vec3& vertical,
    std::vector<Vec3>& pixels
) {
    for (int j = row_start; j < row_end; ++j) {
        for (int i = 0; i < width; ++i) {
            Vec3 color(0, 0, 0);

            for (int s = 0; s < samples_per_pixel; ++s) {
                double su = (i + random_double()) / (width - 1);
                double sv = (j + random_double()) / (height - 1);

                Ray r(origin, lower_left_corner + horizontal * su + vertical * sv - origin);
                Vec3 sample = ray_color(r, world, volumes, clouds, env, max_depth);
                // Une envmap contient des valeurs tres au-dessus de 1 (le soleil) : un seul
                // echantillon peut alors "exploser" (fireflies). On borne chaque echantillon.
                sample.x = std::min(sample.x, max_radiance);
                sample.y = std::min(sample.y, max_radiance);
                sample.z = std::min(sample.z, max_radiance);
                color = color + sample;
            }

            double scale = 1.0 / samples_per_pixel;
            pixels[j * width + i] = Vec3(
                std::sqrt(color.x * scale),
                std::sqrt(color.y * scale),
                std::sqrt(color.z * scale)
            );
        }
    }
}

// Usage : ./path_tracer [samples_par_pixel] [nb_threads] [envmap.hdr] [intensite] [rotation_degres] [borne_par_echantillon]
int main(int argc, char** argv) {
    // Image
    const double aspect_ratio = 16.0 / 9.0;
    const int width = 800;
    const int height = static_cast<int>(width / aspect_ratio);
    const int samples_per_pixel = (argc > 1) ? std::atoi(argv[1]) : 100;
    const int max_depth = 50; // un peu plus haut : la diffusion dans un volume consomme des rebonds

    // Eclairage par image (optionnel)
    EnvMap env;
    if (argc > 3) {
        if (!env.load(argv[3])) return 1;
        if (argc > 4) env.intensity = std::atof(argv[4]);
        if (argc > 5) env.rotation = std::atof(argv[5]) * M_PI / 180.0;
    }
    // Borne par echantillon (voir render_rows) : plus c'est bas, moins il y a de points blancs,
    // mais plus on perd l'energie du soleil.
    const double max_radiance = (argc > 6) ? std::atof(argv[6]) : 50.0;

    // Materiaux
    std::vector<std::unique_ptr<Material>> materials;
    materials.push_back(std::make_unique<Lambertian>(Vec3(0.12, 0.3, 0.12)));  // sol vert
    materials.push_back(std::make_unique<Lambertian>(Vec3(0.8, 0.2, 0.2)));  // sphere rouge mate
    materials.push_back(std::make_unique<Metal>(Vec3(0.8, 0.8, 0.8), 0.05)); // sphere metal
    materials.push_back(std::make_unique<Dielectric>(1.5));                  // verre

    // Scene (objets solides)
    std::vector<Sphere> world;
    world.push_back(Sphere(Vec3(0, -100.5, -1), 100, materials[0].get()));
    world.push_back(Sphere(Vec3(-1.3, 0, -1), 0.5, materials[1].get()));
    world.push_back(Sphere(Vec3(0, 0, -1), 0.5, materials[2].get()));
    world.push_back(Sphere(Vec3(1.3, 0, -1), 0.5, materials[3].get()));
    world.push_back(Sphere(Vec3(1.3, 0, -1), -0.45, materials[3].get()));

    // Scene (milieux participants) : une boule de brouillard devant la scene
    std::vector<Volume> volumes;
    volumes.push_back(Volume(Vec3(0.65, -0.05, 0.1), 0.4, 3.0, Vec3(0.95, 0.95, 0.95)));

    // Nuage a densite variable : grille 3D 64^3 remplie de bruit fractal
    std::vector<GridVolume> clouds;
    {
        GridVolume cloud(Vec3(-1.8, 0.5, -3.6), Vec3(1.8, 2.0, -2.0), 64, 64, 64, 8.0, Vec3(0.98, 0.98, 0.98));
        cloud.fill_cloud(4.0, 6, 0.30, 3.5);
        clouds.push_back(cloud);
    }

    // Camera
    double vfov = 40.0;
    Vec3 lookfrom(0, 1.2, 2.5);
    Vec3 lookat(0, 0.5, -1);
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

    std::vector<Vec3> pixels(width * height);

    // 0 (ou pas d'argument) = autant de threads que de coeurs detectes
    unsigned int num_threads = (argc > 2) ? std::atoi(argv[2]) : 0;
    if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Rendering " << width << "x" << height << ", " << samples_per_pixel
              << " spp, " << num_threads << " thread(s)...\n";

    auto t_start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    int rows_per_thread = height / num_threads;

    for (unsigned int t = 0; t < num_threads; ++t) {
        int row_start = t * rows_per_thread;
        int row_end = (t == num_threads - 1) ? height : row_start + rows_per_thread;

        threads.emplace_back(
            render_rows,
            row_start, row_end,
            width, height,
            samples_per_pixel, max_depth,
            std::cref(world), std::cref(volumes), std::cref(clouds),
            std::cref(env), max_radiance,
            std::cref(origin), std::cref(lower_left_corner),
            std::cref(horizontal), std::cref(vertical),
            std::ref(pixels)
        );
    }

    for (auto& th : threads) th.join();

    auto t_end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t_end - t_start).count();
    std::cout << "Render time: " << seconds << " s\n";

    std::ofstream out("output.ppm");
    out << "P3\n" << width << " " << height << "\n255\n";

    for (int j = height - 1; j >= 0; --j) {
        for (int i = 0; i < width; ++i) {
            const Vec3& c = pixels[j * width + i];
            int ir = static_cast<int>(256 * std::clamp(c.x, 0.0, 0.999));
            int ig = static_cast<int>(256 * std::clamp(c.y, 0.0, 0.999));
            int ib = static_cast<int>(256 * std::clamp(c.z, 0.0, 0.999));
            out << ir << ' ' << ig << ' ' << ib << '\n';
        }
    }

    out.close();
    std::cout << "Image generated: output.ppm\n";
    return 0;
}
