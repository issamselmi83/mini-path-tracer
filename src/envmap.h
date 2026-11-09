#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "vec3.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#include "stb_image.h"

// Carte d'environnement (image HDR equirectangulaire) utilisee comme source de lumiere :
// c'est l'eclairage par image (IBL). Chaque direction qui s'echappe de la scene
// va chercher sa couleur dans l'image au lieu d'un simple degrade de ciel.
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
        loaded = true;
        std::cout << "Envmap chargee : " << path << " (" << width_ << "x" << height_ << ")\n";
        return true;
    }

    // dir doit etre non nulle (pas forcement normalisee). Interpolation bilineaire.
    Vec3 sample(const Vec3& dir) const {
        Vec3 d = dir.normalized();

        // Projection equirectangulaire : y = haut
        double phi = std::atan2(d.x, -d.z) + rotation;               // [-pi, pi] decale
        double u = phi / (2.0 * M_PI) + 0.5;
        u -= std::floor(u);                                           // [0, 1[
        double v = std::acos(std::clamp(d.y, -1.0, 1.0)) / M_PI;      // 0 = zenith, 1 = nadir

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

private:
    int width_ = 0, height_ = 0;
    std::vector<float> data_;

    Vec3 texel(int x, int y) const {
        x = ((x % width_) + width_) % width_;          // on boucle horizontalement
        y = std::clamp(y, 0, height_ - 1);             // on borne verticalement
        const float* p = &data_[(size_t(y) * width_ + x) * 3];
        return Vec3(p[0], p[1], p[2]);
    }
};
