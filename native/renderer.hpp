#pragma once
#include <volk.h>
#include <RmlUi/Core.h>
#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

void checkVk(VkResult result);

class Renderer final : public Rml::RenderInterface {
    struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; };
    struct Geometry { Buffer vertices; Buffer indices; uint32_t count{}; };
    struct Texture { VkImage image{}; VkDeviceMemory memory{}; VkImageView view{}; VkDescriptorSet set{}; Buffer staging; int width{}; int height{}; bool uploaded{}; };
    struct Draw { Geometry* geometry; Texture* texture; Rml::Vector2f translation; Rml::Matrix4f transform; VkRect2D scissor; };
    struct Target { VkImageView view{}; VkFramebuffer framebuffer{}; int width{}; int height{}; };
    struct Retirement { VkEvent event; std::vector<std::function<void()>> releases; };
    VkDevice device;
    VkPhysicalDevice physical;
    VkFormat format;
    VkRenderPass renderPass{};
    VkDescriptorSetLayout descriptorLayout{};
    VkDescriptorPool descriptorPool{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};
    VkSampler sampler{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    std::unordered_map<VkImage, Target> targets;
    std::vector<Texture*> textures;
    std::vector<Geometry*> geometries;
    std::vector<Draw> draws;
    std::vector<std::function<void()>> pendingReleases;
    std::vector<Retirement> retirements;
    Texture* white{};
    Rml::Matrix4f transform = Rml::Matrix4f::Identity();
    VkRect2D scissor{};
    bool scissorEnabled{};
    int width{};
    int height{};
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags);
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data);
    void freeBuffer(Buffer buffer);
    void freeTexture(Texture* texture);
    void freeGeometry(Geometry* geometry);
    VkShaderModule shader(const std::string& path);
    Target& target(VkImage image);
public:
    Renderer(VkDevice device, VkPhysicalDevice physical, VkFormat format, const std::string& assets, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    ~Renderer() override;
    void begin(int width, int height);
    void record(VkCommandBuffer command, VkImage image, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    void clearTargets();
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* matrix) override;
};
