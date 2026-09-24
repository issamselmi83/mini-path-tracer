#pragma once
#include "vec3.h"

class Material; // déclaration anticipée

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    double t;
    bool hit_anything = false;
    const Material* mat_ptr = nullptr; // pointeur vers le matériau touché
};