#include "vulkan.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

/** Vulkan **/
void VulkanApp::initVulkan() {
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
}

bool checkValidationLayerSupport(const std::vector<const char *> &layerNames) {
  const std::vector<vk::LayerProperties> properties =
      vk::enumerateInstanceLayerProperties();
  return std::all_of(
      layerNames.begin(), layerNames.end(), [&properties](auto layerName) {
        return std::any_of(properties.begin(), properties.end(),
                           [layerName](const vk::LayerProperties &prop) {
                             return prop.layerName == std::string(layerName);
                           });
      });
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL
debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              vk::DebugUtilsMessageTypeFlagsEXT messageType,
              const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {

  using DebugSeverityFlag = vk::DebugUtilsMessageSeverityFlagBitsEXT;
  using std::format;

  const auto message = [pCallbackData, messageSeverity]() {
    switch (messageSeverity) {
    case DebugSeverityFlag::eInfo:
      return format("Info: {}", pCallbackData->pMessage);
    case DebugSeverityFlag::eVerbose:
      return format("Verbose: {}", pCallbackData->pMessage);
    case DebugSeverityFlag::eWarning:
      return format("Warning: {}", pCallbackData->pMessage);
    default:
      return format("Error: {}", pCallbackData->pMessage);
    }
  }();
  auto formattedMessage = format("[validation layer] {}\n", message);
  std::cerr << formattedMessage;
  return VK_FALSE;
}

void VulkanApp::createInstance() {

  // Create instance
  {
    VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                              .pApplicationName = "Vulkan Shader Toy",
                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                              .pEngineName = "No Engine",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_3};

    static const std::vector<const char *> validationLayers{
        "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor"};
    if (enableValidationLayers &&
        !checkValidationLayerSupport(validationLayers)) {
      throw std::runtime_error("Not support validation layer");
    }

    instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    if (enableValidationLayers) {
      instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };

    instance = vk::createInstance(createInfo);
  }
}

void VulkanApp::setupDebugMessenger() {
  using SeverityFlag = vk::DebugUtilsMessageSeverityFlagBitsEXT;
  using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;

  auto callbackFunc =
      reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback);
  auto debugMessengerInfo =
      vk::DebugUtilsMessengerCreateInfoEXT()
          .setMessageSeverity(SeverityFlag::eInfo | SeverityFlag::eVerbose |
                              SeverityFlag::eWarning | SeverityFlag::eError)
          .setMessageType(MessageType::eGeneral | MessageType::eValidation |
                          MessageType::ePerformance)
          .setPfnUserCallback(callbackFunc)
          .setPUserData(nullptr);

  vk::DynamicLoader dl;
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
  vk::DispatchLoaderDynamic dispatch(instance, GetInstanceProcAddr);
  if (instance.createDebugUtilsMessengerEXT(&debugMessengerInfo, 0,
                                            &debugMessenger,
                                            dispatch) != vk::Result::eSuccess) {
    throw std::runtime_error("Create debug util messenger Failed");
  };
}

void VulkanApp::createSurface() {
  auto surfaceInfo =
      vk::Win32SurfaceCreateInfoKHR().setHwnd(hwnd).setHinstance(hInstance);
  surface = instance.createWin32SurfaceKHR(surfaceInfo);
}

void VulkanApp::pickPhysicalDevice() {
  auto physicalDevices = instance.enumeratePhysicalDevices();
  if (physicalDevices.empty()) {
    throw std::runtime_error("No physical device.");
  } else {
    gpu = physicalDevices[0];
  }
}

void VulkanApp::createLogicalDevice() {

  queueFamilyProps = gpu.getQueueFamilyProperties();

  const auto supports =
      std::views::iota((std::size_t)0, queueFamilyProps.size()) |
      std::views::transform([this](auto i) {
        auto support = gpu.getSurfaceSupportKHR(i, surface);
        return support == VK_TRUE &&
               queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics;
      });
  const auto graphicProp = std::find(supports.begin(), supports.end(), true);
  bool found = graphicProp != supports.end();
  if (!found) {
    throw std::runtime_error("No found surface support queue.");
  }
  graphicIndex =
      static_cast<uint32_t>(std::distance(supports.begin(), graphicProp));

  auto feature = vk::PhysicalDeviceFeatures().setGeometryShader(VK_TRUE);

  float priorites[] = {0.0f};
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  auto deviceQueueInfo = vk::DeviceQueueCreateInfo()
                             .setQueueCount(1)
                             .setPQueuePriorities(priorites)
                             .setQueueFamilyIndex(graphicIndex);
  auto deviceInfo = vk::DeviceCreateInfo()
                        .setQueueCreateInfoCount(1)
                        .setPQueueCreateInfos(&deviceQueueInfo)
                        .setEnabledExtensionCount(deviceExtensions.size())
                        .setPpEnabledExtensionNames(deviceExtensions.data())
                        .setPEnabledFeatures(&feature);

  device = gpu.createDevice(deviceInfo);
  graphicQueue = device.getQueue(graphicIndex, 0);

  surfaceFormats = gpu.getSurfaceFormatsKHR(surface);
  if (surfaceFormats.empty()) {
    throw std::runtime_error("Get surface formats khr error");
  }

  presentModes = gpu.getSurfacePresentModesKHR(surface);
  if (presentModes.empty()) {
    throw std::runtime_error("Get present modes khr error");
  }

  surfaceCapabilities = gpu.getSurfaceCapabilitiesKHR(surface);
}

void VulkanApp::createSwapChain() {
  format = surfaceFormats[0].format;
  frameCount = surfaceCapabilities.minImageCount;

  auto swapchainInfo =
      vk::SwapchainCreateInfoKHR()
          .setSurface(surface)
          .setImageFormat(format)
          .setMinImageCount(frameCount)
          .setImageExtent(vk::Extent2D(width, height))
          .setPresentMode(vk::PresentModeKHR::eFifo)
          .setImageSharingMode(vk::SharingMode::eExclusive)
          .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
          .setImageColorSpace(surfaceFormats[0].colorSpace)
          .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
          .setImageArrayLayers(1)
          .setClipped(true);

  swapchain = device.createSwapchainKHR(swapchainInfo);

  swapchainImages = device.getSwapchainImagesKHR(swapchain);
  frameCount = swapchainImages.size();

  swapchainIamgesViews.resize(frameCount);
  for (uint32_t i = 0; i < frameCount; i++) {
    auto createImageViewInfo =
        vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setSubresourceRange(vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
            .setFormat(format)
            .setImage(swapchainImages[i])
            .setComponents(
                vk::ComponentMapping(vk::ComponentSwizzle::eIdentity,
                                     vk::ComponentSwizzle::eIdentity,
                                     vk::ComponentSwizzle::eIdentity,
                                     vk::ComponentSwizzle::eIdentity));
    swapchainIamgesViews[i] = device.createImageView(createImageViewInfo);
  }

  auto presentInfo = vk::PresentInfoKHR()
                         .setPImageIndices(&currentImage)
                         .setSwapchainCount(1)
                         .setPSwapchains(&swapchain);
  if (graphicQueue.presentKHR(presentInfo) != vk::Result::eSuccess) {
    throw std::runtime_error("Set present KHR failed.");
  }
}

/** GLFW **/
void VulkanApp::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window =
      glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Shader Toy", nullptr, nullptr);
  hwnd = glfwGetWin32Window(window);
  hInstance = GetModuleHandle(nullptr);
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  width = w, height = h;
}
void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }
}
void VulkanApp::cleanup() {

  glfwDestroyWindow(window);
  glfwTerminate();
}

void VulkanApp::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}