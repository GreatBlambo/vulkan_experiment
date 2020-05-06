#pragma once

#include "vulkan_app.h"

namespace Vulkan {
void render_frame(Vulkan::App& app, const std::function< void(const size_t, VkCommandBuffer) >& render);
}
