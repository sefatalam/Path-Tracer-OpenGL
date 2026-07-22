#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

enum MaterialType : int
{
    LAMBERTIAN,
    METAL,
    DIELECTRIC,
    DIFFUSE_LIGHT
};

struct Material
{
    int texture_index;
    float fuzz;
    float ref_idx;
    MaterialType type;
    float emission_strength;
    float _pad0;
    float _pad1;
    float _pad2;

    Material(int texture_index, float fuzz, float ref_idx, MaterialType type, float emission_strength = 0.0f) :
        texture_index(texture_index), fuzz(fuzz), ref_idx(ref_idx), type(type),
        emission_strength(emission_strength), _pad0(0.0f), _pad1(0.0f), _pad2(0.0f) {}

    static Material Lambertian(int texture_index)
    {
        return Material(texture_index, 0.0f, 0.0f, LAMBERTIAN);
    }

    static Material Metal(int texture_index, float fuzz)
    {
        return Material(texture_index, fuzz, 0.0f, METAL);
    }

    static Material Dielectric(float ref_idx)
    {
        return Material(-1, 0.0f, ref_idx, DIELECTRIC);
    }

    // texture_index selects the emitted color (solid or image); emission_strength scales it,
    // so values above 1.0 are expected for anything meant to read as a bright light source.
    static Material DiffuseLight(int texture_index, float emission_strength = 1.0f)
    {
        return Material(texture_index, 0.0f, 0.0f, DIFFUSE_LIGHT, emission_strength);
    }
};

static_assert(sizeof(Material) == 32, "Material layout must match GLSL std430 Material struct");

#endif