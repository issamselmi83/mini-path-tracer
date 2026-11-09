#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "vec3.h"
#include "rng.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#include "stb_image.h"

// Carte d'environnement (image HDR equirectangulaire) utilisee comme source de lumiere :
// c'est l'eclairage par image (IBL). Chaque direction qui s'echappe de la scene
// va chercher sa couleur dans l'image au lieu d'un simple degrade de ciel.
//
// En plus de la lecture de l'image, la classe sait tirer des directions AU HASARD
// en favorisant les zones lumineuses (echantillonnage d'importance) : c'est ce qui permet
// de retrouver le soleil a coup sur depuis chaque point de la scene.
class EnvMap {
public:
    bool loaded = false;
    double intensity = 1.0;       // multiplicateur d'exposition
    double rotation = 0.0;        // rotation autour de l'axe vertical, en radians

    bool load(const std::string& path) {
        int n;
        float* pixels = stbi_loadf(path.c_str(), &width_, &height_, &n, 3);
        if (!pixels) {
            std::cerr << "Impossible de charger l'envmap '" << path << "' : "
                      << stbi_failure_reason() << "\n";
            return false;
        }
        data_.assign(pixels, pixels + size_t(width_) * height_ * 3);
        stbi_image_free(pixels);
        build_importance();
        loaded = true;
        std::cout << "Envmap chargee : " << path << " (" << width_ << "x" << height_ << ")\n";
        return true;
    }

    // dir doit etre non nulle (pas forcement normalisee). Interpolation bilineaire.
    Vec3 sample(const Vec3& dir) const {
        double u, v;
        direction_to_uv(dir, u, v);

        double fx = u * width_ - 0.5;
        double fy = v * height_ - 0.5;
        int x0 = int(std::floor(fx));
        int y0 = int(std::floor(fy));
        double tx = fx - x0;
        double ty = fy - y0;

        Vec3 c00 = texel(x0,     y0);
        Vec3 c10 = texel(x0 + 1, y0);
        Vec3 c01 = texel(x0,     y0 + 1);
        Vec3 c11 = texel(x0 + 1, y0 + 1);

        Vec3 top    = c00 * (1 - tx) + c10 * tx;
        Vec3 bottom = c01 * (1 - tx) + c11 * tx;
        return (top * (1 - ty) + bottom * ty) * intensity;
    }

    // --- Echantillonnage d'importance --------------------------------------------------

    // Tire une direction wi avec une probabilite proportionnelle a la luminosite de l'image
    // (ponderee par sin(theta), car les pixels pres des poles couvrent moins d'angle solide).
    // pdf = densite de probabilite par unite d'angle solide (steradian).
    void sample_direction(Vec3& wi, double& pdf) const {
        size_t W = width_, H = height_;
        double u1 = random_double(), u2 = random_double();

        size_t j = size_t(std::lower_bound(row_cdf_.begin(), row_cdf_.end(), u1) - row_cdf_.begin());
        j = std::min(j, H - 1);
        auto first = col_cdf_.begin() + j * W;
        size_t i = size_t(std::lower_bound(first, first + W, u2) - first);
        i = std::min(i, W - 1);

        // point uniforme a l'interieur du pixel choisi
        double px = (i + random_double()) / W;
        double py = (j + random_double()) / H;
        double theta = py * M_PI;
        double phi = (px - 0.5) * 2.0 * M_PI - rotation;
        double sin_t = std::sin(theta);
        wi = Vec3(sin_t * std::sin(phi), std::cos(theta), -sin_t * std::cos(phi));
        pdf = pdf_for_pixel(i, j, sin_t);
    }

    // Densite (par steradian) avec laquelle sample_direction() produirait la direction dir.
    // Sert a ponderer correctement les deux strategies d'echantillonnage (voir main.cpp).
    double pdf(const Vec3& dir) const {
        Vec3 d = dir.normalized();
        double u, v;
        direction_to_uv(d, u, v);
        size_t i = std::min(size_t(u * width_), size_t(width_ - 1));
        size_t j = std::min(size_t(v * height_), size_t(height_ - 1));
        double sin_t = std::sqrt(std::max(0.0, 1.0 - d.y * d.y));
        return pdf_for_pixel(i, j, sin_t);
    }

private:
    int width_ = 0, height_ = 0;
    std::vector<float> data_;
    std::vector<double> weights_;    // poids de chaque pixel (luminance * sin theta)
    std::vector<double> row_cdf_;    // repartition cumulee sur les lignes (normalisee)
    std::vector<double> col_cdf_;    // repartition cumulee sur les colonnes, ligne par ligne (normalisee)
    double total_weight_ = 1.0;

    void direction_to_uv(const Vec3& dir, double& u, double& v) const {
        Vec3 d = dir.normalized();
        // Projection equirectangulaire : y = haut
        double phi = std::atan2(d.x, -d.z) + rotation;
        u = phi / (2.0 * M_PI) + 0.5;
        u -= std::floor(u);                                        // [0, 1[
        v = std::acos(std::clamp(d.y, -1.0, 1.0)) / M_PI;          // 0 = zenith, 1 = nadir
    }

    double pdf_for_pixel(size_t i, size_t j, double sin_theta) const {
        if (sin_theta < 1e-8) return 0.0;
        double p_pixel = weights_[j * width_ + i] / total_weight_;
        // densite par unite de (u,v) = p_pixel * W * H ; angle solide = 2 pi^2 sin(theta) du dv
        return p_pixel * double(width_) * double(height_) / (2.0 * M_PI * M_PI * sin_theta);
    }

    void build_importance() {
        size_t W = width_, H = height_;
        weights_.assign(W * H, 0.0);
        row_cdf_.assign(H, 0.0);
        col_cdf_.assign(W * H, 0.0);

        auto luminance = [&](size_t i, size_t j) {
            const float* p = &data_[(j * W + i) * 3];
            return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        };

        double mean = 0.0;
        for (size_t j = 0; j < H; ++j)
            for (size_t i = 0; i < W; ++i) mean += luminance(i, j);
        mean /= double(W * H);
        // plancher : meme les pixels sombres gardent une probabilite non nulle
        // (condition pour que l'estimateur reste exact)
        double floor_value = 1e-3 * mean + 1e-6;

        std::vector<double> row_sum(H, 0.0);
        double total = 0.0;
        for (size_t j = 0; j < H; ++j) {
            double sin_t = std::sin((j + 0.5) / H * M_PI);
            double acc = 0.0;
            for (size_t i = 0; i < W; ++i) {
                double w = (luminance(i, j) + floor_value) * sin_t;
                weights_[j * W + i] = w;
                acc += w;
                col_cdf_[j * W + i] = acc;
            }
            row_sum[j] = acc;
            total += acc;
            row_cdf_[j] = total;
        }
        for (size_t j = 0; j < H; ++j) {
            for (size_t i = 0; i < W; ++i) col_cdf_[j * W + i] /= row_sum[j];
            row_cdf_[j] /= total;
        }
        total_weight_ = total;
    }

    Vec3 texel(int x, int y) const {
        x = ((x % width_) + width_) % width_;          // on boucle horizontalement
        y = std::clamp(y, 0, height_ - 1);             // on borne verticalement
        const float* p = &data_[(size_t(y) * width_ + x) * 3];
        return Vec3(p[0], p[1], p[2]);
    }
};
