  #ifndef TRIANGLE_H
  #define TRIANGLE_H
  #include "aabb.h"

struct Triangle {
    glm::vec3 v0;
    float _pad0;
    glm::vec3 v1;
    float _pad1;
    glm::vec3 v2;
    int material_index;
    glm::vec3 normal;   // flat face normal -- kept for e.g. front_face tests
    float _pad2;
    glm::vec3 n0;        // per-vertex normals, interpolated in the shader for smooth shading
    float _pad3;
    glm::vec3 n1;
    float _pad4;
    glm::vec3 n2;
    float _pad5;
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 uv2;
    glm::vec2 _pad6;

    // Full constructor -- used by the mesh loader, which has real per-vertex normals.
    Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int matIdx,
            glm::vec3 n0, glm::vec3 n1, glm::vec3 n2,
            glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f, 0.0f), 
            glm::vec2 uv2 = glm::vec2(0.0f, 1.0f)) :
        v0(v0), _pad0(0.0f), v1(v1), _pad1(0.0f), v2(v2), material_index(matIdx),
        _pad2(0.0f), n0(n0), _pad3(0.0f), n1(n1), _pad4(0.0f), n2(n2), _pad5(0.0f),
        uv0(uv0), uv1(uv1), uv2(uv2), _pad6(0.0f)
        {
            normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        }

    // Convenience constructor for hand-placed triangles with no real vertex-normal data
    // (e.g. the one in main.cpp). Collapses n0/n1/n2 to the flat face normal, so anything
    // using this overload renders exactly as it did before -- flat shaded.
    Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int matIdx,
            glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f, 0.0f), 
            glm::vec2 uv2 = glm::vec2(0.0f, 1.0f))
        : Triangle(v0, v1, v2, matIdx,
                    glm::normalize(glm::cross(v1 - v0, v2 - v0)),
                    glm::normalize(glm::cross(v1 - v0, v2 - v0)),
                    glm::normalize(glm::cross(v1 - v0, v2 - v0)),
                    uv0, uv1, uv2)
    {}
};

static_assert(sizeof(Triangle) == 144, "Triangle layout must match GLSL std430 Triangle struct");

inline AABB bboxTriangle(const Triangle& t)
{
    glm::vec3 mn = glm::min(t.v0, glm::min(t.v1, t.v2));
    glm::vec3 mx = glm::max(t.v0, glm::max(t.v1, t.v2));

    const float pad = 1e-4f;
    for (int a = 0; a < 3; ++a) {
        if (mx[a] - mn[a] < pad) { mn[a] -= pad; mx[a] += pad; }
    }

    return AABB(mn, mx);
}

#endif