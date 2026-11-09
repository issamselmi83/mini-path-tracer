#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "vec3.h"
#include "ray.h"
#include "rng.h"

// Volume a densite VARIABLE : une grille 3D de densites (nx * ny * nz valeurs dans [0, 1])
// posee dans une boite alignee sur les axes. La densite reelle en un point vaut
//     sigma(p) = sigma_scale * grille(p)      (interpolation trilineaire)
//
// Rendu par "delta tracking" (tracking de Woodcock) :
//   on choisit sigma_max >= sigma(p) partout (le "majorant"), puis on avance le long du rayon
//   par pas aleatoires de loi exponentielle de parametre sigma_max, comme dans un milieu homogene.
//   A chaque pas, on accepte la collision avec la probabilite sigma(p) / sigma_max :
//     - acceptee  : vraie collision -> le rayon est diffuse ici
//     - refusee   : "collision nulle" -> le rayon continue tout droit
//   Le resultat est exact (sans biais) : la proba de traverser vaut exp(-integrale de sigma le long du rayon).
class GridVolume {
public:
    Vec3 bmin, bmax;       // coins de la boite
    int nx, ny, nz;
    double sigma_scale;    // densite maximale (pour une valeur de grille egale a 1)
    Vec3 albedo;           // couleur de la lumiere diffusee

    GridVolume(const Vec3& bmin, const Vec3& bmax, int nx, int ny, int nz,
               double sigma_scale, const Vec3& albedo)
        : bmin(bmin), bmax(bmax), nx(nx), ny(ny), nz(nz),
          sigma_scale(sigma_scale), albedo(albedo),
          grid_(size_t(nx) * ny * nz, 0.0f) {}

    // --- Remplissage de la grille ---------------------------------------------------

    void fill_constant(float value) {
        std::fill(grid_.begin(), grid_.end(), value);
        update_majorant();
    }

    // Nuage procedural : bruit fractal (fBm) multiplie par un masque en forme de boule,
    // pour obtenir un nuage aux contours irreguliers qui s'efface vers les bords de la boite.
    void fill_cloud(double frequency = 4.0, int octaves = 5, double threshold = 0.22, double contrast = 2.6) {
        for (int k = 0; k < nz; ++k)
            for (int j = 0; j < ny; ++j)
                for (int i = 0; i < nx; ++i) {
                    double qx = (i + 0.5) / nx, qy = (j + 0.5) / ny, qz = (k + 0.5) / nz;
                    // position centree, rayon normalise : 0 au centre, 1 au milieu des faces
                    double cx = (qx - 0.5) * 2, cy = (qy - 0.5) * 2, cz = (qz - 0.5) * 2;
                    double r = std::sqrt(cx * cx + cy * cy + cz * cz);
                    double mask = 1.0 - smoothstep(0.35, 1.0, r);

                    double n = fbm(qx * frequency, qy * frequency, qz * frequency, octaves);
                    double d = (n * mask - threshold) * contrast;
                    grid_[index(i, j, k)] = float(std::clamp(d, 0.0, 1.0));
                }
        update_majorant();
    }

    // --- Acces ----------------------------------------------------------------------

    // Densite reelle sigma(p) (0 hors de la boite)
    double sigma(const Vec3& p) const {
        double u = (p.x - bmin.x) / (bmax.x - bmin.x);
        double v = (p.y - bmin.y) / (bmax.y - bmin.y);
        double w = (p.z - bmin.z) / (bmax.z - bmin.z);
        if (u < 0 || u > 1 || v < 0 || v > 1 || w < 0 || w > 1) return 0.0;

        // coordonnees dans la grille, centres de voxels a (i + 0.5)
        double fx = u * nx - 0.5, fy = v * ny - 0.5, fz = w * nz - 0.5;
        int x0 = int(std::floor(fx)), y0 = int(std::floor(fy)), z0 = int(std::floor(fz));
        double tx = fx - x0, ty = fy - y0, tz = fz - z0;

        double c000 = at(x0, y0, z0),         c100 = at(x0 + 1, y0, z0);
        double c010 = at(x0, y0 + 1, z0),     c110 = at(x0 + 1, y0 + 1, z0);
        double c001 = at(x0, y0, z0 + 1),     c101 = at(x0 + 1, y0, z0 + 1);
        double c011 = at(x0, y0 + 1, z0 + 1), c111 = at(x0 + 1, y0 + 1, z0 + 1);

        double c00 = c000 * (1 - tx) + c100 * tx, c10 = c010 * (1 - tx) + c110 * tx;
        double c01 = c001 * (1 - tx) + c101 * tx, c11 = c011 * (1 - tx) + c111 * tx;
        double c0 = c00 * (1 - ty) + c10 * ty, c1 = c01 * (1 - ty) + c11 * ty;
        return sigma_scale * (c0 * (1 - tz) + c1 * tz);
    }

    double sigma_max() const { return sigma_max_; }

    // true si le rayon interagit avec le volume avant t_max (delta tracking).
    bool sample_interaction(const Ray& r, double t_max, double& t_out) const {
        if (sigma_max_ <= 0.0) return false;

        double t0, t1;
        if (!intersect_box(r, t0, t1)) return false;
        t0 = std::max(t0, 0.001);
        t1 = std::min(t1, t_max);
        if (t0 >= t1) return false;

        double len = r.direction.length();
        double t = t0;
        while (true) {
            t += -std::log(1.0 - random_double()) / sigma_max_ / len;
            if (t >= t1) return false;                       // sorti du volume sans collision
            if (random_double() * sigma_max_ < sigma(r.at(t))) {
                t_out = t;                                   // vraie collision
                return true;
            }
            // sinon : collision nulle, on continue
        }
    }

private:
    std::vector<float> grid_;
    double sigma_max_ = 0.0;

    size_t index(int i, int j, int k) const { return (size_t(k) * ny + j) * nx + i; }

    double at(int i, int j, int k) const {
        i = std::clamp(i, 0, nx - 1);
        j = std::clamp(j, 0, ny - 1);
        k = std::clamp(k, 0, nz - 1);
        return grid_[index(i, j, k)];
    }

    void update_majorant() {
        float m = 0.0f;
        for (float v : grid_) m = std::max(m, v);
        sigma_max_ = double(m) * sigma_scale;   // l'interpolation trilineaire ne depasse jamais le max des voxels
    }

    bool intersect_box(const Ray& r, double& t0, double& t1) const {
        t0 = -1e30;
        t1 = 1e30;
        const double o[3] = {r.origin.x, r.origin.y, r.origin.z};
        const double d[3] = {r.direction.x, r.direction.y, r.direction.z};
        const double lo[3] = {bmin.x, bmin.y, bmin.z};
        const double hi[3] = {bmax.x, bmax.y, bmax.z};
        for (int a = 0; a < 3; ++a) {
            if (std::abs(d[a]) < 1e-12) {
                if (o[a] < lo[a] || o[a] > hi[a]) return false;   // parallele et hors de la boite
            } else {
                double ta = (lo[a] - o[a]) / d[a];
                double tb = (hi[a] - o[a]) / d[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) return false;
            }
        }
        return true;
    }

    // --- Bruit procedural (value noise 3D + fBm) ------------------------------------

    static double smoothstep(double a, double b, double x) {
        double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }

    static double hash(int x, int y, int z) {
        uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(z) * 2147483647u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= (h >> 16);
        return (h & 0xFFFFFF) / 16777216.0;
    }

    static double value_noise(double x, double y, double z) {
        int xi = int(std::floor(x)), yi = int(std::floor(y)), zi = int(std::floor(z));
        double tx = x - xi, ty = y - yi, tz = z - zi;
        tx = tx * tx * (3 - 2 * tx);
        ty = ty * ty * (3 - 2 * ty);
        tz = tz * tz * (3 - 2 * tz);
        auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
        double c00 = lerp(hash(xi, yi, zi),         hash(xi + 1, yi, zi),         tx);
        double c10 = lerp(hash(xi, yi + 1, zi),     hash(xi + 1, yi + 1, zi),     tx);
        double c01 = lerp(hash(xi, yi, zi + 1),     hash(xi + 1, yi, zi + 1),     tx);
        double c11 = lerp(hash(xi, yi + 1, zi + 1), hash(xi + 1, yi + 1, zi + 1), tx);
        return lerp(lerp(c00, c10, ty), lerp(c01, c11, ty), tz);
    }

    static double fbm(double x, double y, double z, int octaves) {
        double sum = 0.0, amp = 0.5, norm = 0.0;
        for (int o = 0; o < octaves; ++o) {
            sum += amp * value_noise(x, y, z);
            norm += amp;
            x *= 2.0; y *= 2.0; z *= 2.0;
            amp *= 0.5;
        }
        return sum / norm;
    }
};
