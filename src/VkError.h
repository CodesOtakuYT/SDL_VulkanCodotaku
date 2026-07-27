#pragma once
#include <volk.h>
#include <stdexcept>

struct VkbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void vkCheck(VkResult result, const char* msg) {
    if (result != VK_SUCCESS) throw VkbError(msg);
}
