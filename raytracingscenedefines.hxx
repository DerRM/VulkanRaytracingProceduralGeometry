#ifndef RAYTRACINGSCENEDEFINES_HXX
#define RAYTRACINGSCENEDEFINES_HXX

#include "raytracingglsldefines.hxx"

#include <algorithm>

struct AABB
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

namespace GeometryType {
    enum Enum {
        Triangle = 0,
        AABB,
        Count
    };
}

namespace BottomLevelASType = GeometryType;

inline uint32_t max(uint32_t val0, uint32_t val1) {
    return val0 > val1 ? val0 : val1;
}

namespace IntersectionShaderType {
    enum Enum {
        AnalyticPrimitive = 0,
        VolumetricPrimitive,
        SignedDistancePrimitive,
        Count
    };

    inline uint32_t perPrimitiveTypeCount(Enum type) {
        switch (type)
        {

        case AnalyticPrimitive: return AnalyticPrimitive::Count;
        case VolumetricPrimitive: return VolumetricPrimitive::Count;
        case SignedDistancePrimitive: return SignedDistancePrimitive::Count;
        default: return 0;
        }
    }

    static const uint32_t kMaxPerPrimitiveTypeCount = max(AnalyticPrimitive::Count, max(VolumetricPrimitive::Count, SignedDistancePrimitive::Count));
    static const uint32_t kTotalPrimitiveCount = AnalyticPrimitive::Count + VolumetricPrimitive::Count + SignedDistancePrimitive::Count;
}

#endif // RAYTRACINGSCENEDEFINES_HXX
