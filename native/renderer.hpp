#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>
#include <RmlUi/Core.h>
#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

void checkVk(VkResult result);

class Renderer final : public Rml::RenderInterface {
    struct Buffer { VkBuffer buffer{}; VmaAllocation allocation{}; };
    struct Geometry { Buffer vertices; Buffer indices; uint32_t count{}; };
    struct Texture {
        VkImage image{};
        VmaAllocation allocation{};
        VkImageView view{};
        VkDescriptorSet set{};
        Buffer staging;
        int width{};
        int height{};
        VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
    };
    struct Effect {
        Buffer buffer;
        VkDescriptorSet set{};
        int mode{};
        int count{};
        bool repeating{};
    };
    struct Filter {
        enum class Type { Matrix, Opacity, Blur, Shadow, Mask } type{Type::Matrix};
        Effect* effect{};
        Texture* mask{};
        float value{1};
        Rml::Vector2f offset{};
        Rml::Colourf color{};
    };
    struct Surface {
        Texture* texture{};
        Texture* multisample{};
        VkFramebuffer framebuffer{};
        bool initialized{};
    };
    struct Target { VkImageView view{}; VkFramebuffer framebuffer{}; };
    struct Retirement { uint64_t id{}; std::vector<std::function<void()>> releases; };
    struct Push {
        float matrix[16];
        float translation[2];
        float extent[2];
        float data[4];
        float color[4];
        int config[4];
    };
    struct Operation {
        enum class Type { Draw, Clip, Clear, Composite, Save } type{Type::Draw};
        Geometry* geometry{};
        Texture* texture{};
        Effect* effect{};
        Rml::Vector2f translation{};
        Rml::Matrix4f transform{Rml::Matrix4f::Identity()};
        VkRect2D scissor{};
        int layer{};
        int source{};
        int stencil{};
        bool clip{};
        Rml::ClipMaskOperation clipOperation{};
        Rml::BlendMode blend{Rml::BlendMode::Blend};
        std::vector<Filter*> filters;
    };
    VkDevice device;
    VkPhysicalDevice physical;
    VkFormat format;
    VkImageLayout finalLayout;
    VmaAllocator allocator{};
    VkRenderPass layerPass{};
    VkRenderPass effectPass{};
    VkRenderPass outputPass{};
    VkDescriptorSetLayout textureLayout{};
    VkDescriptorSetLayout effectLayout{};
    VkDescriptorPool descriptorPool{};
    VkPipelineLayout pipelineLayout{};
    std::array<VkPipeline, 6> layerPipelines{};
    VkPipeline effectPipeline{};
    VkPipeline outputPipeline{};
    VkSampler sampler{};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    VkFormat stencilFormat{};
    Texture* stencilTexture{};
    std::unordered_map<VkImage, Target> targets;
    std::vector<Texture*> textures;
    std::vector<Geometry*> geometries;
    std::vector<Effect*> effects;
    std::vector<Filter*> filters;
    std::vector<Operation> operations;
    std::vector<Surface> layers;
    std::array<Surface, 3> scratch{};
    std::vector<std::function<void()>> pendingReleases;
    std::vector<Retirement> retirements;
    uint64_t lastRetirement{};
    Texture* white{};
    Geometry* quad{};
    Effect* identity{};
    Rml::Matrix4f transform{Rml::Matrix4f::Identity()};
    VkRect2D scissor{};
    bool scissorEnabled{};
    bool clipEnabled{};
    int stencilReference{};
    int activeLayer{};
    int maximumLayer{};
    int width{};
    int height{};
    int surfaceWidth{};
    int surfaceHeight{};
    VkCommandBuffer command{};
    Surface* activeSurface{};
    bool passActive{};
    bool outputSrgb{};
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data);
    void freeBuffer(Buffer value);
    Texture* texture(int width, int height, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void freeTexture(Texture* value);
    void freeGeometry(Geometry* value);
    void freeEffect(Effect* value);
    void freeFilter(Filter* value);
    Effect* effect(int mode, const Rml::Matrix4f& matrix, Rml::Vector4f data0 = {}, Rml::Vector4f data1 = {}, const Rml::ColorStopList* stops = nullptr, bool repeating = false);
    VkShaderModule shader(const std::string& path);
    VkRenderPass renderPass(VkFormat format, bool stencil, bool multisample, VkImageLayout finalLayout);
    VkPipeline pipeline(VkRenderPass pass, VkShaderModule vertex, VkShaderModule fragment, VkSampleCountFlagBits samples, int stencilMode, bool blend);
    Surface surface(bool withStencil);
    void freeSurface(Surface& value);
    void prepareSurfaces();
    void endPass();
    void bindSurface(Surface& value, bool clear = false);
    void transition(Texture* value, VkImageLayout layout);
    void upload(Texture* value);
    VkRect2D fullScissor() const;
    VkRect2D currentScissor() const;
    Operation operation(Operation::Type type) const;
    void draw(Geometry* geometry, Texture* texture, Texture* auxiliary, Effect* effect, Push push, VkPipeline pipeline, VkRect2D bounds, int stencil);
    Push push(Effect* effect = nullptr) const;
    void blit(Texture* input, Surface& destination, int mode, VkRect2D bounds, Effect* effect = nullptr, Texture* auxiliary = nullptr, Rml::Vector4f data = {}, Rml::Colourf color = {});
    Texture* applyFilters(Texture* input, const std::vector<Filter*>& filters, VkRect2D bounds);
    Surface& temporary(Texture* avoidA = nullptr, Texture* avoidB = nullptr);
    Target& target(VkImage image);
    void retire();
public:
    Renderer(VkInstance instance, VkDevice device, VkPhysicalDevice physical, VkFormat format, const std::string& assets, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    ~Renderer() override;
    void begin(int width, int height);
    void record(VkCommandBuffer command, VkImage image, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    void clearTargets();
    uint64_t takeRetirement();
    void releaseRetirement(uint64_t id);
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* matrix) override;
    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;
    Rml::LayerHandle PushLayer() override;
    void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend, Rml::Span<const Rml::CompiledFilterHandle> filters) override;
    void PopLayer() override;
    Rml::TextureHandle SaveLayerAsTexture() override;
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;
    Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void ReleaseFilter(Rml::CompiledFilterHandle filter) override;
    Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseShader(Rml::CompiledShaderHandle shader) override;
};
