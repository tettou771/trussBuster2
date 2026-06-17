#pragma once

#include <TrussC.h>

using namespace std;
using namespace tc;

// A flat-shaded regular dodecahedron (12 pentagonal faces) of the given
// circumradius. Per-face normals give it the faceted "12-sided die" read used
// for armed mines. Verified planar/regular; pipeline3d does no back-face
// culling, so winding is irrelevant — only the (outward) normals matter.
inline Mesh createDodecahedron(float radius) {
    const float phi = 1.6180339887498949f;
    const float a   = 1.0f / phi;
    const float s   = radius / 1.7320508075688772f;   // verts have length sqrt(3)

    const Vec3 V[20] = {
        { 1, 1, 1}, { 1, 1,-1}, { 1,-1, 1}, { 1,-1,-1},
        {-1, 1, 1}, {-1, 1,-1}, {-1,-1, 1}, {-1,-1,-1},
        { 0, a, phi}, { 0, a,-phi}, { 0,-a, phi}, { 0,-a,-phi},
        { a, phi, 0}, { a,-phi, 0}, {-a, phi, 0}, {-a,-phi, 0},
        { phi, 0, a}, { phi, 0,-a}, {-phi, 0, a}, {-phi, 0,-a},
    };
    const int F[12][5] = {
        {0, 8, 10, 2, 16}, {0, 16, 17, 1, 12}, {0, 12, 14, 4, 8},
        {8, 4, 18, 6, 10}, {10, 6, 15, 13, 2}, {2, 13, 3, 17, 16},
        {1, 17, 3, 11, 9}, {1, 9, 5, 14, 12},  {4, 14, 5, 19, 18},
        {6, 18, 19, 7, 15}, {15, 7, 11, 3, 13}, {9, 11, 7, 19, 5},
    };

    Mesh m;
    unsigned int base = 0;
    for (auto& f : F) {
        Vec3 c(0, 0, 0);
        for (int k = 0; k < 5; k++) c += V[f[k]];
        Vec3 n = (c / 5.0f).normalized();    // outward face normal
        for (int k = 0; k < 5; k++) {
            m.addVertex(V[f[k]] * s);
            m.addNormal(n);
        }
        m.addTriangle(base + 0, base + 1, base + 2);
        m.addTriangle(base + 0, base + 2, base + 3);
        m.addTriangle(base + 0, base + 3, base + 4);
        base += 5;
    }
    return m;
}
