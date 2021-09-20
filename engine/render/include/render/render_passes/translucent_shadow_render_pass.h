#pragma once

#include "render/render_passes/base_render_pass.h"

#include <core/containers/pair.h>

namespace kw {

class RenderScene;
class ShadowManager;
class TaskScheduler;

struct TranslucentShadowRenderPassDescriptor {
    RenderScene* scene;
    ShadowManager* shadow_manager;
    TaskScheduler* task_scheduler;
    MemoryResource* transient_memory_resource;
};

class TranslucentShadowRenderPass : public BaseRenderPass {
public:
    explicit TranslucentShadowRenderPass(const TranslucentShadowRenderPassDescriptor& descriptor);

    void get_color_attachment_descriptors(Vector<AttachmentDescriptor>& attachment_descriptors) override;
    void get_depth_stencil_attachment_descriptors(Vector<AttachmentDescriptor>& attachment_descriptors) override;
    void get_render_pass_descriptors(Vector<RenderPassDescriptor>& render_pass_descriptors) override;
    void create_graphics_pipelines(FrameGraph& frame_graph) override;
    void destroy_graphics_pipelines(FrameGraph& frame_graph) override;

    // Must be placed between acquire and present frame graph's tasks.
    Pair<Task*, Task*> create_tasks();

private:
    class BeginTask;
    class WorkerTask;

    RenderScene& m_scene;
    ShadowManager& m_shadow_manager;
    TaskScheduler& m_task_scheduler;
    MemoryResource& m_transient_memory_resource;
};

} // namespace kw
