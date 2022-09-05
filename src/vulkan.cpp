#include "vulkan.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

/** Vulkan **/
void VulkanApp::initVulkan() { createInstance(); }

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

    instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    if (enableValidationLayers) {
      instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
    };

    instance = vk::createInstance(createInfo);
  }

  // Setup debug messenger
  {
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
                                              &debugMessenger, dispatch) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("Create debug util messenger failed");
    };
  }

  // Create device and queue
  {
    auto physicalDevices = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
      throw std::runtime_error("No physical device.");
    } else {
      gpu = physicalDevices[0];
    }

    queueFamilyProps = gpu.getQueueFamilyProperties();

    auto graphicProp = std::find_if(
        queueFamilyProps.begin(), queueFamilyProps.end(), [](auto &prop) {
          return prop.queueFlags & vk::QueueFlagBits::eGraphics;
        });

    if (auto found = graphicProp != queueFamilyProps.end(); !found) {
      throw std::runtime_error("No found queue family.");
    }

    graphicIndex = std::distance(queueFamilyProps.begin(), graphicProp);

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
  }
}

/** GLFW **/
void VulkanApp::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window =
      glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Shader Toy", nullptr, nullptr);
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
