#include "app_base.hpp"

#include <iostream>
#include <ranges>
#include <unordered_set>

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
    }




} // imr