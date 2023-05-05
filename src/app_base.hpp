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
        static const std::vector<std::string> deviceExtensions;
        static const std::vector<const char*> validationLayers;

        vk::raii::Context context;
        vk::raii::Instance instance{VK_NULL_HANDLE};

        vk::raii::PhysicalDevice physicalDevice{VK_NULL_HANDLE};
        vk::raii::Device device{VK_NULL_HANDLE};

        vk::raii::SurfaceKHR surface{VK_NULL_HANDLE};

        vk::raii::Queue graphicsQueue{VK_NULL_HANDLE};
        vk::raii::Queue presentQueue{VK_NULL_HANDLE};

        vk::raii::CommandPool commandPool{VK_NULL_HANDLE};

        std::vector<const char*> getGlfwRequiredExtensions();
        bool isDeviceSuitable(vk::raii::PhysicalDevice& physicalDev, vk::raii::SurfaceKHR& surf);
        void initVulkan();

    public:

        virtual void onDraw(Renderer& renderer) {

        }

        void onFrame(Renderer& renderer) {
            // process events


            // draw frame
            renderer.begin();
            onDraw(renderer);
            renderer.end();
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
