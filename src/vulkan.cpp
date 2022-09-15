#include "vulkan.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include "shader.hpp"
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
  createImageView();
  createGraphicPipline();
  createRenderPass();
  present();
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
}

void VulkanApp::createImageView() {
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
}

void VulkanApp::createRenderPass() {
  auto colorAttachment =
      vk::AttachmentDescription{}
          .setFormat(format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  auto colorAttachmentRef =
      vk::AttachmentReference{}.setAttachment(0).setLayout(
          vk::ImageLayout::eColorAttachmentOptimal);

  auto subpass = vk::SubpassDescription{}
                     .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                     .setColorAttachmentCount(1)
                     .setPColorAttachments(&colorAttachmentRef);
  auto renderPassInfo = vk::RenderPassCreateInfo()
                            .setAttachmentCount(1)
                            .setPAttachments(&colorAttachment)
                            .setSubpassCount(1)
                            .setPSubpasses(&subpass);
  renderPass = device.createRenderPass(renderPassInfo);
}
void VulkanApp::createGraphicPipline() {
  auto vertexShader = createShaderModule("Shaders\\vertex.spv", device);
  auto fragmentShader = createShaderModule("Shaders\\fragement.spv", device);

  using ShaderStage = vk::ShaderStageFlagBits;

  auto vertexShaderCreateInfo = vk::PipelineShaderStageCreateInfo()
                                    .setPName("main")
                                    .setModule(vertexShader)
                                    .setStage(ShaderStage::eVertex);

  /*
    auto SMEData =
        vk::SpecializationMapEntry().setConstantID(0).setOffset(0).setSize(
            sizeof(vk::Format));
    auto specializationInfo = vk::SpecializationInfo()
                                  .setDataSize(sizeof(vk::Format))
                                  .setMapEntryCount(1)
                                  .setPMapEntries(&SMEData)
                                  .setPData(&data);
                                  */

  auto fragmentShaderCreateInfo = vk::PipelineShaderStageCreateInfo()
                                      .setPName("main")
                                      .setModule(fragmentShader)
                                      .setStage(ShaderStage::eFragment);

  std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderInfo{
      vertexShaderCreateInfo, fragmentShaderCreateInfo};

  std::vector<vk::DynamicState> dynamicStates = {
      vk::DynamicState::eScissor,
      vk::DynamicState::eViewport,
      vk::DynamicState::eBlendConstants,
  };

  auto dynamicInfo =
      vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamicStates);
  auto viInfo =
      vk::PipelineVertexInputStateCreateInfo()
          .setVertexBindingDescriptions(vertex.bindings)
          .setVertexAttributeDescriptions(vertex.attribute_description);
  auto iaInfo = vk::PipelineInputAssemblyStateCreateInfo()
                    .setTopology(vk::PrimitiveTopology::eTriangleList)
                    .setPrimitiveRestartEnable(VK_FALSE);
  auto rsInfo = vk::PipelineRasterizationStateCreateInfo()
                    .setCullMode(vk::CullModeFlagBits::eBack)
                    .setDepthBiasEnable(VK_FALSE)
                    .setDepthClampEnable(VK_FALSE)
                    .setFrontFace(vk::FrontFace::eClockwise)
                    .setLineWidth(1.0f)
                    .setPolygonMode(vk::PolygonMode::eFill)
                    .setRasterizerDiscardEnable(VK_FALSE);
  vk::Viewport viewport;
  viewport.setMaxDepth(1.0f);
  viewport.setMinDepth(0.0f);
  viewport.setX(0.0f);
  viewport.setY(0.0f);
  viewport.setWidth(width);
  viewport.setHeight(height);

  auto scissor = vk::Rect2D()
                     .setOffset(vk::Offset2D(0.0f, 0.0f))
                     .setExtent(vk::Extent2D(width, height));
  auto vpInfo = vk::PipelineViewportStateCreateInfo()
                    .setScissorCount(1)
                    .setPScissors(&scissor)
                    .setViewportCount(1)
                    .setPViewports(&viewport);
  auto dsInfo = vk::PipelineDepthStencilStateCreateInfo()
                    .setDepthTestEnable(VK_TRUE)
                    .setDepthWriteEnable(VK_TRUE)
                    .setDepthCompareOp(vk::CompareOp::eLess)
                    .setDepthBoundsTestEnable(VK_FALSE)
                    .setStencilTestEnable(VK_FALSE);
  auto attState =
      vk::PipelineColorBlendAttachmentState()
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
          .setBlendEnable(VK_TRUE)
          .setColorBlendOp(vk::BlendOp::eAdd)
          .setSrcColorBlendFactor(vk::BlendFactor::eSrc1Alpha)
          .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
          .setAlphaBlendOp(vk::BlendOp::eAdd)
          .setSrcAlphaBlendFactor(vk::BlendFactor::eZero)
          .setDstAlphaBlendFactor(vk::BlendFactor::eOne);
  auto cbInfo = vk::PipelineColorBlendStateCreateInfo()
                    .setLogicOpEnable(VK_FALSE)
                    .setAttachmentCount(1)
                    .setPAttachments(&attState)
                    .setLogicOp(vk::LogicOp::eNoOp);
  auto msInfo = vk::PipelineMultisampleStateCreateInfo()
                    .setAlphaToCoverageEnable(VK_TRUE)
                    .setAlphaToOneEnable(VK_FALSE)
                    .setMinSampleShading(1.0f)
                    .setRasterizationSamples(vk::SampleCountFlagBits::e1)
                    .setSampleShadingEnable(VK_TRUE);
  auto range = vk::PushConstantRange()
                   .setOffset(0)
                   //.setSize(dataSize)
                   .setStageFlags(vk::ShaderStageFlagBits::eAll);
  auto plInfo = vk::PipelineLayoutCreateInfo()
                    .setPushConstantRangeCount(1)
                    .setPPushConstantRanges(&range);

  pipelineLayout = device.createPipelineLayout(plInfo);

  auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
                          .setStages(pipelineShaderInfo)
                          .setPVertexInputState(&viInfo)
                          .setPInputAssemblyState(&iaInfo)
                          .setPViewportState(&vpInfo)
                          .setPRasterizationState(&rsInfo)
                          .setPMultisampleState(&msInfo)
                          .setPDepthStencilState(&dsInfo)
                          .setPColorBlendState(&cbInfo)
                          .setPDynamicState(&dynamicInfo)
                          .setLayout(pipelineLayout)
                          .setRenderPass(renderPass)
                          .setSubpass(0);
  if (auto p = device.createGraphicsPipeline(nullptr, pipelineInfo);
      p.result == vk::Result::eSuccess) {
    pipeline = p.value;
  } else {
    throw std::runtime_error("Create graphic pipline failed.");
  }
}

void VulkanApp::createFrambuffers() {
  swapChainFramebuffers.resize(swapchainIamgesViews.size());
  for (auto i = 0; i < swapchainIamgesViews.size(); i++) {
    vk::ImageView attachments[] = {swapchainIamgesViews[i]};

    auto frambufferInfo = vk::FramebufferCreateInfo()
                              .setRenderPass(renderPass)
                              .setAttachmentCount(1)
                              .setPAttachments(attachments)
                              .setWidth(width)
                              .setHeight(height)
                              .setLayers(1);
    swapChainFramebuffers[i] = device.createFramebuffer(frambufferInfo);
  }
}

void VulkanApp::present() {
  auto presentInfo = vk::PresentInfoKHR()
                         .setImageIndices(currentImage)
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