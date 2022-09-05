#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>

class VulkanApp {
public:
  void initVulkan();

  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();

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

  vk::SurfaceKHR surface;
  std::vector<vk::PresentModeKHR> presentModes;
  std::vector<vk::SurfaceFormatKHR> surfaceFormats;
  vk::SurfaceCapabilitiesKHR surfaceCapabilities;
  HWND hwnd;
  HINSTANCE hInstance;

  vk::Format format;
  vk::SwapchainKHR swapchain;
  std::vector<vk::Image> swapchainImages;
  std::vector<vk::ImageView> swapchainIamgesViews;
  uint32_t currentImage = 0;
  uint32_t frameCount = 0;
  uint32_t width = 0, height = 0;


  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions;

  std::vector<vk::QueueFamilyProperties> queueFamilyProps;
  uint32_t graphicIndex;

  static const unsigned WIDTH = 800;
  static const unsigned HEIGHT = 600;
  struct GLFWwindow *window;
};
