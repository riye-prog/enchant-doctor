#include "renderer.hpp"
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

void checkVk(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan failure " + std::to_string(result));
}

Renderer::Buffer Renderer::buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data) {
    Buffer result;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO;
    allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo properties{};
    checkVk(vmaCreateBuffer(allocator, &info, &allocation, &result.buffer, &result.allocation, &properties));
    if (data) {
        std::memcpy(properties.pMappedData, data, static_cast<size_t>(size));
        checkVk(vmaFlushAllocation(allocator, result.allocation, 0, size));
    }
    return result;
}

void Renderer::freeBuffer(Buffer value) {
    if (value.buffer) vmaDestroyBuffer(allocator, value.buffer, value.allocation);
}

VkShaderModule Renderer::shader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Missing shader: " + path);
    auto size = static_cast<size_t>(file.tellg());
    if (size == 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V");
    std::vector<uint32_t> bytes(size / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("Unable to read shader: " + path);
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = size;
    info.pCode = bytes.data();
    VkShaderModule module;
    checkVk(vkCreateShaderModule(device, &info, nullptr, &module));
    return module;
}

VkRenderPass Renderer::renderPass(VkFormat colorFormat, bool stencil, bool multisample, VkImageLayout lastLayout) {
    std::array<VkAttachmentDescription, 3> attachments{};
    auto& color = attachments[0];
    color.format = colorFormat;
    color.samples = multisample ? samples : VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = multisample ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : lastLayout;
    uint32_t count = 1;
    VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolveReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference stencilReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    if (multisample) {
        resolveReference.attachment = count;
        attachments[count] = color;
        attachments[count].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[count].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[count++].finalLayout = lastLayout;
    }
    if (stencil) {
        stencilReference.attachment = count;
        auto& depth = attachments[count++];
        depth.format = stencilFormat;
        depth.samples = samples;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.initialLayout = depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pResolveAttachments = multisample ? &resolveReference : nullptr;
    subpass.pDepthStencilAttachment = stencil ? &stencilReference : nullptr;
    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (stencil) dependencies[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (stencil) dependencies[1].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = count;
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = dependencies.size();
    info.pDependencies = dependencies.data();
    VkRenderPass result;
    checkVk(vkCreateRenderPass(device, &info, nullptr, &result));
    return result;
}

VkPipeline Renderer::pipeline(VkRenderPass pass, VkShaderModule vertex, VkShaderModule fragment, VkSampleCountFlagBits sampleCount, int stencilMode, bool blendEnabled) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment;
    stages[1].pName = "main";
    VkVertexInputBindingDescription binding{0, sizeof(Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position)},
        {1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord)}
    };
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = 3;
    input.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = sampleCount;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.stencilTestEnable = stencilMode == 1 || stencilMode == 2 || stencilMode == 3 || stencilMode == 5;
    depth.front.compareOp = stencilMode == 2 ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_EQUAL;
    depth.front.passOp = stencilMode == 2 ? VK_STENCIL_OP_REPLACE : stencilMode == 3 ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_KEEP;
    depth.front.failOp = depth.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depth.front.compareMask = 255;
    depth.front.writeMask = stencilMode == 2 || stencilMode == 3 ? 255 : 0;
    depth.back = depth.front;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = blendEnabled;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = stencilMode == 2 || stencilMode == 3 ? 0 : 15;
    VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 3;
    dynamic.pDynamicStates = states;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blending;
    info.pDynamicState = &dynamic;
    info.layout = pipelineLayout;
    info.renderPass = pass;
    VkPipeline result;
    checkVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &result));
    return result;
}

Renderer::Renderer(VkInstance instance, VkDevice device, VkPhysicalDevice physical, VkFormat format, const std::string& assets, VkImageLayout finalLayout)
    : device(device), physical(physical), format(format), finalLayout(finalLayout) {
    static_assert(sizeof(Push) == 128);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    auto supportedSamples = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferStencilSampleCounts & properties.limits.framebufferDepthSampleCounts;
    if (supportedSamples & VK_SAMPLE_COUNT_4_BIT) samples = VK_SAMPLE_COUNT_4_BIT;
    else if (supportedSamples & VK_SAMPLE_COUNT_2_BIT) samples = VK_SAMPLE_COUNT_2_BIT;
    for (auto candidate : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        VkFormatProperties support{};
        vkGetPhysicalDeviceFormatProperties(physical, candidate, &support);
        if (support.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { stencilFormat = candidate; break; }
    }
    if (!stencilFormat) throw std::runtime_error("No supported stencil attachment format");
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo allocation{};
    allocation.instance = instance;
    allocation.device = device;
    allocation.physicalDevice = physical;
    allocation.vulkanApiVersion = VK_API_VERSION_1_1;
    allocation.pVulkanFunctions = &functions;
    checkVk(vmaCreateAllocator(&allocation, &allocator));
    outputSrgb = format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    layerPass = renderPass(VK_FORMAT_R8G8B8A8_UNORM, true, samples != VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    effectPass = renderPass(VK_FORMAT_R8G8B8A8_UNORM, false, false, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    outputPass = renderPass(format, false, false, finalLayout);
    VkDescriptorSetLayoutBinding textureBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &textureBinding;
    checkVk(vkCreateDescriptorSetLayout(device, &layout, nullptr, &textureLayout));
    VkDescriptorSetLayoutBinding effectBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    layout.pBindings = &effectBinding;
    checkVk(vkCreateDescriptorSetLayout(device, &layout, nullptr, &effectLayout));
    VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8192}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8192}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = 16384;
    pool.poolSizeCount = 2;
    pool.pPoolSizes = sizes;
    checkVk(vkCreateDescriptorPool(device, &pool, nullptr, &descriptorPool));
    VkSamplerCreateInfo sampling{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampling.magFilter = sampling.minFilter = VK_FILTER_LINEAR;
    sampling.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampling.addressModeU = sampling.addressModeV = sampling.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    checkVk(vkCreateSampler(device, &sampling, nullptr, &sampler));
    VkDescriptorSetLayout layouts[]{textureLayout, textureLayout, effectLayout};
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)};
    VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 3;
    pipelineInfo.pSetLayouts = layouts;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &range;
    checkVk(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &pipelineLayout));
    auto vertex = shader(assets + "/ui.vert.spv");
    auto fragment = shader(assets + "/ui.frag.spv");
    for (int i = 0; i < 6; ++i) layerPipelines[i] = pipeline(layerPass, vertex, fragment, samples, i, i < 2);
    effectPipeline = pipeline(effectPass, vertex, fragment, VK_SAMPLE_COUNT_1_BIT, 0, false);
    outputPipeline = pipeline(outputPass, vertex, fragment, VK_SAMPLE_COUNT_1_BIT, 0, true);
    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
    const Rml::byte pixel[]{255,255,255,255};
    white = reinterpret_cast<Texture*>(GenerateTexture({pixel, 4}, {1, 1}));
    identity = effect(0, Rml::Matrix4f::Identity());
    Rml::Mesh mesh;
    Rml::MeshUtilities::GenerateQuad(mesh, {0,0}, {1,1}, {255,255,255,255});
    quad = reinterpret_cast<Geometry*>(CompileGeometry(mesh.vertices, mesh.indices));
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(device);
    for (auto& retirement : retirements) {
        for (auto& release : retirement.releases) release();
    }
    for (auto& release : pendingReleases) release();
    clearTargets();
    while (!filters.empty()) freeFilter(filters.back());
    while (!effects.empty()) freeEffect(effects.back());
    while (!geometries.empty()) freeGeometry(geometries.back());
    while (!textures.empty()) freeTexture(textures.back());
    for (auto value : layerPipelines) vkDestroyPipeline(device, value, nullptr);
    vkDestroyPipeline(device, effectPipeline, nullptr);
    vkDestroyPipeline(device, outputPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, textureLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, effectLayout, nullptr);
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyRenderPass(device, layerPass, nullptr);
    vkDestroyRenderPass(device, effectPass, nullptr);
    vkDestroyRenderPass(device, outputPass, nullptr);
    vmaDestroyAllocator(allocator);
}

Renderer::Texture* Renderer::texture(int textureWidth, int textureHeight, VkFormat textureFormat, VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount, VkImageAspectFlags aspect) {
    auto* result = new Texture;
    result->width = textureWidth;
    result->height = textureHeight;
    result->aspect = aspect;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = textureFormat;
    info.extent = {static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), 1};
    info.mipLevels = info.arrayLayers = 1;
    info.samples = sampleCount;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    checkVk(vmaCreateImage(allocator, &info, &allocation, &result->image, &result->allocation, nullptr));
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = result->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = textureFormat;
    view.subresourceRange = {aspect,0,1,0,1};
    checkVk(vkCreateImageView(device, &view, nullptr, &result->view));
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        descriptor.descriptorPool = descriptorPool;
        descriptor.descriptorSetCount = 1;
        descriptor.pSetLayouts = &textureLayout;
        checkVk(vkAllocateDescriptorSets(device, &descriptor, &result->set));
        VkDescriptorImageInfo image{sampler, result->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = result->set;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
    textures.push_back(result);
    return result;
}

void Renderer::freeTexture(Texture* value) {
    if (!value) return;
    std::erase(textures, value);
    freeBuffer(value->staging);
    if (value->set) vkFreeDescriptorSets(device, descriptorPool, 1, &value->set);
    vkDestroyImageView(device, value->view, nullptr);
    vmaDestroyImage(allocator, value->image, value->allocation);
    delete value;
}

Renderer::Surface Renderer::surface(bool withStencil) {
    Surface result;
    result.texture = texture(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    std::vector<VkImageView> views;
    if (withStencil && samples != VK_SAMPLE_COUNT_1_BIT) {
        result.multisample = texture(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, samples);
        views.push_back(result.multisample->view);
    }
    views.push_back(result.texture->view);
    if (withStencil) views.push_back(stencilTexture->view);
    VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    frame.renderPass = withStencil ? layerPass : effectPass;
    frame.attachmentCount = views.size();
    frame.pAttachments = views.data();
    frame.width = width;
    frame.height = height;
    frame.layers = 1;
    checkVk(vkCreateFramebuffer(device, &frame, nullptr, &result.framebuffer));
    return result;
}

void Renderer::freeSurface(Surface& value) {
    if (value.framebuffer) vkDestroyFramebuffer(device, value.framebuffer, nullptr);
    freeTexture(value.multisample);
    freeTexture(value.texture);
    value = {};
}

void Renderer::clearTargets() {
    checkVk(vkDeviceWaitIdle(device));
    for (auto& [image, value] : targets) {
        vkDestroyFramebuffer(device, value.framebuffer, nullptr);
        vkDestroyImageView(device, value.view, nullptr);
    }
    targets.clear();
    for (auto& value : layers) freeSurface(value);
    layers.clear();
    for (auto& value : scratch) freeSurface(value);
    freeTexture(stencilTexture);
    stencilTexture = nullptr;
    surfaceWidth = surfaceHeight = 0;
    activeSurface = nullptr;
}

void Renderer::prepareSurfaces() {
    if (surfaceWidth != width || surfaceHeight != height) {
        if (stencilTexture) clearTargets();
        stencilTexture = texture(width, height, stencilFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, samples, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        surfaceWidth = width;
        surfaceHeight = height;
    }
    while (static_cast<int>(layers.size()) <= maximumLayer) layers.push_back(surface(true));
}

Renderer::Target& Renderer::target(VkImage image) {
    auto found = targets.find(image);
    if (found != targets.end()) return found->second;
    Target result;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    checkVk(vkCreateImageView(device, &view, nullptr, &result.view));
    VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    frame.renderPass = outputPass;
    frame.attachmentCount = 1;
    frame.pAttachments = &result.view;
    frame.width = width;
    frame.height = height;
    frame.layers = 1;
    checkVk(vkCreateFramebuffer(device, &frame, nullptr, &result.framebuffer));
    return targets.emplace(image, result).first->second;
}

void Renderer::transition(Texture* value, VkImageLayout layout) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = value->layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = value->layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = value->image;
    barrier.subresourceRange = {value->aspect,0,1,0,1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,0,nullptr,0,nullptr,1,&barrier);
    value->layout = layout;
}

void Renderer::endPass() {
    if (!passActive) return;
    vkCmdEndRenderPass(command);
    passActive = false;
    if (activeSurface) transition(activeSurface->texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    activeSurface = nullptr;
}

void Renderer::bindSurface(Surface& value, bool clear) {
    if (passActive && activeSurface == &value && !clear) return;
    endPass();
    bool withStencil = std::any_of(layers.begin(), layers.end(), [&](auto& layer) { return &layer == &value; });
    transition(value.texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (value.multisample) transition(value.multisample, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (withStencil) transition(stencilTexture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = withStencil ? layerPass : effectPass;
    pass.framebuffer = value.framebuffer;
    pass.renderArea.extent = {static_cast<uint32_t>(width),static_cast<uint32_t>(height)};
    vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
    passActive = true;
    activeSurface = &value;
    VkViewport viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    vkCmdSetViewport(command,0,1,&viewport);
    if (clear || !value.initialized) {
        VkClearAttachment attachment{};
        attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearRect rect{fullScissor(),0,1};
        vkCmdClearAttachments(command,1,&attachment,1,&rect);
        value.initialized = true;
    }
}

VkRect2D Renderer::fullScissor() const { return {{0,0},{static_cast<uint32_t>(width),static_cast<uint32_t>(height)}}; }
VkRect2D Renderer::currentScissor() const { return scissorEnabled ? scissor : fullScissor(); }

void Renderer::begin(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    operations.clear();
    transform = Rml::Matrix4f::Identity();
    scissor = fullScissor();
    scissorEnabled = clipEnabled = false;
    stencilReference = activeLayer = maximumLayer = 0;

}

Renderer::Operation Renderer::operation(Operation::Type type) const {
    Operation result;
    result.type = type;
    result.transform = transform;
    result.scissor = currentScissor();
    result.layer = activeLayer;
    result.stencil = stencilReference;
    result.clip = clipEnabled;
    return result;
}

Rml::CompiledGeometryHandle Renderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    if (vertices.empty() || indices.empty()) return 0;
    auto* result = new Geometry;
    result->vertices = buffer(vertices.size() * sizeof(Rml::Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data());
    result->indices = buffer(indices.size() * sizeof(int), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data());
    result->count = indices.size();
    geometries.push_back(result);
    return reinterpret_cast<Rml::CompiledGeometryHandle>(result);
}

void Renderer::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle textureHandle) {
    if (!handle) return;
    auto result = operation(Operation::Type::Draw);
    result.geometry = reinterpret_cast<Geometry*>(handle);
    result.texture = textureHandle ? reinterpret_cast<Texture*>(textureHandle) : white;
    result.translation = translation;
    if (result.scissor.extent.width && result.scissor.extent.height) operations.push_back(std::move(result));
}

void Renderer::freeGeometry(Geometry* value) {
    std::erase(geometries, value);
    freeBuffer(value->vertices);
    freeBuffer(value->indices);
    delete value;
}

void Renderer::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    if (handle) pendingReleases.push_back([this, value = reinterpret_cast<Geometry*>(handle)] { freeGeometry(value); });
}

Rml::TextureHandle Renderer::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) {
    auto* files = Rml::GetFileInterface();
    auto file = files->Open(source);
    if (!file) return 0;
    auto size = files->Length(file);
    if (!size || size > 64 * 1024 * 1024) { files->Close(file); return 0; }
    std::vector<Rml::byte> bytes(size);
    auto read = files->Read(bytes.data(), size, file);
    files->Close(file);
    if (read != size) return 0;
    int channels;
    if (!stbi_info_from_memory(bytes.data(),static_cast<int>(size),&dimensions.x,&dimensions.y,&channels)) return 0;
    VkPhysicalDeviceProperties limits{};
    vkGetPhysicalDeviceProperties(physical,&limits);
    if (dimensions.x <= 0 || dimensions.y <= 0 || static_cast<uint32_t>(dimensions.x) > limits.limits.maxImageDimension2D || static_cast<uint32_t>(dimensions.y) > limits.limits.maxImageDimension2D) return 0;
    auto* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(size), &dimensions.x, &dimensions.y, &channels, 4);
    if (!pixels) { Rml::Log::Message(Rml::Log::LT_WARNING, "Unable to decode image '%s': %s", source.c_str(), stbi_failure_reason()); return 0; }
    size_t count = static_cast<size_t>(dimensions.x) * dimensions.y;
    for (size_t i = 0; i < count; ++i)
        for (size_t c = 0; c < 3; ++c) pixels[i * 4 + c] = (static_cast<unsigned>(pixels[i * 4 + c]) * pixels[i * 4 + 3] + 127) / 255;
    auto result = GenerateTexture({pixels, count * 4}, dimensions);
    stbi_image_free(pixels);
    return result;
}

Rml::TextureHandle Renderer::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) {
    if (dimensions.x <= 0 || dimensions.y <= 0 || source.size() != static_cast<size_t>(dimensions.x) * dimensions.y * 4) return 0;
    auto* result = texture(dimensions.x, dimensions.y, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    result->staging = buffer(source.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, source.data());
    return reinterpret_cast<Rml::TextureHandle>(result);
}

void Renderer::ReleaseTexture(Rml::TextureHandle handle) {
    if (handle) pendingReleases.push_back([this, value = reinterpret_cast<Texture*>(handle)] { freeTexture(value); });
}

void Renderer::EnableScissorRegion(bool enable) { scissorEnabled = enable; }
void Renderer::SetScissorRegion(Rml::Rectanglei region) {
    int x = std::clamp(region.Left(),0,width);
    int y = std::clamp(region.Top(),0,height);
    int right = std::clamp(region.Right(),x,width);
    int bottom = std::clamp(region.Bottom(),y,height);
    scissor = {{x,y},{static_cast<uint32_t>(right-x),static_cast<uint32_t>(bottom-y)}};
}
void Renderer::SetTransform(const Rml::Matrix4f* value) { transform = value ? *value : Rml::Matrix4f::Identity(); }
void Renderer::EnableClipMask(bool enable) { clipEnabled = enable; }

void Renderer::RenderToClipMask(Rml::ClipMaskOperation clipOperation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {
    stencilReference = clipOperation == Rml::ClipMaskOperation::Intersect ? stencilReference + 1 : 1;
    if (stencilReference > 255) throw std::runtime_error("Clip mask nesting exceeds stencil capacity");
    auto result = operation(Operation::Type::Clip);
    result.clipOperation = clipOperation;
    result.geometry = reinterpret_cast<Geometry*>(geometry);
    result.translation = translation;
    operations.push_back(std::move(result));
}

Rml::LayerHandle Renderer::PushLayer() {
    ++activeLayer;
    maximumLayer = std::max(maximumLayer, activeLayer);
    operations.push_back(operation(Operation::Type::Clear));
    return static_cast<Rml::LayerHandle>(activeLayer);
}
void Renderer::PopLayer() { if (activeLayer > 0) --activeLayer; }
void Renderer::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend, Rml::Span<const Rml::CompiledFilterHandle> filterHandles) {
    auto result = operation(Operation::Type::Composite);
    result.source = static_cast<int>(source);
    result.layer = static_cast<int>(destination);
    result.blend = blend;
    for (auto handle : filterHandles) result.filters.push_back(reinterpret_cast<Filter*>(handle));
    operations.push_back(std::move(result));
}
Rml::TextureHandle Renderer::SaveLayerAsTexture() {
    auto bounds = currentScissor();
    if (!bounds.extent.width || !bounds.extent.height) return 0;
    auto* saved = texture(bounds.extent.width,bounds.extent.height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    auto result = operation(Operation::Type::Save);
    result.texture = saved;
    operations.push_back(std::move(result));
    return reinterpret_cast<Rml::TextureHandle>(saved);
}
Rml::CompiledFilterHandle Renderer::SaveLayerAsMaskImage() {
    auto* saved = texture(width,height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    auto result = operation(Operation::Type::Save);
    result.texture = saved;
    result.source = 1;
    operations.push_back(std::move(result));
    auto* filter = new Filter;
    filter->type = Filter::Type::Mask;
    filter->mask = saved;
    filters.push_back(filter);
    return reinterpret_cast<Rml::CompiledFilterHandle>(filter);
}

Renderer::Effect* Renderer::effect(int mode, const Rml::Matrix4f& matrix, Rml::Vector4f data0, Rml::Vector4f data1, const Rml::ColorStopList* stops, bool repeating) {
    auto* result = new Effect;
    result->mode = mode;
    result->repeating = repeating;
    result->count = stops ? stops->size() : 0;
    std::vector<float> values(24 + result->count * 8);
    std::memcpy(values.data(), matrix.data(), 64);
    std::memcpy(values.data() + 16, &data0, 16);
    std::memcpy(values.data() + 20, &data1, 16);
    for (int i = 0; i < result->count; ++i) {
        const auto& stop = (*stops)[i];
        values[24+i*8] = stop.position.number;
        values[28+i*8] = stop.color.red / 255.f;
        values[29+i*8] = stop.color.green / 255.f;
        values[30+i*8] = stop.color.blue / 255.f;
        values[31+i*8] = stop.color.alpha / 255.f;
    }
    result->buffer = buffer(values.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, values.data());
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = descriptorPool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &effectLayout;
    checkVk(vkAllocateDescriptorSets(device, &allocation, &result->set));
    VkDescriptorBufferInfo descriptor{result->buffer.buffer,0,VK_WHOLE_SIZE};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = result->set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &descriptor;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    effects.push_back(result);
    return result;
}

void Renderer::freeEffect(Effect* value) {
    if (!value) return;
    std::erase(effects,value);
    vkFreeDescriptorSets(device,descriptorPool,1,&value->set);
    freeBuffer(value->buffer);
    delete value;
}
void Renderer::freeFilter(Filter* value) {
    std::erase(filters,value);
    freeEffect(value->effect);
    freeTexture(value->mask);
    delete value;
}
Rml::CompiledFilterHandle Renderer::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
    auto* result = new Filter;
    float value = Rml::Get(parameters,"value",1.f);
    auto matrix = Rml::Matrix4f::Identity();
    if (name == "opacity") {
        result->type = Filter::Type::Opacity;
        result->value = std::clamp(value,0.f,1.f);
    } else if (name == "blur" || name == "drop-shadow") {
        result->type = name == "blur" ? Filter::Type::Blur : Filter::Type::Shadow;
        result->value = std::max(0.f,Rml::Get(parameters,"sigma",0.f));
        result->offset = Rml::Get(parameters,"offset",Rml::Vector2f(0.f));
        auto color = Rml::Get(parameters,"color",Rml::Colourb()).ToPremultiplied();
        result->color = {color.red/255.f,color.green/255.f,color.blue/255.f,color.alpha/255.f};
    } else if (name == "brightness") {
        matrix = Rml::Matrix4f::Diag(value,value,value,1.f);
    } else if (name == "contrast") {
        float gray = 0.5f - 0.5f * value;
        matrix = Rml::Matrix4f::Diag(value,value,value,1.f);
        matrix.SetColumn(3,{gray,gray,gray,1.f});
    } else if (name == "invert") {
        value = std::clamp(value,0.f,1.f);
        matrix = Rml::Matrix4f::Diag(1.f-2.f*value,1.f-2.f*value,1.f-2.f*value,1.f);
        matrix.SetColumn(3,{value,value,value,1.f});
    } else if (name == "grayscale") {
        value = std::clamp(value,0.f,1.f);
        float r = 0.2126f*value, g = 0.7152f*value, b = 0.0722f*value, remain = 1.f-value;
        matrix = Rml::Matrix4f::FromRows({r+remain,g,b,0},{r,g+remain,b,0},{r,g,b+remain,0},{0,0,0,1});
    } else if (name == "sepia") {
        value = std::clamp(value,0.f,1.f);
        float remain = 1.f-value;
        matrix = Rml::Matrix4f::FromRows({0.393f*value+remain,0.769f*value,0.189f*value,0},{0.349f*value,0.686f*value+remain,0.168f*value,0},{0.272f*value,0.534f*value,0.131f*value+remain,0},{0,0,0,1});
    } else if (name == "saturate") {
        matrix = Rml::Matrix4f::FromRows({0.213f+0.787f*value,0.715f-0.715f*value,0.072f-0.072f*value,0},{0.213f-0.213f*value,0.715f+0.285f*value,0.072f-0.072f*value,0},{0.213f-0.213f*value,0.715f-0.715f*value,0.072f+0.928f*value,0},{0,0,0,1});
    } else if (name == "hue-rotate") {
        float c = std::cos(value), s = std::sin(value);
        matrix = Rml::Matrix4f::FromRows({0.213f+0.787f*c-0.213f*s,0.715f-0.715f*c-0.715f*s,0.072f-0.072f*c+0.928f*s,0},{0.213f-0.213f*c+0.143f*s,0.715f+0.285f*c+0.140f*s,0.072f-0.072f*c-0.283f*s,0},{0.213f-0.213f*c-0.787f*s,0.715f-0.715f*c+0.715f*s,0.072f+0.928f*c+0.072f*s,0},{0,0,0,1});
    } else {
        delete result;
        Rml::Log::Message(Rml::Log::LT_WARNING,"Unsupported filter '%s'",name.c_str());
        return 0;
    }
    if (result->type == Filter::Type::Matrix) result->effect = effect(3,matrix);
    filters.push_back(result);
    return reinterpret_cast<Rml::CompiledFilterHandle>(result);
}
void Renderer::ReleaseFilter(Rml::CompiledFilterHandle handle) {
    if (handle) pendingReleases.push_back([this,value=reinterpret_cast<Filter*>(handle)] { freeFilter(value); });
}
Rml::CompiledShaderHandle Renderer::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) {
    int mode = name == "linear-gradient" ? 10 : name == "radial-gradient" ? 11 : name == "conic-gradient" ? 12 : 0;
    auto stops = parameters.find("color_stop_list");
    if (!mode || stops == parameters.end()) {
        Rml::Log::Message(Rml::Log::LT_WARNING,"Unsupported shader '%s'",name.c_str());
        return 0;
    }
    Rml::Vector2f position{}, direction{};
    if (mode == 10) {
        position = Rml::Get(parameters,"p0",Rml::Vector2f(0.f));
        direction = Rml::Get(parameters,"p1",Rml::Vector2f(0.f)) - position;
    } else if (mode == 11) {
        position = Rml::Get(parameters,"center",Rml::Vector2f(0.f));
        auto radius = Rml::Get(parameters,"radius",Rml::Vector2f(1.f));
        direction = {1.f/std::max(radius.x,0.0001f),1.f/std::max(radius.y,0.0001f)};
    } else {
        position = Rml::Get(parameters,"center",Rml::Vector2f(0.f));
        float angle = Rml::Get(parameters,"angle",0.f);
        direction = {std::cos(angle),std::sin(angle)};
    }
    auto& colors = stops->second.GetReference<Rml::ColorStopList>();
    return reinterpret_cast<Rml::CompiledShaderHandle>(effect(mode,Rml::Matrix4f::Identity(),{position.x,position.y,direction.x,direction.y},{},&colors,Rml::Get(parameters,"repeating",false)));
}
void Renderer::RenderShader(Rml::CompiledShaderHandle handle, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    if (!handle || !geometry) return;
    const auto previousSize = operations.size();
    RenderGeometry(geometry,translation,texture);
    if (operations.size() > previousSize && operations.back().type == Operation::Type::Draw && operations.back().geometry == reinterpret_cast<Geometry*>(geometry))
        operations.back().effect = reinterpret_cast<Effect*>(handle);
}
void Renderer::ReleaseShader(Rml::CompiledShaderHandle handle) {
    if (handle) pendingReleases.push_back([this,value=reinterpret_cast<Effect*>(handle)] { freeEffect(value); });
}

void Renderer::upload(Texture* value) {
    if (!value->staging.buffer) return;
    transition(value,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageExtent = {static_cast<uint32_t>(value->width),static_cast<uint32_t>(value->height),1};
    vkCmdCopyBufferToImage(command,value->staging.buffer,value->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    transition(value,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto staging = value->staging;
    value->staging = {};
    pendingReleases.push_back([this,staging] { freeBuffer(staging); });
}
Renderer::Push Renderer::push(Effect* value) const {
    Push result{};
    auto matrix = Rml::Matrix4f::Identity();
    std::memcpy(result.matrix,matrix.data(),64);
    result.extent[0] = static_cast<float>(width);
    result.extent[1] = static_cast<float>(height);
    if (value) {
        result.config[0] = value->mode;
        result.config[1] = value->count;
        result.config[2] = value->repeating;
    }
    return result;
}
void Renderer::draw(Geometry* geometry, Texture* image, Texture* auxiliary, Effect* effect, Push constants, VkPipeline pipeline, VkRect2D bounds, int stencil) {
    if (!bounds.extent.width || !bounds.extent.height) return;
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    vkCmdSetScissor(command,0,1,&bounds);
    vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,stencil);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command,0,1,&geometry->vertices.buffer,&offset);
    vkCmdBindIndexBuffer(command,geometry->indices.buffer,0,VK_INDEX_TYPE_UINT32);
    VkDescriptorSet sets[]{image ? image->set : white->set,auxiliary ? auxiliary->set : white->set,effect ? effect->set : identity->set};
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,3,sets,0,nullptr);
    vkCmdPushConstants(command,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(constants),&constants);
    vkCmdDrawIndexed(command,geometry->count,1,0,0,0);
}
void Renderer::blit(Texture* input, Surface& destination, int mode, VkRect2D bounds, Effect* effect, Texture* auxiliary, Rml::Vector4f data, Rml::Colourf color) {
    endPass();
    transition(input,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (auxiliary) transition(auxiliary,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    bindSurface(destination,true);
    auto constants = push(effect);
    constants.matrix[0] = static_cast<float>(width);
    constants.matrix[5] = static_cast<float>(height);
    constants.config[0] = mode;
    std::memcpy(constants.data,&data,16);
    constants.color[0] = color.red;
    constants.color[1] = color.green;
    constants.color[2] = color.blue;
    constants.color[3] = color.alpha;
    draw(quad,input,auxiliary,effect,constants,effectPipeline,bounds,0);
    endPass();
}
Renderer::Surface& Renderer::temporary(Texture* avoidA, Texture* avoidB) {
    for (auto& value : scratch) {
        if (value.texture && (value.texture == avoidA || value.texture == avoidB)) continue;
        if (!value.texture) value = surface(false);
        return value;
    }
    throw std::runtime_error("No available postprocessing surface");
}
Renderer::Texture* Renderer::applyFilters(Texture* input, const std::vector<Filter*>& chain, VkRect2D bounds) {
    for (auto* filter : chain) {
        if (!filter) continue;
        if (filter->type == Filter::Type::Blur || filter->type == Filter::Type::Shadow) {
            auto* original = input;
            if (filter->type == Filter::Type::Shadow) {
                auto& output = temporary(input);
                blit(input,output,4,fullScissor(),nullptr,nullptr,{filter->offset.x,filter->offset.y,0,0},filter->color);
                input = output.texture;
            }
            if (filter->value > 0.01f) {
                auto& horizontal = temporary(input,filter->type == Filter::Type::Shadow ? original : nullptr);
                blit(input,horizontal,2,fullScissor(),nullptr,nullptr,{1,0,filter->value,0});
                auto& vertical = temporary(horizontal.texture,filter->type == Filter::Type::Shadow ? original : nullptr);
                blit(horizontal.texture,vertical,2,fullScissor(),nullptr,nullptr,{0,1,filter->value,0});
                input = vertical.texture;
            }
            if (filter->type == Filter::Type::Shadow) {
                auto& output = temporary(input,original);
                blit(input,output,6,bounds,nullptr,original);
                input = output.texture;
            }
        } else {
            auto& output = temporary(input);
            int mode = filter->type == Filter::Type::Opacity ? 1 : filter->type == Filter::Type::Mask ? 5 : 3;
            blit(input,output,mode,bounds,filter->effect,filter->mask,{filter->value,0,0,0});
            input = output.texture;
        }
    }
    return input;
}

void Renderer::retire() {
    if (pendingReleases.empty()) return;
    static uint64_t nextId = 1;
    lastRetirement = nextId++;
    retirements.push_back({lastRetirement,std::move(pendingReleases)});
    pendingReleases.clear();
}
uint64_t Renderer::takeRetirement() {
    return std::exchange(lastRetirement,0);
}
void Renderer::releaseRetirement(uint64_t id) {
    auto found = std::find_if(retirements.begin(),retirements.end(),[id](const auto& value) { return value.id == id; });
    if (found == retirements.end()) return;
    for (auto& release : found->releases) release();
    retirements.erase(found);
}
void Renderer::record(VkCommandBuffer frameCommand, VkImage image, VkImageLayout initialLayout) {
    command = frameCommand;
    for (auto* value : textures) upload(value);
    if (operations.empty()) { retire(); command = nullptr; return; }
    prepareSurfaces();
    bindSurface(layers[0],true);
    VkClearAttachment stencilClear{};
    stencilClear.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    VkClearRect clearRect{fullScissor(),0,1};
    vkCmdClearAttachments(command,1,&stencilClear,1,&clearRect);
    for (auto& op : operations) {
        if (op.type == Operation::Type::Draw || op.type == Operation::Type::Clip) {
            bindSurface(layers[op.layer]);
            auto constants = push(op.effect);
            std::memcpy(constants.matrix,op.transform.data(),64);
            constants.translation[0] = op.translation.x;
            constants.translation[1] = op.translation.y;
            int pipelineIndex = op.clip ? 1 : 0;
            int reference = op.stencil;
            if (op.type == Operation::Type::Clip) {
                if (op.clipOperation != Rml::ClipMaskOperation::Intersect) {
                    stencilClear.clearValue.depthStencil.stencil = op.clipOperation == Rml::ClipMaskOperation::SetInverse ? 1 : 0;
                    vkCmdClearAttachments(command,1,&stencilClear,1,&clearRect);
                    pipelineIndex = 2;
                    reference = op.clipOperation == Rml::ClipMaskOperation::SetInverse ? 0 : 1;
                } else { pipelineIndex = 3; reference = op.stencil-1; }
            }
            draw(op.geometry,op.texture,nullptr,op.effect,constants,layerPipelines[pipelineIndex],op.scissor,reference);
        } else if (op.type == Operation::Type::Clear) {
            bindSurface(layers[op.layer],true);
        } else if (op.type == Operation::Type::Save) {
            endPass();
            auto* source = layers[op.layer].texture;
            transition(source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            transition(op.texture,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkClearColorValue transparent{};
            VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            vkCmdClearColorImage(command,op.texture->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&transparent,1,&range);
            transition(op.texture,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkImageCopy copy{};
            copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            copy.srcOffset = {op.scissor.offset.x,op.scissor.offset.y,0};
            if (op.source == 1) copy.dstOffset = copy.srcOffset;
            copy.extent = {op.scissor.extent.width,op.scissor.extent.height,1};
            if (copy.extent.width && copy.extent.height) vkCmdCopyImage(command,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,op.texture->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            transition(source,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            transition(op.texture,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (op.type == Operation::Type::Composite) {
            endPass();
            auto* source = layers[op.source].texture;
            transition(source,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            auto* filtered = applyFilters(source,op.filters,op.scissor);
            if (filtered == layers[op.layer].texture) {
                auto& output = temporary(filtered);
                blit(filtered,output,0,fullScissor());
                filtered = output.texture;
            }
            bindSurface(layers[op.layer]);
            auto constants = push();
            constants.matrix[0] = static_cast<float>(width);
            constants.matrix[5] = static_cast<float>(height);
            int pipelineIndex = (op.blend == Rml::BlendMode::Replace ? 4 : 0) + (op.clip ? 1 : 0);
            draw(quad,filtered,nullptr,nullptr,constants,layerPipelines[pipelineIndex],op.scissor,op.stencil);
        }
    }
    endPass();
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = initialLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = outputPass;
    pass.framebuffer = target(image).framebuffer;
    pass.renderArea.extent = {static_cast<uint32_t>(width),static_cast<uint32_t>(height)};
    vkCmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    vkCmdSetViewport(command,0,1,&viewport);
    auto constants = push();
    constants.matrix[0] = static_cast<float>(width);
    constants.matrix[5] = static_cast<float>(height);
    constants.config[3] = outputSrgb;
    draw(quad,layers[0].texture,nullptr,nullptr,constants,outputPipeline,fullScissor(),0);
    vkCmdEndRenderPass(command);
    retire();
    command = nullptr;
}
