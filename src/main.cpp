#include "vulkan.hpp"
#include <format>
#include <iostream>

int main() {
  VulkanApp app;
  try {
    app.run();
  } catch (const std::exception &e) {
    using std::cout;
    using std::format;

    cout << format("{}\n", e.what());
  }
}