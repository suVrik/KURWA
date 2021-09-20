#pragma once

#include <core/scene/scene.h>

namespace kw {

class aabbox;
class AccelerationStructure;
class AnimationPlayer;
class frustum;
class GeometryPrimitive;
class LightPrimitive;
class ParticleSystemPlayer;
class ParticleSystemPrimitive;
class ReflectionProbeManager;
class ReflectionProbePrimitive;

struct RenderSceneDescriptor {
    AnimationPlayer* animation_player;
    ParticleSystemPlayer* particle_system_player;
    ReflectionProbeManager* reflection_probe_manager;
    AccelerationStructure* geometry_acceleration_structure;
    AccelerationStructure* light_acceleration_structure;
    AccelerationStructure* particle_system_acceleration_structure;
    AccelerationStructure* reflection_probe_acceleration_structure;
    MemoryResource* persistent_memory_resource;
    MemoryResource* transient_memory_resource;
};

class RenderScene : public virtual Scene {
public:
    explicit RenderScene(const RenderSceneDescriptor& descriptor);

    Vector<GeometryPrimitive*> query_geometry(const aabbox& bounds) const;
    Vector<GeometryPrimitive*> query_geometry(const frustum& frustum) const;
    
    Vector<LightPrimitive*> query_lights(const aabbox& bounds) const;
    Vector<LightPrimitive*> query_lights(const frustum& frustum) const;
    
    Vector<ParticleSystemPrimitive*> query_particle_systems(const aabbox& bounds) const;
    Vector<ParticleSystemPrimitive*> query_particle_systems(const frustum& frustum) const;

    Vector<ReflectionProbePrimitive*> query_reflection_probes(const aabbox& bounds) const;
    Vector<ReflectionProbePrimitive*> query_reflection_probes(const frustum& frustum) const;

protected:
    void child_added(Primitive& primitive) override;

private:
    AnimationPlayer& m_animation_player;
    ParticleSystemPlayer& m_particle_system_player;
    ReflectionProbeManager& m_reflection_probe_manager;
    AccelerationStructure& m_geometry_acceleration_structure;
    AccelerationStructure& m_light_acceleration_structure;
    AccelerationStructure& m_particle_system_acceleration_structure;
    AccelerationStructure& m_reflection_probe_acceleration_structure;
};

} // namespace kw
