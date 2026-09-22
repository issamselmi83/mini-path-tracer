#pragma once
#include "vec3.h"

struct HitRecord {
    Vec3 point;      // le point d'impact
    Vec3 normal;      // la normale à la surface à cet endroit
    double t;         // la distance le long du rayon
    bool hit_anything = false;
};