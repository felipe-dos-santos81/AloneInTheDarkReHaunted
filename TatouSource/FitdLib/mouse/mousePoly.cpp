///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine's cover-zone polygon test. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mousePoly.h"

namespace mouse
{

int testCrossProduct(int x1, int z1, int x2, int z2, int x3, int z3, int x4, int z4)
{
    int returnFlag = 0;
    const int xAB = x1 - x2;
    const int yCD = z3 - z4;
    const int xCD = x3 - x4;
    const int yAB = z1 - z2;
    const int xAC = x1 - x3;
    const int yAC = z1 - z3;
    int dot = (xAB * yCD) - (xCD * yAC);
    if (dot)
    {
        int dda = xAC * yCD - xCD * yAC;
        int dmu = -xAB * yAC + xAC * yAB;
        if (dot < 0)
        {
            dot = -dot;
            dda = -dda;
            dmu = -dmu;
        }
        if (dda >= 0 && dmu >= 0 && dot >= dda && dot >= dmu)
            returnFlag = 1;
    }
    return returnFlag;
}

bool insideTwoRay(int x, int z, const std::vector<XZ>& poly)
{
    int flag = 0;
    const size_t n = poly.size();
    for (size_t j = 0; j < n; ++j)
    {
        const XZ a = poly[j];
        const XZ b = poly[(j + 1) % n];
        if (testCrossProduct(x, z, x - 10000, z, a.x, a.z, b.x, b.z))
            flag |= 1;
        if (testCrossProduct(x, z, x + 10000, z, a.x, a.z, b.x, b.z))
            flag |= 2;
    }
    return flag == 3;
}

bool insideCoverZone(int x, int z, const std::vector<XZ>& coverPoly)
{
    return insideTwoRay(floorDiv(x, kCoverScale), floorDiv(z, kCoverScale), coverPoly);
}

int floorDiv(int a, int b)
{
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

} // namespace mouse
