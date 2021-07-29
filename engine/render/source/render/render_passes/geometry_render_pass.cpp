#include "render/render_passes/geometry_render_pass.h"
#include "render/geometry/geometry.h"
#include "render/geometry/geometry_primitive.h"
#include "render/material/material.h"
#include "render/scene/scene.h"

#include <core/concurrency/task.h>
#include <core/containers/unordered_map.h>
#include <core/debug/assert.h>
#include <core/math/aabbox.h>
#include <core/math/float4x4.h>
#include <core/memory/memory_resource.h>

#include <algorithm>

namespace kw {

class GeometryRenderPass::Task : public kw::Task {
public:
    Task(GeometryRenderPass& render_pass)
        : m_render_pass(render_pass)
    {
    }

    void run() override {
        RenderPassContext* context = m_render_pass.begin();
        if (context != nullptr) {
            // TODO: Use camera bounds.
            Vector<GeometryPrimitive*> primitives = m_render_pass.m_scene.query_geometry(aabbox(float3(), float3(500.f)));

            // Sort primitives by graphics pipeline (to avoid graphics pipeline switches),
            // by material (to avoid rebinding uniform data), by geometry (for instancing).
            std::sort(primitives.begin(), primitives.end(), GeometrySort());

            // MSVC is freaking out because of iterating past the end iterator.
            if (primitives.empty()) return;

            auto from_it = primitives.begin();
            for (auto to_it = ++primitives.begin(); to_it <= primitives.end(); ++to_it) {
                if (to_it == primitives.end() ||
                    (*to_it)->get_geometry() != (*from_it)->get_geometry() ||
                    (*to_it)->get_material() != (*from_it)->get_material() ||
                    (*from_it)->get_material()->is_skinned())
                {
                    if ((*from_it)->get_geometry() && (*from_it)->get_material()) {
                        Geometry& geometry = *(*from_it)->get_geometry();
                        Material& material = *(*from_it)->get_material();

                        VertexBuffer* vertex_buffer = geometry.get_vertex_buffer();
                        IndexBuffer* index_buffer = geometry.get_index_buffer();
                        uint32_t index_count = geometry.get_index_count();

                        VertexBuffer* instance_buffer = nullptr;
                        size_t instance_buffer_count = 0;

                        if (!material.is_skinned()) {
                            Vector<Material::InstanceData> instances_data(m_render_pass.m_transient_memory_resource);
                            instances_data.reserve(to_it - from_it);

                            for (auto it = from_it; it != to_it; ++it) {
                                Material::InstanceData instance_data;
                                instance_data.model = float4x4((*it)->get_global_transform());
                                instance_data.inverse_model = inverse(instance_data.model);
                                instances_data.push_back(instance_data);
                            }

                            instance_buffer = context->get_render().acquire_transient_vertex_buffer(instances_data.data(), instances_data.size() * sizeof(Material::InstanceData));
                            KW_ASSERT(instance_buffer != nullptr);

                            instance_buffer_count = 1;
                        }

                        Vector<Texture*> uniform_textures(m_render_pass.m_transient_memory_resource);
                        uniform_textures.reserve(material.get_textures().size());

                        for (const SharedPtr<Texture*>& texture : material.get_textures()) {
                            KW_ASSERT(texture && *texture != nullptr);
                            uniform_textures.push_back(*texture);
                        }

                        UniformBuffer* uniform_buffer = nullptr;
                        size_t uniform_buffer_count = 0;

                        if (material.is_skinned()) {
                            Material::UniformData uniform_data{};

                            // TODO: Skinning.
                            //if ((*from_it)->get_skeleton()) {
                            //    Skeleton& skeleton = *(*from_it)->get_skeleton();
                            //    // TODO: Fill `uniform_data` from `skeleton`.
                            //} else {
                            //    // TODO: Use some default values for `uniform_data`.
                            //}

                            uniform_buffer = context->get_render().acquire_transient_uniform_buffer(&uniform_data, sizeof(uniform_data));
                            KW_ASSERT(uniform_buffer != nullptr);

                            uniform_buffer_count = 1;
                        }

                        Material::PushConstants geometry_push_constants{};
                        geometry_push_constants.view_projection = m_render_pass.m_scene.get_camera().get_view_projection_matrix();

                        DrawCallDescriptor draw_call_descriptor{};
                        draw_call_descriptor.graphics_pipeline = *material.get_graphics_pipeline();
                        draw_call_descriptor.vertex_buffers = &vertex_buffer;
                        draw_call_descriptor.vertex_buffer_count = 1;
                        draw_call_descriptor.instance_buffers = &instance_buffer;
                        draw_call_descriptor.instance_buffer_count = instance_buffer_count;
                        draw_call_descriptor.index_buffer = index_buffer;
                        draw_call_descriptor.index_count = index_count;
                        draw_call_descriptor.instance_count = to_it - from_it;
                        draw_call_descriptor.stencil_reference = 0xFF;
                        draw_call_descriptor.uniform_textures = uniform_textures.data();
                        draw_call_descriptor.uniform_texture_count = uniform_textures.size();
                        draw_call_descriptor.uniform_buffers = &uniform_buffer;
                        draw_call_descriptor.uniform_buffer_count = uniform_buffer_count;
                        draw_call_descriptor.push_constants = &geometry_push_constants;
                        draw_call_descriptor.push_constants_size = sizeof(geometry_push_constants);

                        context->draw(draw_call_descriptor);
                    }

                    // MSVC is freaking out because of iterating past the end iterator.
                    if (to_it == primitives.end()) break;
                    else from_it = to_it;
                }
            }
        }
    }

    const char* get_name() const {
        return "Geometry Render Pass";
    }

private:
    struct GeometrySort {
    public:
        bool operator()(GeometryPrimitive* a, GeometryPrimitive* b) const {
            if (a->get_material()->get_graphics_pipeline() == b->get_material()->get_graphics_pipeline()) {
                if (a->get_material() == b->get_material()) {
                    if (a->get_geometry() == b->get_geometry()) {
                        return a < b;
                    }
                    return a->get_geometry() < b->get_geometry();
                }
                return a->get_material() < b->get_material();
            }
            return a->get_material()->get_graphics_pipeline() < b->get_material()->get_graphics_pipeline();
        }
    };

    GeometryRenderPass& m_render_pass;
};

GeometryRenderPass::GeometryRenderPass(Render& render, Scene& scene, MemoryResource& transient_memory_resource)
    : m_render(render)
    , m_scene(scene)
    , m_transient_memory_resource(transient_memory_resource)
{
}

void GeometryRenderPass::get_color_attachment_descriptors(Vector<AttachmentDescriptor>& attachment_descriptors) {
    attachment_descriptors.push_back(AttachmentDescriptor{ "albedo_ao_attachment", TextureFormat::RGBA8_UNORM,  LoadOp::DONT_CARE });
    attachment_descriptors.push_back(AttachmentDescriptor{ "normal_roughness_attachment", TextureFormat::RGBA16_SNORM, LoadOp::DONT_CARE });
    attachment_descriptors.push_back(AttachmentDescriptor{ "emission_metalness_attachment", TextureFormat::RGBA8_UNORM, LoadOp::DONT_CARE });
}

void GeometryRenderPass::get_depth_stencil_attachment_descriptors(Vector<AttachmentDescriptor>& attachment_descriptors) {
    AttachmentDescriptor depth_stencil_attachment_descriptor{};
    depth_stencil_attachment_descriptor.name = "depth_attachment";
    depth_stencil_attachment_descriptor.format = TextureFormat::D24_UNORM_S8_UINT;
    depth_stencil_attachment_descriptor.clear_depth = 1.f;
    attachment_descriptors.push_back(depth_stencil_attachment_descriptor);
}

void GeometryRenderPass::get_render_pass_descriptors(Vector<RenderPassDescriptor>& render_pass_descriptors) {
    static const char* const COLOR_ATTACHMENT_NAMES[] = {
        "albedo_ao_attachment",
        "normal_roughness_attachment",
        "emission_metalness_attachment",
    };

    RenderPassDescriptor render_pass_descriptor{};
    render_pass_descriptor.name = "geometry_render_pass";
    render_pass_descriptor.render_pass = this;
    render_pass_descriptor.write_color_attachment_names = COLOR_ATTACHMENT_NAMES;
    render_pass_descriptor.write_color_attachment_name_count = std::size(COLOR_ATTACHMENT_NAMES);
    render_pass_descriptor.write_depth_stencil_attachment_name = "depth_attachment";
    render_pass_descriptors.push_back(render_pass_descriptor);
}

void GeometryRenderPass::create_graphics_pipelines(FrameGraph& frame_graph) {
    // All graphics pipelines are stored in geometry primirives' materials.
}

Task* GeometryRenderPass::create_task() {
    return new (m_transient_memory_resource.allocate<Task>()) Task(*this);
}

} // namespace kw