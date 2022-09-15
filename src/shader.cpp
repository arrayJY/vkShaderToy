#include "shader.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

vk::ShaderModule createShaderModule(const std::string &path,
                                    vk::Device device) {
  std::ifstream loadFile(path, std::ios::ate | std::ios::binary);

  if (!loadFile.is_open()) {
    throw std::runtime_error("Open shader file failed.");
  }
  auto fileSize = loadFile.tellg();
  std::vector<char> buffer(fileSize);
  loadFile.seekg(0);

  loadFile.close();

  vk::ShaderModuleCreateInfo createInfo = {};
  createInfo.setCodeSize(buffer.size());
  createInfo.setPCode(reinterpret_cast<const uint32_t *>(buffer.data()));

  vk::ShaderModule shaderModule;
  if (device.createShaderModule(&createInfo, 0, &shaderModule) !=
      vk::Result::eSuccess) {
    throw std::runtime_error("Open shader file failed.");
  }

  return shaderModule;
}

void createPiplineShader(vk::Device &device) {
}
