#ifndef BVH_H
#define BVH_H

#include "prims.h"
#include <algorithm>
#include <vector>

enum PrimType
{
    NODE = 0,
    SPHERE = 1,
    TRIANGLE = 2,
    QUAD = 3,
    LEAF = 4,
};

struct BVHNode
{
    glm::vec3 aabb_min;
    PrimType prim_type;    // NODE (internal) or LEAF
    glm::vec3 aabb_max;
    int left;               // internal: left child index.  leaf: unused (-1)
    int right;              // internal: right child index. leaf: start offset into primRefs
    int axis;               // internal: split axis (0/1/2). leaf: unused
    int count;               // leaf: number of prims.       internal: unused
    float _pad;

    BVHNode() :
        aabb_min(FLT_MAX, FLT_MAX, FLT_MAX),
        prim_type(NODE),
        aabb_max(-FLT_MAX, -FLT_MAX, -FLT_MAX),
        left(-1), right(-1), axis(0), count(0), _pad(0.0f) {}
};

static_assert(sizeof(BVHNode) == 48, "BVHNode layout must match GLSL std430 BVHNode struct");

// GPU-facing indirection entry: one per primitive inside a leaf's range.
struct BVHPrimRef
{
    int type;
    int index;
};

static_assert(sizeof(BVHPrimRef) == 8, "BVHPrimRef layout must match GLSL std430 BVHPrimRef struct");

struct PrimitiveRef
{
    PrimType type;
    int index;
    AABB bounds;
    glm::vec3 centroid;

    PrimitiveRef(PrimType type, int index, AABB bounds, glm::vec3 centroid) :
        type(type), index(index), bounds(bounds), centroid(centroid) {}
};

std::vector<PrimitiveRef> buildRefs(const std::vector<Sphere>& spheres = {},
                                    const std::vector<Triangle>& triangles = {}, const std::vector<Quad>& quads = {})
{
    std::vector<PrimitiveRef> refs = {};

    for (size_t i = 0; i < spheres.size(); ++i) {
        AABB bounds = bboxSphere(spheres[i]);
        refs.push_back(PrimitiveRef(SPHERE, static_cast<int>(i), bounds, (bounds.min + bounds.max) * 0.5f));
    }

    for (size_t i = 0; i < triangles.size(); ++i) {
        AABB bounds = bboxTriangle(triangles[i]);
        refs.push_back(PrimitiveRef(TRIANGLE, static_cast<int>(i), bounds, (bounds.min + bounds.max) * 0.5f));
    }

    for (size_t i = 0; i < quads.size(); ++i) {
        AABB bounds = bboxQuad(quads[i]);
        refs.push_back(PrimitiveRef(QUAD, static_cast<int>(i), bounds, (bounds.min + bounds.max) * 0.5f));
    }

    return refs;
}

struct BVHBuilder
{
    std::vector<BVHNode> nodes;
    std::vector<BVHPrimRef> primRefs;

    static constexpr int MIN_LEAF_SIZE = 2;    // below this, don't bother running SAH
    static constexpr int MAX_LEAF_SIZE = 8;    // hard cap: force a split past this regardless of SAH verdict
    static constexpr int SAH_BINS = 16;
    static constexpr float C_TRAVERSAL = 4.0f;
    static constexpr float C_INTERSECT = 1.0f;

    int build(std::vector<PrimitiveRef>& refs, int start, int end)
    {
        BVHNode node;
        AABB bounds = refs[start].bounds;
        for (int i = start + 1; i < end; ++i) {
            bounds = AABB(bounds, refs[i].bounds);
        }
        node.aabb_min = bounds.min;
        node.aabb_max = bounds.max;

        int size = end - start;

        glm::vec3 cmin(FLT_MAX), cmax(-FLT_MAX);
        for (int i = start; i < end; ++i) {
            cmin = glm::min(cmin, refs[i].centroid);
            cmax = glm::max(cmax, refs[i].centroid);
        }
        glm::vec3 span = cmax - cmin;
        float maxSpan = glm::max(span.x, glm::max(span.y, span.z));

        // Too few prims to bother, or all centroids coincident (nothing to spatially split on).
        if (size <= MIN_LEAF_SIZE || maxSpan < 1e-8f) {
            return makeLeaf(node, refs, start, end);
        }

        float parentArea = surfaceArea(bounds);

        int bestAxis = -1;
        int bestSplit = -1;
        float bestCost = FLT_MAX;

        for (int axis = 0; axis < 3; ++axis) {
            if (span[axis] < 1e-8f) continue;

            struct Bin { AABB bounds = empty(); int count = 0; };
            std::vector<Bin> bins(SAH_BINS);

            float binScale = SAH_BINS / span[axis];
            auto binOf = [&](float c) {
                int b = static_cast<int>((c - cmin[axis]) * binScale);
                return std::clamp(b, 0, SAH_BINS - 1);
            };

            for (int i = start; i < end; ++i) {
                int b = binOf(refs[i].centroid[axis]);
                bins[b].bounds = AABB(bins[b].bounds, refs[i].bounds);
                bins[b].count++;
            }

            std::vector<AABB> leftBounds(SAH_BINS, empty());
            std::vector<int> leftCount(SAH_BINS, 0);
            AABB acc = empty();
            int accCount = 0;
            for (int b = 0; b < SAH_BINS; ++b) {
                acc = AABB(acc, bins[b].bounds);
                accCount += bins[b].count;
                leftBounds[b] = acc;
                leftCount[b] = accCount;
            }

            std::vector<AABB> rightBounds(SAH_BINS, empty());
            std::vector<int> rightCount(SAH_BINS, 0);
            acc = empty();
            accCount = 0;
            for (int b = SAH_BINS - 1; b >= 0; --b) {
                acc = AABB(acc, bins[b].bounds);
                accCount += bins[b].count;
                rightBounds[b] = acc;
                rightCount[b] = accCount;
            }

            // Evaluate the split between bin b and bin b+1.
            for (int b = 0; b < SAH_BINS - 1; ++b) {
                int nLeft = leftCount[b];
                int nRight = rightCount[b + 1];
                if (nLeft == 0 || nRight == 0) continue;

                float cost = C_TRAVERSAL +
                    (surfaceArea(leftBounds[b]) * nLeft + surfaceArea(rightBounds[b + 1]) * nRight)
                    / parentArea * C_INTERSECT;

                if (cost < bestCost) {
                    bestCost = cost;
                    bestAxis = axis;
                    bestSplit = b;
                }
            }
        }

        float leafCost = C_INTERSECT * static_cast<float>(size);
        bool forceSplit = size > MAX_LEAF_SIZE;

        if (bestAxis == -1 || (bestCost >= leafCost && !forceSplit)) {
            return makeLeaf(node, refs, start, end);
        }

        // Partition refs[start,end) by which side of bestSplit their centroid bin falls on.
        float binScale = SAH_BINS / span[bestAxis];
        auto belongsLeft = [&](const PrimitiveRef& r) {
            int b = std::clamp(static_cast<int>((r.centroid[bestAxis] - cmin[bestAxis]) * binScale), 0, SAH_BINS - 1);
            return b <= bestSplit;
        };

        auto midIt = std::partition(refs.begin() + start, refs.begin() + end, belongsLeft);
        int mid = static_cast<int>(midIt - refs.begin());

        // Degenerate partition safety net (shouldn't trigger given nLeft/nRight > 0 above, but guards the forceSplit path too).
        if (mid == start || mid == end) {
            mid = start + size / 2;
            std::nth_element(refs.begin() + start, refs.begin() + mid, refs.begin() + end,
                [bestAxis](const PrimitiveRef& a, const PrimitiveRef& b) {
                    return a.centroid[bestAxis] < b.centroid[bestAxis];
                });
        }

        node.prim_type = NODE;
        node.axis = bestAxis;
        int nodeIdx = static_cast<int>(nodes.size());
        nodes.push_back(node);

        int leftIdx = build(refs, start, mid);
        int rightIdx = build(refs, mid, end);
        nodes[nodeIdx].left = leftIdx;
        nodes[nodeIdx].right = rightIdx;

        return nodeIdx;
    }

    int makeLeaf(BVHNode node, std::vector<PrimitiveRef>& refs, int start, int end)
    {
        node.prim_type = LEAF;
        node.left = -1;
        node.right = static_cast<int>(primRefs.size());
        node.count = end - start;

        for (int i = start; i < end; ++i) {
            primRefs.push_back({ static_cast<int>(refs[i].type), refs[i].index });
        }

        nodes.push_back(node);
        return static_cast<int>(nodes.size()) - 1;
    }
};

#endif
