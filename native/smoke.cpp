#include "renderer.hpp"
#include "ui.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <chrono>
#include <algorithm>

class SmokeSystem : public Rml::SystemInterface {
public:
    int errors{};
    double elapsed{};
    double GetElapsedTime() override { return elapsed; }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type <= Rml::Log::LT_WARNING) errors++;
        std::cerr << message << '\n';
        return true;
    }
};

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    try {
        checkVk(volkInitialize());
        uint32_t count = 0;
        checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> extensions(count);
        checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()));
        std::vector<const char*> enabled;
        for (auto& extension : extensions)
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) enabled.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "Doctrine offscreen test";
        application.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo creation{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        creation.pApplicationInfo = &application;
        creation.enabledExtensionCount = static_cast<uint32_t>(enabled.size());
        creation.ppEnabledExtensionNames = enabled.data();
        if (!enabled.empty()) creation.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        VkInstance instance;
        checkVk(vkCreateInstance(&creation, nullptr, &instance));
        volkLoadInstance(instance);
        checkVk(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        if (!count) throw std::runtime_error("No Vulkan test device");
        std::vector<VkPhysicalDevice> physicalDevices(count);
        checkVk(vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data()));
        auto physical = physicalDevices.front();
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical, &properties);
        std::cout << "Device: " << properties.deviceName << '\n';
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        uint32_t family = 0;
        while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) family++;
        if (family == count) throw std::runtime_error("No graphics queue");
        enabled.clear();
        checkVk(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr));
        extensions.resize(count);
        checkVk(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extensions.data()));
        for (auto& extension : extensions)
            if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) enabled.push_back("VK_KHR_portability_subset");
        float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(enabled.size());
        deviceInfo.ppEnabledExtensionNames = enabled.data();
        VkDevice device;
        checkVk(vkCreateDevice(physical, &deviceInfo, nullptr, &device));
        volkLoadDevice(device);
        VkQueue queue;
        vkGetDeviceQueue(device, family, 0, &queue);
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physical, &memoryProperties);
        auto memoryType = [&](uint32_t bits, VkMemoryPropertyFlags flags) {
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
                if ((bits & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) return i;
            throw std::runtime_error("No test memory type");
        };
        int width = argc > 4 ? std::stoi(argv[4]) : 1440;
        int height = argc > 5 ? std::stoi(argv[5]) : 1000;
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        imageInfo.mipLevels = imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImage image;
        checkVk(vkCreateImage(device, &imageInfo, nullptr, &image));
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory imageMemory;
        checkVk(vkAllocateMemory(device, &allocation, nullptr, &imageMemory));
        checkVk(vkBindImageMemory(device, image, imageMemory, 0));
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = static_cast<VkDeviceSize>(width) * height * 4;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer readback;
        checkVk(vkCreateBuffer(device, &bufferInfo, nullptr, &readback));
        vkGetBufferMemoryRequirements(device, readback, &requirements);
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDeviceMemory readbackMemory;
        checkVk(vkAllocateMemory(device, &allocation, nullptr, &readbackMemory));
        checkVk(vkBindBufferMemory(device, readback, readbackMemory, 0));
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = family;
        VkCommandPool pool;
        checkVk(vkCreateCommandPool(device, &poolInfo, nullptr, &pool));
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        commandInfo.commandPool = pool;
        commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandInfo.commandBufferCount = 1;
        VkCommandBuffer command;
        checkVk(vkAllocateCommandBuffers(device, &commandInfo, &command));
        SmokeSystem system;
        {
            Renderer renderer(device, physical, imageInfo.format, argv[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            Rml::SetSystemInterface(&system);
            Rml::SetRenderInterface(&renderer);
            if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");
            if (!Rml::LoadFontFace(std::string(argv[1]) + "/PlexSans.ttf")) throw std::runtime_error("Font load failed");
            auto ui = std::make_unique<UiController>(argv[1],width,height,1.f);
            renderer.begin(width,height);
            ui->warmup();
            renderer.begin(width,height);
            ui->show(true);
            if (argc > 6) ui->selectTab(argv[6]);
            ui->render();
            system.elapsed = 1;
            ui->render();
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            checkVk(vkBeginCommandBuffer(command, &begin));
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkClearColorValue clear{{0.16f,0.21f,0.27f,1.0f}};
            vkCmdClearColorImage(command, image, VK_IMAGE_LAYOUT_GENERAL, &clear,1,&barrier.subresourceRange);
            renderer.record(command, image, VK_IMAGE_LAYOUT_GENERAL);
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            copy.imageExtent = imageInfo.extent;
            vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
            checkVk(vkEndCommandBuffer(command));
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            checkVk(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
            checkVk(vkQueueWaitIdle(queue));
            void* mapped;
            checkVk(vkMapMemory(device,readbackMemory,0,VK_WHOLE_SIZE,0,&mapped));
            std::ofstream output(argv[3],std::ios::binary);
            output << "P6\n" << width << ' ' << height << "\n255\n";
            auto* pixels = static_cast<char*>(mapped);
            for (int i=0; i<width*height; i++) output.write(pixels+i*4,3);
            vkUnmapMemory(device,readbackMemory);
            std::vector<double> times;
            for (int sample = 0; sample < 500; sample++) {
                auto start = std::chrono::steady_clock::now();
                renderer.begin(width,height);
                ui->render();
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
            }
            std::sort(times.begin(),times.end());
            std::cout << "RmlUi update and draw-list CPU: median " << times[250] << " us, p95 " << times[475] << " us\n";
            auto click = [&](const std::string& id) {
                auto* element = ui->element(id);
                if (!element) throw std::runtime_error("Missing element: " + id);
                element->ScrollIntoView();
                ui->render();
                auto point = element->GetAbsoluteOffset();
                auto size = element->GetBox().GetSize();
                ui->input(0,point.x+size.x*.5,point.y+size.y*.5,0,0);
                ui->input(1,0,0,1,0);
                ui->input(2,0,0,1,0);
                ui->render();
            };
            click("tab-recovery");
            click("start-recovery");
            if (ui->pollAction() != "recover") throw std::runtime_error("Recovery button input failed");
            click("cancel-recovery");
            if (ui->pollAction() != "cancel") throw std::runtime_error("Cancel button input failed");
            click("tab-planner");
            click("item");
            ui->input(4,0,0,4,192);
            for (char character : std::string("minecraft:book")) ui->input(6,0,0,character,0);
            if (ui->value("item") != "minecraft:book") throw std::runtime_error("Keyboard text replacement failed: " + ui->value("item"));
            click("search-route");
            if (ui->pollAction() != "search") throw std::runtime_error("Planner button input failed");
            click("drop-plan");
            if (ui->pollAction() != "drop") throw std::runtime_error("Drop button input failed");
            click("tab-offers");
            click("read-table");
            if (ui->pollAction() != "observe") throw std::runtime_error("Observation button input failed");
            ui->resize(width/2,height,1.f);
            ui->render();
            ui->resize(width,height,1.f);
            ui->render();
            std::cout << "Rendered " << width << 'x' << height << ", mouse actions, tabs, text input and resize passed; RmlUi warnings " << system.errors << '\n';
            ui.reset();
            Rml::Shutdown();
        }
        vkDestroyCommandPool(device,pool,nullptr);
        vkDestroyBuffer(device,readback,nullptr);
        vkFreeMemory(device,readbackMemory,nullptr);
        vkDestroyImage(device,image,nullptr);
        vkFreeMemory(device,imageMemory,nullptr);
        vkDestroyDevice(device,nullptr);
        vkDestroyInstance(instance,nullptr);
        return system.errors ? 1 : 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
