#ifndef VK_IMM_RENDERER_APP_BASE_HPP
#define VK_IMM_RENDERER_APP_BASE_HPP

#include <vector>
#include <string>

#include "vulkan/vulkan_raii.hpp"
#include <GLFW/glfw3.h>

#include "renderer.hpp"

namespace imr {

    class AppBase {
    protected:
        AppBase() {
            initGlfw();
            initVulkan();
        };

        ~AppBase() {

        }
    private:
        // GLFW
        GLFWwindow* window;

        void initGlfw();

        // Vulkan
        static const std::vector<const char*> deviceExtensions;
        static const std::vector<const char*> validationLayers;

        vk::raii::Context context;
        vk::raii::Instance instance{VK_NULL_HANDLE};

        vk::raii::PhysicalDevice physicalDevice{VK_NULL_HANDLE};
        vk::raii::Device device{VK_NULL_HANDLE};

        vk::raii::SurfaceKHR surface{VK_NULL_HANDLE};

        vk::raii::Queue graphicsQueue{VK_NULL_HANDLE};
        vk::raii::Queue presentQueue{VK_NULL_HANDLE};

        vk::raii::CommandPool commandPool{VK_NULL_HANDLE};

        vk::Format swapchainImageFormat;
        vk::Extent2D swapchainExtent;

        std::vector<vk::raii::Framebuffer> swapchainFramebuffers;
        vk::raii::RenderPass renderPass{VK_NULL_HANDLE};

        // Frame info
        std::vector<vk::raii::Image> depthImages;
        std::vector<vk::raii::DeviceMemory> depthImageMemorys;
        std::vector<vk::raii::ImageView> depthImageViews;
        std::vector<vk::Image> swapchainImages;
        std::vector<vk::raii::ImageView> swapchainImageViews;
        std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
        std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
        std::vector<vk::raii::Fence> imageInFlightFences;

        vk::Extent2D windowExtent;

        vk::raii::SwapchainKHR swapchain{VK_NULL_HANDLE};

        uint32_t currentFrame = 0;

        std::vector<const char*> getGlfwRequiredExtensions();
        bool isDeviceSuitable(vk::raii::PhysicalDevice& physicalDev, vk::raii::SurfaceKHR& surf);
        void initVulkan();

    public:

        virtual void onDraw(Renderer& renderer) {

        }

        void onFrame(Renderer& renderer) {
            // process events


            // draw frame
            //renderer.begin();
            onDraw(renderer);
            //renderer.end();
        }

        void run() {
            Renderer renderer;

            while(!glfwWindowShouldClose(this->window)) {
                onFrame(renderer);
            }
        }
    };

} // imr

#endif //VK_IMM_RENDERER_APP_BASE_HPP
