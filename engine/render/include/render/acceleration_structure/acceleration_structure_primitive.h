#pragma once

#include "render/scene/primitive.h"

#include <core/math/aabbox.h>

namespace kw {

class AccelerationStructure;

class AccelerationStructurePrimitive : public Primitive {
public:
    explicit AccelerationStructurePrimitive(const transform& local_transform = transform());
    ~AccelerationStructurePrimitive() override;

    // Acceleration structure is set from AccelerationStructure's `add` method.
    AccelerationStructure* get_acceleration_structure() const {
        return m_acceleration_structure;
    }

    const aabbox& get_bounds() const {
        return m_bounds;
    }

protected:
    // TODO: Call acceleration structure's `update` from `global_transform_updated`.

protected:
    AccelerationStructure* m_acceleration_structure;
    aabbox m_bounds;
    void* m_node;

    friend class LinearAccelerationStructure;
    friend class OctreeAccelerationStructure;
    friend class QuadtreeAccelerationStructure;
};

} // namespace kw
