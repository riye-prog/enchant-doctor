#include "renderer.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

void checkVk(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan failure " + std::to_string(result));
}

uint32_t Renderer::memoryType(uint32_t bits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) return i;
    throw std::runtime_error("No compatible Vulkan memory type");
}

Renderer::Buffer Renderer::buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data) {
    Buffer result;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    checkVk(vkCreateBuffer(device, &info, nullptr, &result.buffer));
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    checkVk(vkAllocateMemory(device, &allocation, nullptr, &result.memory));
    checkVk(vkBindBufferMemory(device, result.buffer, result.memory, 0));
    if (data) {
        void* mapped;
        checkVk(vkMapMemory(device, result.memory, 0, size, 0, &mapped));
        std::memcpy(mapped, data, size);
        vkUnmapMemory(device, result.memory);
    }
    return result;
}

void Renderer::freeBuffer(Buffer value) {
    vkDestroyBuffer(device, value.buffer, nullptr);
    vkFreeMemory(device, value.memory, nullptr);
}

VkShaderModule Renderer::shader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Missing shader: " + path);
    auto size = static_cast<size_t>(file.tellg());
    if (size == 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V");
    std::vector<uint32_t> bytes(size / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = size;
    info.pCode = bytes.data();
    VkShaderModule module;
    checkVk(vkCreateShaderModule(device, &info, nullptr, &module));
    return module;
}

Renderer::Renderer(VkDevice device, VkPhysicalDevice physical, VkFormat format, const std::string& assets, VkImageLayout finalLayout)
    : device(device), physical(physical), format(format) {
    vkGetPhysicalDeviceMemoryProperties(physical, &memoryProperties);
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = finalLayout;
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = 1;
    pass.pAttachments = &attachment;
    pass.subpassCount = 1;
    pass.pSubpasses = &subpass;
    pass.dependencyCount = 2;
    pass.pDependencies = dependencies;
    checkVk(vkCreateRenderPass(device, &pass, nullptr, &renderPass));
    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &binding;
    checkVk(vkCreateDescriptorSetLayout(device, &layout, nullptr, &descriptorLayout));
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = 4096;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    checkVk(vkCreateDescriptorPool(device, &pool, nullptr, &descriptorPool));
    VkSamplerCreateInfo sampling{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampling.magFilter = VK_FILTER_LINEAR;
    sampling.minFilter = VK_FILTER_LINEAR;
    sampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampling.addressModeU = sampling.addressModeV = sampling.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    checkVk(vkCreateSampler(device, &sampling, nullptr, &sampler));
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, 80};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;
    checkVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    VkShaderModule vertex = shader(assets + "/ui.vert.spv");
    VkShaderModule fragment = shader(assets + "/ui.frag.spv");
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment;
    stages[1].pName = "main";
    VkVertexInputBindingDescription vertexBinding{0, sizeof(Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position)},
        {1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord)}
    };
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &blending;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
    checkVk(result);
    const Rml::byte pixel[]{255,255,255,255};
    white = reinterpret_cast<Texture*>(GenerateTexture({pixel, 4}, {1, 1}));
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(device);
    for (auto& retirement : retirements) {
        for (auto& release : retirement.releases) release();
        vkDestroyEvent(device, retirement.event, nullptr);
    }
    for (auto& release : pendingReleases) release();
    for (auto* geometry : geometries) freeGeometry(geometry);
    for (auto* texture : textures) freeTexture(texture);
    clearTargets();
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
}

void Renderer::clearTargets() {
    checkVk(vkDeviceWaitIdle(device));
    for (auto& [image, value] : targets) {
        vkDestroyFramebuffer(device, value.framebuffer, nullptr);
        vkDestroyImageView(device, value.view, nullptr);
    }
    targets.clear();
}

Renderer::Target& Renderer::target(VkImage image) {
    auto found = targets.find(image);
    if (found != targets.end()) return found->second;
    Target value;
    value.width = width;
    value.height = height;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    checkVk(vkCreateImageView(device, &view, nullptr, &value.view));
    VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    frame.renderPass = renderPass;
    frame.attachmentCount = 1;
    frame.pAttachments = &value.view;
    frame.width = width;
    frame.height = height;
    frame.layers = 1;
    checkVk(vkCreateFramebuffer(device, &frame, nullptr, &value.framebuffer));
    return targets.emplace(image, value).first->second;
}

void Renderer::begin(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    draws.clear();
    for (auto it = retirements.begin(); it != retirements.end();) {
        auto status = vkGetEventStatus(device, it->event);
        if (status == VK_EVENT_SET) {
            for (auto& release : it->releases) release();
            vkDestroyEvent(device, it->event, nullptr);
            it = retirements.erase(it);
        } else {
            if (status != VK_EVENT_RESET) checkVk(status);
            ++it;
        }
    }
}

Rml::CompiledGeometryHandle Renderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    if (vertices.empty() || indices.empty()) return 0;
    auto* geometry = new Geometry;
    geometry->vertices = buffer(vertices.size() * sizeof(Rml::Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data());
    geometry->indices = buffer(indices.size() * sizeof(int), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data());
    geometry->count = static_cast<uint32_t>(indices.size());
    geometries.push_back(geometry);
    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void Renderer::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) {
    if (!handle) return;
    VkRect2D clip = scissorEnabled ? scissor : VkRect2D{{0,0}, {static_cast<uint32_t>(width),static_cast<uint32_t>(height)}};
    if (clip.extent.width && clip.extent.height)
        draws.push_back({reinterpret_cast<Geometry*>(handle), texture ? reinterpret_cast<Texture*>(texture) : white, translation, transform, clip});
}

void Renderer::freeGeometry(Geometry* geometry) {
    freeBuffer(geometry->vertices);
    freeBuffer(geometry->indices);
    delete geometry;
}

void Renderer::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    if (!handle) return;
    auto* geometry = reinterpret_cast<Geometry*>(handle);
    std::erase(geometries, geometry);
    pendingReleases.push_back([this, geometry] { freeGeometry(geometry); });
}

Rml::TextureHandle Renderer::LoadTexture(Rml::Vector2i&, const Rml::String&) { return 0; }

Rml::TextureHandle Renderer::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) {
    if (dimensions.x <= 0 || dimensions.y <= 0 || source.size() != static_cast<size_t>(dimensions.x) * dimensions.y * 4) return 0;
    auto* texture = new Texture;
    texture->width = dimensions.x;
    texture->height = dimensions.y;
    texture->staging = buffer(source.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, source.data());
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {static_cast<uint32_t>(dimensions.x),static_cast<uint32_t>(dimensions.y),1};
    info.mipLevels = info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    checkVk(vkCreateImage(device, &info, nullptr, &texture->image));
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, texture->image, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(device, &allocation, nullptr, &texture->memory));
    checkVk(vkBindImageMemory(device, texture->image, texture->memory, 0));
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = texture->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = info.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    checkVk(vkCreateImageView(device, &view, nullptr, &texture->view));
    VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptor.descriptorPool = descriptorPool;
    descriptor.descriptorSetCount = 1;
    descriptor.pSetLayouts = &descriptorLayout;
    checkVk(vkAllocateDescriptorSets(device, &descriptor, &texture->set));
    VkDescriptorImageInfo image{sampler, texture->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = texture->set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    textures.push_back(texture);
    return reinterpret_cast<Rml::TextureHandle>(texture);
}

void Renderer::freeTexture(Texture* texture) {
    if (texture->staging.buffer) freeBuffer(texture->staging);
    vkFreeDescriptorSets(device, descriptorPool, 1, &texture->set);
    vkDestroyImageView(device, texture->view, nullptr);
    vkDestroyImage(device, texture->image, nullptr);
    vkFreeMemory(device, texture->memory, nullptr);
    delete texture;
}

void Renderer::ReleaseTexture(Rml::TextureHandle handle) {
    if (!handle) return;
    auto* texture = reinterpret_cast<Texture*>(handle);
    std::erase(textures, texture);
    pendingReleases.push_back([this, texture] { freeTexture(texture); });
}

void Renderer::EnableScissorRegion(bool enable) { scissorEnabled = enable; }
void Renderer::SetScissorRegion(Rml::Rectanglei region) {
    int x = std::clamp(region.Left(), 0, width);
    int y = std::clamp(region.Top(), 0, height);
    int right = std::clamp(region.Right(), x, width);
    int bottom = std::clamp(region.Bottom(), y, height);
    scissor = {{x,y},{static_cast<uint32_t>(right-x),static_cast<uint32_t>(bottom-y)}};
}
void Renderer::SetTransform(const Rml::Matrix4f* matrix) { transform = matrix ? *matrix : Rml::Matrix4f::Identity(); }

void Renderer::record(VkCommandBuffer command, VkImage image, VkImageLayout initialLayout) {
    for (auto* texture : textures) {
        if (texture->uploaded) continue;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.imageExtent = {static_cast<uint32_t>(texture->width),static_cast<uint32_t>(texture->height),1};
        vkCmdCopyBufferToImage(command, texture->staging.buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,nullptr,0,nullptr,1,&barrier);
        texture->uploaded = true;
        auto staging = texture->staging;
        texture->staging = {};
        pendingReleases.push_back([this, staging] { freeBuffer(staging); });
    }
    if (!draws.empty()) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = initialLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,nullptr,0,nullptr,1,&barrier);
        VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass.renderPass = renderPass;
        pass.framebuffer = target(image).framebuffer;
        pass.renderArea.extent = {static_cast<uint32_t>(width),static_cast<uint32_t>(height)};
        vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkViewport viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
        vkCmdSetViewport(command, 0, 1, &viewport);
        for (auto& draw : draws) {
            vkCmdSetScissor(command, 0, 1, &draw.scissor);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command, 0, 1, &draw.geometry->vertices.buffer, &offset);
            vkCmdBindIndexBuffer(command, draw.geometry->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &draw.texture->set, 0, nullptr);
            struct Push { float matrix[16]; float translation[2]; float extent[2]; } push{};
            std::memcpy(push.matrix, draw.transform.data(), 64);
            push.translation[0] = draw.translation.x;
            push.translation[1] = draw.translation.y;
            push.extent[0] = static_cast<float>(width);
            push.extent[1] = static_cast<float>(height);
            vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
            vkCmdDrawIndexed(command, draw.geometry->count, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(command);
    }
    if (!pendingReleases.empty()) {
        VkEventCreateInfo info{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
        VkEvent event;
        checkVk(vkCreateEvent(device, &info, nullptr, &event));
        vkCmdSetEvent(command, event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        retirements.push_back({event, std::move(pendingReleases)});
        pendingReleases.clear();
    }
}
