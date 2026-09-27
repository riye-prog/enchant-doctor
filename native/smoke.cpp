#include "renderer.hpp"
#include "ui.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <chrono>
#include <algorithm>

static int validationErrors;
static VKAPI_ATTR VkBool32 VKAPI_CALL validationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++validationErrors;
        std::cerr << message->pMessage << '\n';
    }
    return VK_FALSE;
}

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
        bool debugUtils = std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) { return std::strcmp(extension.extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0; });
        if (debugUtils) enabled.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
        VkDebugUtilsMessengerEXT messenger{};
        if (debugUtils) {
            VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pfnUserCallback = validationMessage;
            checkVk(vkCreateDebugUtilsMessengerEXT(instance,&debug,nullptr,&messenger));
        }
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
        float scale = argc > 7 ? std::stof(argv[7]) : 1.f;
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
            Renderer renderer(instance, device, physical, imageInfo.format, argv[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            Rml::SetSystemInterface(&system);
            Rml::SetRenderInterface(&renderer);
            if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");
            if (!Rml::LoadFontFace(std::string(argv[1]) + "/Manrope.ttf")) throw std::runtime_error("Font load failed");
            auto ui = std::make_unique<UiController>(argv[1],width,height,scale);
            renderer.begin(width,height);
            ui->warmup();
            renderer.begin(width,height);
            ui->show(true);
            if (argc > 6) ui->selectTab(argv[6]);
            if (argc > 8) {
                int progress = std::stoi(argv[8]);
                ui->setProgress(progress);
                ui->text("recovery-count", std::to_string(progress));
            }
            system.elapsed = 1;
            ui->render();
            auto* workbench = ui->element("workbench");
            auto panelPosition = workbench->GetAbsoluteOffset();
            auto panelSize = workbench->GetBox().GetSize();
            if (panelPosition.x < 0 || panelPosition.y < 0 || panelPosition.x + panelSize.x > width + 2 || panelPosition.y + panelSize.y > height + 2)
                throw std::runtime_error("Workbench extends outside the viewport");
            if (panelSize.x > 582 * scale || panelSize.y > 512 * scale)
                throw std::runtime_error("Workbench exceeds compact dimensions");
            VkQueryPoolCreateInfo timingInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            timingInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            timingInfo.queryCount = 2;
            VkQueryPool timing;
            checkVk(vkCreateQueryPool(device,&timingInfo,nullptr,&timing));
            auto submitFrame = [&] {
                checkVk(vkResetCommandPool(device,pool,0));
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                checkVk(vkBeginCommandBuffer(command, &begin));
                vkCmdResetQueryPool(command,timing,0,2);
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkClearColorValue clear{{0.16f,0.21f,0.27f,1.0f}};
                vkCmdClearColorImage(command,image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&barrier.subresourceRange);
                vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timing,0);
                auto started = std::chrono::steady_clock::now();
                renderer.record(command,image,VK_IMAGE_LAYOUT_GENERAL);
                auto retirement = renderer.takeRetirement();
                double recordTime = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
                vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timing,1);
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
                renderer.releaseRetirement(retirement);
                uint64_t timestamps[2]{};
                checkVk(vkGetQueryPoolResults(device,timing,0,2,sizeof(timestamps),timestamps,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT));
                return std::pair(recordTime,(timestamps[1]-timestamps[0])*properties.limits.timestampPeriod/1000.0);
            };
            auto readPixels = [&] {
                void* mapped;
                checkVk(vkMapMemory(device,readbackMemory,0,VK_WHOLE_SIZE,0,&mapped));
                auto* pixels = static_cast<unsigned char*>(mapped);
                std::vector<unsigned char> result(pixels,pixels+width*height*4);
                vkUnmapMemory(device,readbackMemory);
                return result;
            };
            submitFrame();
            auto firstFrame = readPixels();
            std::vector<double> cpuTimes, gpuTimes;
            for (int sample = 0; sample < 120; sample++) {
                system.elapsed += 1.0/60.0;
                auto started = std::chrono::steady_clock::now();
                renderer.begin(width,height);
                ui->render();
                double updateTime = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
                auto [recordTime,gpuTime] = submitFrame();
                if (sample == 30) firstFrame = readPixels();
                cpuTimes.push_back(updateTime+recordTime);
                gpuTimes.push_back(gpuTime);
            }
            auto stableFrame = readPixels();
            if (firstFrame != stableFrame) throw std::runtime_error("Static UI changed across repeated GPU frames");
            std::ofstream output(argv[3],std::ios::binary);
            output << "P6\n" << width << ' ' << height << "\n255\n";
            for (int i=0; i<width*height; i++) output.write(reinterpret_cast<const char*>(stableFrame.data()+i*4),3);
            output.close();
            std::sort(cpuTimes.begin(),cpuTimes.end());
            std::sort(gpuTimes.begin(),gpuTimes.end());
            std::cout << "UI update + Vulkan recording CPU: median " << cpuTimes[60] << " us, p95 " << cpuTimes[114]
                << " us; GPU: median " << gpuTimes[60] << " us, p95 " << gpuTimes[114] << " us\n";
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
            if (!ui->element("view-route")->IsVisible() || !ui->element("tab-planner")->IsClassSet("selected"))
                throw std::runtime_error("Search did not open the route screen");
            click("edit-plan");
            if (ui->value("item") != "minecraft:book" || ui->value("wanted") != "efficiency:4,unbreaking:3")
                throw std::runtime_error("Editing a route lost the planner fields");
            click("reopen-route");
            click("drop-plan");
            if (ui->pollAction() != "drop") throw std::runtime_error("Drop button input failed");
            click("copy-route");
            if (ui->pollAction() != "copy") throw std::runtime_error("Copy button input failed");
            click("tab-offers");
            click("read-table");
            if (ui->pollAction() != "observe") throw std::runtime_error("Observation button input failed");
            click("tab-session");
            click("reset-session");
            if (ui->pollAction() != "reset") throw std::runtime_error("Session reset button input failed");
            click("show-shortcuts");
            if (!ui->element("view-shortcuts")->IsVisible() || !ui->element("tab-session")->IsClassSet("selected"))
                throw std::runtime_error("Shortcuts did not retain session navigation");
            click("customize-controls");
            if (ui->pollAction() != "controls") throw std::runtime_error("Customize keys button input failed");
            click("tab-recovery");
            ui->setProgress(12);
            for (int i = 0; i < 12; i++)
                if (!ui->element("probe-" + std::to_string(i))->IsClassSet("complete")) throw std::runtime_error("Completed recovery sample is missing");
            ui->setProgress(0);
            if (ui->element("probe-0")->IsClassSet("complete")) throw std::runtime_error("Recovery progress did not reset");
            click("close-button");
            if (ui->pollAction() != "close") throw std::runtime_error("Back to game button input failed");
            ui->resize(width/2,height,scale);
            ui->render();
            ui->resize(width,height,scale);
            ui->render();
            std::cout << "Rendered " << width << 'x' << height << ", mouse actions, tabs, text input and resize passed; RmlUi warnings " << system.errors << '\n';
            ui.reset();
            auto* effectsContext = Rml::CreateContext("effects",{width,height});
            auto* effectsDocument = effectsContext->LoadDocument(std::string(DOCTRINE_TEST_ASSETS)+"/effects.rml");
            if (!effectsDocument) throw std::runtime_error("Effects fixture failed to load");
            effectsDocument->Show();
            renderer.begin(width,height);
            effectsContext->Update();
            effectsContext->Render();
            submitFrame();
            auto effectPixels = readPixels();
            auto pixel = [&](int x,int y) {
                auto offset = (y*width+x)*4;
                return std::array<int,3>{effectPixels[offset],effectPixels[offset+1],effectPixels[offset+2]};
            };
            auto expect = [&](const char* name,int x,int y,std::array<int,3> expected,int tolerance=4) {
                auto actual = pixel(x,y);
                for (int channel=0; channel<3; ++channel)
                    if (std::abs(actual[channel]-expected[channel])>tolerance)
                        throw std::runtime_error(std::string(name)+" pixel mismatch: "+std::to_string(actual[0])+","+std::to_string(actual[1])+","+std::to_string(actual[2]));
            };
            std::ofstream effectsOutput(std::string(argv[3])+".effects.ppm",std::ios::binary);
            effectsOutput << "P6\n" << width << ' ' << height << "\n255\n";
            for (int i=0; i<width*height; ++i) effectsOutput.write(reinterpret_cast<const char*>(effectPixels.data()+i*4),3);
            effectsOutput.close();
            expect("linear gradient",40,40,{124,0,131});
            expect("radial gradient",100,40,{249,249,249},6);
            expect("repeating gradient",205,40,pixel(215,40));
            expect("opacity",40,100,{148,27,34});
            expect("brightness",100,100,{128,0,0});
            expect("invert",160,100,{0,255,255});
            expect("grayscale",220,100,{54,54,54});
            expect("transformed clip center",280,100,{0,0,255});
            expect("transformed clip outside",260,80,{41,54,69});
            expect("blur center",40,160,{255,0,0});
            expect("box shadow content",100,160,{0,255,0});
            expect("drop shadow content",160,160,{0,0,255});
            expect("nested opacity",220,160,{94,40,52});
            expect("gradient mask",280,160,{146,28,35});
            expect("PNG top left",22,202,{255,0,0});
            expect("PNG top right",57,202,{0,255,0});
            expect("PNG bottom left",22,237,{0,0,255});
            expect("PNG transparent",57,237,{41,54,69});
            expect("backdrop filter",100,220,{0,255,255});
            expect("contrast",160,220,{128,128,128});
            expect("sepia",220,220,{100,89,69});
            expect("saturation",280,220,{54,54,54});
            expect("hue rotation",40,280,{0,109,109});
            expect("filter opacity",100,280,{148,27,34});
            expect("filter chain",160,280,{127,255,255});
            auto blurEdge = pixel(18,160);
            if (blurEdge[0] <= 41 || blurEdge[0] >= 230) throw std::runtime_error("Blur did not soften the edge");
            if (pixel(182,184)[0] >= 30) throw std::runtime_error("Drop shadow missing outside content");
            if (pixel(150,35) == pixel(170,45)) throw std::runtime_error("Conic gradient is uniform");
            for (int frame=0; frame<3; ++frame) {
                renderer.begin(width,height);
                effectsContext->Update();
                effectsContext->Render();
                submitFrame();
                if (readPixels() != effectPixels) throw std::runtime_error("Cached effects changed across GPU frames");
            }
            std::array<VkCommandBuffer,3> flightCommands;
            commandInfo.commandBufferCount = flightCommands.size();
            checkVk(vkAllocateCommandBuffers(device,&commandInfo,flightCommands.data()));
            for (int batch=0; batch<8; ++batch) {
                checkVk(vkResetCommandPool(device,pool,0));
                std::vector<uint64_t> retirements;
                for (auto flightCommand : flightCommands) {
                    renderer.begin(width,height);
                    effectsContext->Update();
                    effectsContext->Render();
                    Rml::Mesh transient;
                    Rml::MeshUtilities::GenerateQuad(transient,{330,330},{20,20},Rml::ColourbPremultiplied(255,255,255,255));
                    auto transientGeometry = renderer.CompileGeometry(transient.vertices,transient.indices);
                    renderer.RenderGeometry(transientGeometry,{0,0},0);
                    renderer.ReleaseGeometry(transientGeometry);
                    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                    checkVk(vkBeginCommandBuffer(flightCommand,&begin));
                    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = image;
                    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                    vkCmdPipelineBarrier(flightCommand,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                    VkClearColorValue clear{{0.16f,0.21f,0.27f,1.f}};
                    vkCmdClearColorImage(flightCommand,image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&barrier.subresourceRange);
                    renderer.record(flightCommand,image,VK_IMAGE_LAYOUT_GENERAL);
                    retirements.push_back(renderer.takeRetirement());
                    checkVk(vkEndCommandBuffer(flightCommand));
                    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                    submit.commandBufferCount = 1;
                    submit.pCommandBuffers = &flightCommand;
                    checkVk(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
                }
                checkVk(vkQueueWaitIdle(queue));
                for (auto retirement : retirements) renderer.releaseRetirement(retirement);
            }
            vkFreeCommandBuffers(device,pool,flightCommands.size(),flightCommands.data());
            effectsContext->SetDimensions({width/2,height});
            renderer.begin(width/2,height);
            effectsContext->Update();
            effectsContext->Render();
            submitFrame();
            effectsContext->SetDimensions({width,height});
            renderer.begin(width,height);
            effectsContext->Update();
            effectsContext->Render();
            submitFrame();
            if (readPixels() != effectPixels) throw std::runtime_error("Effects failed to recover after resize");
            std::cout << "Pixel checks passed: gradients, clipping, image decoding, filters, masks, cached layers, overlapping frames and GPU resize\n";
            Rml::RemoveContext("effects");
            vkDestroyQueryPool(device,timing,nullptr);
            Rml::Shutdown();
        }
        vkDestroyCommandPool(device,pool,nullptr);
        vkDestroyBuffer(device,readback,nullptr);
        vkFreeMemory(device,readbackMemory,nullptr);
        vkDestroyImage(device,image,nullptr);
        vkFreeMemory(device,imageMemory,nullptr);
        vkDestroyDevice(device,nullptr);
        if (messenger) vkDestroyDebugUtilsMessengerEXT(instance,messenger,nullptr);
        vkDestroyInstance(instance,nullptr);
        return system.errors || validationErrors ? 1 : 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
