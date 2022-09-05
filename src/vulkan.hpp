#pragma once

#include <vulkan/vulkan.hpp>

class VulkanApp {
public:
  void initVulkan();
  void createInstance();



  void initWindow();
  void mainLoop();
  void cleanup();
  void run();
private:
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::PhysicalDevice gpu;
    vk::Device device;
    vk::Queue graphicQueue;



    std::vector<const char*> instanceExtensions;
    std::vector<const char*> deviceExtensions;

    std::vector<vk::QueueFamilyProperties> queueFamilyProps;
    uint32_t graphicIndex;


    static const unsigned WIDTH = 800;
    static const unsigned HEIGHT = 600;
    class GLFWwindow* window;
};
