#include "app_base.hpp"

#include <iostream>
#include <ranges>
#include <unordered_set>
#include <fstream>

namespace imr {

    const std::vector<const char*> AppBase::deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char*> AppBase::validationLayers = {"VK_LAYER_KHRONOS_validation"};

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            vk::DebugUtilsMessageTypeFlagsEXT messageType,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    void AppBase::initGlfw() {
        if(!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        this->window = glfwCreateWindow(640, 480, "Window", nullptr, nullptr);
        if (!this->window) throw std::runtime_error("Failed to create window");
    }

    std::vector<const char *> AppBase::getGlfwRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        //if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        //}

        return extensions;
    }

    bool AppBase::isDeviceSuitable(vk::raii::PhysicalDevice &physicalDev, vk::raii::SurfaceKHR &surf) {
        // Supported Queues check
        bool hasGraphicsQueue = false, hasPresentQueue = false;
        auto queueFamilies = physicalDev.getQueueFamilyProperties();
        for (int i = 0; i < queueFamilies.size(); i++) {
            hasGraphicsQueue = hasGraphicsQueue || (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics);
            hasPresentQueue = hasPresentQueue || (queueFamilies[i].queueCount > 0 && physicalDev.getSurfaceSupportKHR(i, *surf));
        }

        if (!hasGraphicsQueue || !hasPresentQueue) return false;

        // Extensions check
        auto supportedExtensions = physicalDev.enumerateDeviceExtensionProperties();
        std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &ext: supportedExtensions) {
            requiredExtensions.erase(ext.extensionName);
        }

        if (!requiredExtensions.empty()) return false;

        // Surface Format & PresentMode check
        auto supportedSurfaceFormats = physicalDev.getSurfaceFormatsKHR(*surface);
        auto supportedPresentModes = physicalDev.getSurfacePresentModesKHR(*surface);

        if (supportedSurfaceFormats.empty() || supportedPresentModes.empty()) return false;

        // Features check
        vk::PhysicalDeviceFeatures supportedFeatures = physicalDev.getFeatures();

        return supportedFeatures.samplerAnisotropy;
    }

    void AppBase::initVulkan() {
        // Vulkan Instance creation
        vk::ApplicationInfo appInfo {
                "Vulkan Immediate Renderer",
                VK_MAKE_VERSION(1, 0, 0),
                "No Engine",
                VK_MAKE_VERSION(0, 0, 0),
                VK_API_VERSION_1_0
        };

        auto extensions = getGlfwRequiredExtensions();

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)debugCallback;
        debugCreateInfo.pUserData = nullptr;  // Optional

        vk::InstanceCreateInfo createInfo {
                {},
                &appInfo,
                validationLayers,
                extensions,
                &debugCreateInfo
        };

        this->instance = context.createInstance(createInfo);

        // Surface creation
        VkSurfaceKHR tmpSurface;
        if(glfwCreateWindowSurface(*this->instance, this->window, nullptr, &tmpSurface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        this->surface = vk::raii::SurfaceKHR(this->instance, tmpSurface);

        // Pick PhysicalDevice
        auto physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty()) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

        bool foundSuitablePhysicalDevice = false;
        for (auto& dev : physicalDevices) {
            if (isDeviceSuitable(dev, this->surface)) {
                foundSuitablePhysicalDevice = true;
                this->physicalDevice = std::move(dev);
                break;
            }
        }
//        auto suitableDevices = physicalDevices |
//                std::views::filter([this](auto& dev){return isDeviceSuitable(dev, this->surface);});

        if (!foundSuitablePhysicalDevice) throw std::runtime_error("Failed to find a suitable GPU!");

        // this->physicalDevice = std::move(suitableDevices.front());
        std::cout << "Physical Device: " << this->physicalDevice.getProperties().deviceName << '\n';

        // Logical Device creation
        bool foundGraphicsQueue = false, foundPresentQueue = false;
        uint32_t graphicsQueueFamilyIndex = 0, presentQueueFamilyIndex = 0;
        auto queueFamilies = this->physicalDevice.getQueueFamilyProperties();
        for (int i = 0; i < queueFamilies.size(); i++) {
            if (!foundGraphicsQueue && (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
                graphicsQueueFamilyIndex = i;
                foundGraphicsQueue = true;
            }

            if (!foundPresentQueue && (queueFamilies[i].queueCount > 0 && this->physicalDevice.getSurfaceSupportKHR(i, *this->surface))) {
                presentQueueFamilyIndex = i;
                foundPresentQueue = true;
            }

            if (foundPresentQueue && foundGraphicsQueue) break;
        }

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        float queuePriority = 1.0f;
        queueCreateInfos.push_back({{}, graphicsQueueFamilyIndex, 1, &queuePriority});

        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            queueCreateInfos.push_back({{}, presentQueueFamilyIndex, 1, &queuePriority});
        }

        vk::PhysicalDeviceFeatures deviceFeatures {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        vk::DeviceCreateInfo deviceCreateInfo {
            {},
            queueCreateInfos,
            validationLayers,
            deviceExtensions,
            &deviceFeatures
        };

        this->device = this->physicalDevice.createDevice(deviceCreateInfo);

        // Command Pool creation
        vk::CommandPoolCreateInfo cmdPoolCreateInfo {
                vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                graphicsQueueFamilyIndex
        };

        this->commandPool = this->device.createCommandPool(cmdPoolCreateInfo);

        // Swapchain creation
        auto surfaceCapabilities = this->physicalDevice.getSurfaceCapabilitiesKHR(*this->surface);
        auto surfaceFormats = this->physicalDevice.getSurfaceFormatsKHR(*this->surface);
        auto presentModes = this->physicalDevice.getSurfacePresentModesKHR(*this->surface);

        vk::SurfaceFormatKHR surfaceFormat = [&]{
            for (const auto& f : surfaceFormats) {
                if (f.format == vk::Format::eB8G8R8A8Srgb &&
                    f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {

                    return f;
                }
            }
            throw std::runtime_error("Couldn't find appropriate surface format");
        }();

        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo; // TODO
        vk::Extent2D surfaceExtent = vk::Extent2D{640, 480}; //this->windowExtent; // TODO set window extent

        uint32_t imageCount = 2; // TODO
        std::vector<uint32_t> queueFamilyIndices = {graphicsQueueFamilyIndex, presentQueueFamilyIndex};

        vk::SwapchainCreateInfoKHR swapchainCreateInfo {
                {},
                *this->surface,
                imageCount,
                surfaceFormat.format,
                surfaceFormat.colorSpace,
                surfaceExtent,
                1,
                vk::ImageUsageFlagBits::eColorAttachment,
                (graphicsQueueFamilyIndex != presentQueueFamilyIndex) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
                static_cast<uint32_t>(queueFamilyIndices.size()),
                queueFamilyIndices.data(),
                surfaceCapabilities.currentTransform,
                vk::CompositeAlphaFlagBitsKHR::eOpaque,
                presentMode,
                VK_TRUE,
                VK_NULL_HANDLE
        };

        this->swapchain = this->device.createSwapchainKHR(swapchainCreateInfo);
        this->swapchainImages = [this]{
            std::vector<vk::Image> result;
            for (auto& i : swapchain.getImages())
                result.emplace_back(i);

            return std::move(result);
        }();

        this->swapchainImageViews = [this, &surfaceFormat]{
            std::vector<vk::raii::ImageView> result;

            vk::ImageViewCreateInfo viewCreateInfo {
                    {},
                    {},
                    vk::ImageViewType::e2D,
                    surfaceFormat.format,
                    {},
                    {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
            };

            for (auto& i : this->swapchainImages) {
                viewCreateInfo.image = i;
                result.push_back(this->device.createImageView(viewCreateInfo));
            }

            return std::move(result);
        }();

        auto checkFormat = [this](vk::Format wantedFormat, vk::ImageTiling tiling, vk::FormatFeatureFlags features){
            vk::FormatProperties props = this->physicalDevice.getFormatProperties(wantedFormat);
            return  (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) ||
                    (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features);
        };

        auto chooseFormat = [checkFormat](const std::vector<vk::Format>& formats, vk::ImageTiling targetTiling, vk::FormatFeatureFlags targetFlags){
            for (auto& f : formats)
                if (checkFormat(f, targetTiling, targetFlags))
                    return f;

            throw std::runtime_error("There was no format satisfying the requirements");
        };

        vk::Format depthFormat = chooseFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);

        this->renderPass = [&]{
            vk::AttachmentDescription depthAttachment {
                {},
                depthFormat,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eDontCare,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            };

            vk::AttachmentReference depthAttachmentRef {1, vk::ImageLayout::eDepthStencilAttachmentOptimal };

            vk::AttachmentDescription colorAttachment {
                    {},
                    surfaceFormat.format,
                    vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::ePresentSrcKHR
            };

            vk::AttachmentReference colorAttachmentRef {0, vk::ImageLayout::eColorAttachmentOptimal };

            vk::SubpassDescription subpassDescription {
                    {},
                    vk::PipelineBindPoint::eGraphics,
                    0, nullptr,
                    1, &colorAttachmentRef,
                    nullptr,
                    &depthAttachmentRef
            };

            vk::SubpassDependency dependency {
                VK_SUBPASS_EXTERNAL,
                0,
                vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                vk::AccessFlagBits::eNone,
                vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
            };

            std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
            std::array<vk::SubpassDescription, 1> subpasses = {subpassDescription};
            std::array<vk::SubpassDependency, 1> dependencies = {dependency};

            vk::RenderPassCreateInfo renderPassInfo {
                    {},
                    attachments,
                    subpasses,
                    dependencies
            };

            return this->device.createRenderPass(renderPassInfo);
        }();

        // Depth resources
        auto findMemType = [this](uint32_t typeFilter, vk::MemoryPropertyFlags props){
            auto memProps = this->physicalDevice.getMemoryProperties();
            for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
                if ((typeFilter & (1<<i)) && ((memProps.memoryTypes[i].propertyFlags & props) == props)) return i;
            }
            throw std::runtime_error("Failed to find suitable memory type");
        };
        auto createImageWithInfo = [&](const vk::ImageCreateInfo& imageInfo, vk::MemoryPropertyFlags memProps){
            vk::raii::Image image{VK_NULL_HANDLE};
            vk::raii::DeviceMemory imageMemory{VK_NULL_HANDLE};

            image = this->device.createImage(imageInfo);

            vk::MemoryRequirements memReq = image.getMemoryRequirements();

            vk::MemoryAllocateInfo allocInfo {
                memReq.size,
                findMemType(memReq.memoryTypeBits, memProps)
            };

            imageMemory = this->device.allocateMemory(allocInfo);

            image.bindMemory(*imageMemory, 0);

            return std::make_pair(std::move(image), std::move(imageMemory));
        };

        for (int i = 0; i < this->swapchainImages.size(); i++){
            vk::ImageCreateInfo imageInfo {
                    {},
                    vk::ImageType::e2D,
                    depthFormat,
                    vk::Extent3D(640, 480, 1), // TODO add dynamic extent
                    1, 1,
                    vk::SampleCountFlagBits::e1,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eDepthStencilAttachment,
                    vk::SharingMode::eExclusive, 0, nullptr,
                    vk::ImageLayout::eUndefined
            };

            auto [img, mem] = createImageWithInfo(imageInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);

            vk::ImageViewCreateInfo viewInfo {
                    {},
                    *img,
                    vk::ImageViewType::e2D,
                    depthFormat, {},
                    {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
            };

            this->depthImageViews.push_back(this->device.createImageView(viewInfo));
            this->depthImages.push_back(std::move(img));
            this->depthImageMemorys.push_back(std::move(mem));
        }

        // Framebuffers
        for (int i = 0; i < this->swapchainImages.size(); i++) {
            std::array<vk::ImageView, 2> attachments = {*this->swapchainImageViews[i], *this->depthImageViews[i]};

            vk::FramebufferCreateInfo framebufferInfo {
                    {},
                    *this->renderPass,
                    attachments,
                    640, 480, 1
            };

            this->swapchainFramebuffers.push_back(this->device.createFramebuffer(framebufferInfo));
        }

        // Sync objects
        for (int i = 0; i < this->swapchainImages.size(); i++) {
            this->renderFinishedSemaphores.push_back(this->device.createSemaphore({}));
            this->imageAvailableSemaphores.push_back(this->device.createSemaphore({}));
            this->imageInFlightFences.push_back(this->device.createFence({ vk::FenceCreateFlagBits::eSignaled }));
        }

        // Debug messenger
        this->debugMessenger = this->instance.createDebugUtilsMessengerEXT(debugCreateInfo);

        // Pipeline Layout
        vk::PipelineLayoutCreateInfo layoutInfo {
                {}, 0, nullptr, 0, nullptr
        };

        this->pipelineLayout = this->device.createPipelineLayout(layoutInfo);

        // Shaders TODO: compile shaders and make modules
        auto makeShader = [this](const std::string& filepath){
            std::ifstream codeFile{filepath, std::ios::ate | std::ios::binary};

            if (!codeFile.is_open()) throw std::runtime_error("Failed to open file: " + filepath);

            size_t fileSize = static_cast<size_t>(codeFile.tellg());
            std::vector<char> buffer(fileSize);

            codeFile.seekg(0);
            codeFile.read(buffer.data(), fileSize);

            codeFile.close();

            vk::ShaderModuleCreateInfo shaderInfo {
                    {},
                    buffer.size(),
                    reinterpret_cast<const uint32_t*>(buffer.data())
            };

            return this->device.createShaderModule(shaderInfo);
        };

        this->vertexShaderModule = makeShader("../shaders/simple_shader.vert.spv");
        this->fragmentShaderModule = makeShader("../shaders/simple_shader.frag.spv");

        // Pipeline
        vk::PipelineShaderStageCreateInfo shaderStages[2] = {
                {{}, vk::ShaderStageFlagBits::eVertex, *this->vertexShaderModule, "main", nullptr },
                {{}, vk::ShaderStageFlagBits::eFragment, *this->fragmentShaderModule, "main", nullptr }
        };

        vk::Viewport viewport{
            0.0f,
            0.0f,
            640.0f, // TODO dynamic size
            480.0f,
            0.0f,
            1.0f
        };

        vk::Rect2D scissor{
                {0, 0},
                {640, 480} // TODO dynamic size
        };

        vk::PipelineViewportStateCreateInfo viewportStateInfo {
                {},
                1, &viewport,
                1, &scissor
        };

        vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo {
                {}, 0, nullptr, 0, nullptr
        };

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {
                {},
                vk::PrimitiveTopology::eTriangleList,
                VK_FALSE
        };

        vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo {
                {},
                VK_FALSE,
                VK_FALSE,
                vk::PolygonMode::eFill,
                vk::CullModeFlagBits::eNone,
                vk::FrontFace::eClockwise,
                VK_FALSE,
                0.0f, 0.0f, 0.0f,
                1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampleStateInfo {
                {},
                vk::SampleCountFlagBits::e1,
                VK_FALSE,
                1.0f,
                nullptr,
                VK_FALSE,
                VK_FALSE
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachmentState {
                VK_FALSE,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eZero,
                vk::BlendOp::eAdd,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eZero,
                vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo {
                {},
                VK_FALSE,
                vk::LogicOp::eCopy,
                1, &colorBlendAttachmentState,
                {.0f, .0f, .0f, .0f}
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencilStateInfo {
                {},
                VK_TRUE,
                VK_TRUE,
                vk::CompareOp::eLess,
                VK_FALSE,
                VK_FALSE, {}, {},
                0.0f,
                1.0f
        };

        vk::GraphicsPipelineCreateInfo pipelineInfo {
                {},
                2, shaderStages,
                &vertexInputStateInfo,
                &inputAssemblyStateInfo, {},
                &viewportStateInfo,
                &rasterizationStateInfo,
                &multisampleStateInfo,
                &depthStencilStateInfo,
                &colorBlendStateInfo,
                {},
                *this->pipelineLayout,
                *this->renderPass,
                0,
                VK_NULL_HANDLE, -1
        };

        this->pipeline = this->device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);

    }
} // imr