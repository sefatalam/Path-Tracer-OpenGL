#ifndef BVH_H
#define BVH_H

#include "prims.h"
#include <algorithm>
#include <iostream>
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
    PrimType prim_type;
    glm::vec3 aabb_max;
    int left; // NODE: index of left child, LEAF: -1 (no left child)
    int right; // NODE: index of right child, LEAF: offset into primRefs
    int axis; // NODE: split axis (0/1/2), LEAF: unused
    int count; // NODE: 0, LEAF: number of primitives
    float _pad;

    BVHNode() :
        aabb_min(FLT_MAX, FLT_MAX, FLT_MAX),
        prim_type(NODE),
        aabb_max(-FLT_MAX, -FLT_MAX, -FLT_MAX),
        left(-1), right(-1), axis(0), count(0), _pad(0.0f) {}
};

static_assert(sizeof(BVHNode) == 48, "BVHNode layout must match GLSL std430 BVHNode struct");

// Store PrimitiveRef information
struct BVHPrimRef
{
    int type;
    int index; // Index in std::vector<PrimitiveRef>
};

static_assert(sizeof(BVHPrimRef) == 8, "BVHPrimRef layout must match GLSL std430 BVHPrimRef struct");

// Store primitive information
struct PrimitiveRef
{
    PrimType type;
    int index; // Index in std::vector<Sphere/Triangle/Quad>
    AABB bounds;
    glm::vec3 centroid;

    PrimitiveRef(PrimType type, int index, AABB bounds, glm::vec3 centroid) :
        type(type), index(index), bounds(bounds), centroid(centroid) {}
};

// Flatten all primitive arrays, keep bound and centroid informaiton
inline std::vector<PrimitiveRef> buildRefs(const std::vector<Sphere>& spheres = {},
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

    // Mostly arbitrary values, could be improved empirically
    static constexpr int MIN_LEAF_SIZE = 2;
    static constexpr int MAX_LEAF_SIZE = 8;
    static constexpr int SAH_BINS = 16;
    static constexpr float C_TRAVERSAL = 4.0f;
    static constexpr float C_INTERSECT = 1.0f;

    // Recursively build tree top-down
    int build(std::vector<PrimitiveRef>& refs, int start, int end)
    {
        // Union AABBs to make BVH bounding box (Root = Universe (union all AABBs))
        BVHNode node;
        AABB bounds = refs[start].bounds;
        for (int i = start + 1; i < end; ++i) {
            bounds = AABB(bounds, refs[i].bounds);
        }
        node.aabb_min = bounds.min;
        node.aabb_max = bounds.max;

        int size = end - start;

        // Find biggest centroid span distance along all axes (biggest distance between primitives)
        glm::vec3 cmin(FLT_MAX), cmax(-FLT_MAX);
        for (int i = start; i < end; ++i) {
            cmin = glm::min(cmin, refs[i].centroid);
            cmax = glm::max(cmax, refs[i].centroid);
        }
        glm::vec3 span = cmax - cmin;
        float maxSpan = glm::max(span.x, glm::max(span.y, span.z));

        // Early leaf: too few primitives or primitives share centroids (cannot be split)
        if (size <= MIN_LEAF_SIZE || maxSpan < 1e-8f) {
            return makeLeaf(node, refs, start, end);
        }

        float parentArea = surfaceArea(bounds);

        int bestAxis = -1;
        int bestSplit = -1;
        float bestCost = FLT_MAX;

        // Determine split positions (across all axes)
        for (int axis = 0; axis < 3; ++axis) {
            if (span[axis] < 1e-8f) continue;

            // Divide each axis' centroid range into SAH_BINS bins
            struct Bin { AABB bounds = empty(); int count = 0; }; // Bin's AABB and number of primitives
            std::vector<Bin> bins(SAH_BINS);

            float binScale = SAH_BINS / span[axis];
            auto binOf = [&](float c) {
                int b = static_cast<int>((c - cmin[axis]) * binScale);
                return std::clamp(b, 0, SAH_BINS - 1);
            };

            // Place primitive into bin and update bounds and count
            for (int i = start; i < end; ++i) {
                int b = binOf(refs[i].centroid[axis]);
                bins[b].bounds = AABB(bins[b].bounds, refs[i].bounds);
                bins[b].count++;
            }

            // Cumulative count + union of bins 0 to b
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

            // Cumulative count + union of bins b to SAH_BINS
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

            // Evaluate optimal cost according to 
            // cost = C_traversal + (SA(left)/SA(parent)) * N_left * C_intersect + (SA(right)/SA(parent)) * N_right * C_intersect
            for (int b = 0; b < SAH_BINS - 1; ++b) {
                int nLeft = leftCount[b];
                int nRight = rightCount[b + 1];
                if (nLeft == 0 || nRight == 0) {
                    continue;
                }

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

        // Compare optimal split cost to cost of making a leaf instead
        float leafCost = C_INTERSECT * static_cast<float>(size);

        // Must split regardless of cost if too many primitives
        bool forceSplit = size > MAX_LEAF_SIZE;

        if (bestAxis == -1 || (bestCost >= leafCost && !forceSplit)) {
            return makeLeaf(node, refs, start, end);
        }

        // Rearrange refs such that everything on left side comes before right side
        float binScale = SAH_BINS / span[bestAxis];
        auto belongsLeft = [&](const PrimitiveRef& r) {
            int b = std::clamp(static_cast<int>((r.centroid[bestAxis] - cmin[bestAxis]) * binScale), 0, SAH_BINS - 1);
            return b <= bestSplit;
        };

        auto midIt = std::partition(refs.begin() + start, refs.begin() + end, belongsLeft);
        int mid = static_cast<int>(midIt - refs.begin());

        // Fallback median split if everything lands on one side
        if (mid == start || mid == end) {
            mid = start + size / 2;
            std::nth_element(refs.begin() + start, refs.begin() + mid, refs.begin() + end,
                [bestAxis](const PrimitiveRef& a, const PrimitiveRef& b) {
                    return a.centroid[bestAxis] < b.centroid[bestAxis];
                });
        }

        // Create node
        node.prim_type = NODE;
        node.axis = bestAxis;
        int nodeIdx = static_cast<int>(nodes.size());
        nodes.push_back(node);

        // Backfill left and right children after recursion
        int leftIdx = build(refs, start, mid);
        int rightIdx = build(refs, mid, end);
        nodes[nodeIdx].left = leftIdx;
        nodes[nodeIdx].right = rightIdx;

        return nodeIdx;
    }

    // Create leaf
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

// inline void printBVHStats(const BVHBuilder& builder)
// {
//     int leafCount = 0;
//     int maxLeafSize = 0;
//     long long leafPrimTotal = 0;
//     for (const BVHNode& n : builder.nodes) {
//         if (n.prim_type == LEAF) {
//             leafCount++;
//             leafPrimTotal += n.count;
//             maxLeafSize = std::max(maxLeafSize, n.count);
//         }
//     }
//     std::cout << "BVH stats: " << builder.nodes.size() << " nodes, "
//               << leafCount << " leaves, " << builder.primRefs.size() << " prim refs, "
//               << "avg leaf size " << (leafCount ? (double)leafPrimTotal / leafCount : 0.0)
//               << ", max leaf size " << maxLeafSize << "\n";
// }

#endif
