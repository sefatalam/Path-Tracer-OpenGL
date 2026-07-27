#ifndef QUAD_H
#define QUAD_H

#include "aabb.h"

// Parameterized Quad = Q + αu + βv for α,β ∈ [0,1]
struct Quad {
    // Precompute plane normal, plane constant and unnormalized cross product (used to compute planar coordinates)
    glm::vec3 Q;        float D;
    glm::vec3 u;        int material_index;
    glm::vec3 v;        float _pad0;
    glm::vec3 normal;   float _pad1;
    glm::vec3 w;        float _pad2;

    Quad(glm::vec3 Q, glm::vec3 u, glm::vec3 v, int matIdx) :
        Q(Q), u(u), material_index(matIdx), v(v), _pad0(0.0f), _pad1(0.0f), _pad2(0.0f)
        {
            glm::vec3 n = glm::cross(u, v);
            normal = glm::normalize(n);
            D = glm::dot(normal, Q);
            w = n / glm::dot(n, n);
        }
};

static_assert(sizeof(Quad) == 80, "Quad layout must match GLSL std430 Quad struct");

inline AABB bboxQuad(const Quad& q)
{
    glm::vec3 p0 = q.Q;
    glm::vec3 p1 = q.Q + q.u;
    glm::vec3 p2 = q.Q + q.v;
    glm::vec3 p3 = q.Q + q.u + q.v;

    glm::vec3 qMin = glm::min(p0, glm::min(p1, glm::min(p2, p3)));
    glm::vec3 qMax = glm::max(p0, glm::max(p1, glm::max(p2, p3)));

    // Padding in case a quad lies flat on 2 axes
    // Prevents SAH breaking down (where surface area = 0)
    // Prevents false slab test results (where t_min == t_max for legitimate ray hits)
    const float pad = 1e-4f;
    for (int a = 0; a < 3; ++a) {
        if (qMax[a] - qMin[a] < pad) { qMin[a] -= pad; qMax[a] += pad; }
    }

    return AABB(qMin, qMax);
}

#endif