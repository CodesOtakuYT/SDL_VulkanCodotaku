#pragma once
#include <volk.h>
#include <stdexcept>

struct VkbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void vkCheck(VkResult result, const char* msg) {
    if (result != VK_SUCCESS) throw VkbError(msg);
}

// --- Debug utils helpers (no-op when VK_EXT_debug_utils is not loaded) ---

[[maybe_unused]] inline void vkSetObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
    if (!vkSetDebugUtilsObjectNameEXT) return;
    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &info);
}

[[maybe_unused]] inline void vkCmdBeginLabel(VkCommandBuffer cmd, const char* name, float r = 0.46f, float g = 0.56f, float b = 0.96f, float a = 1.0f) {
    if (!vkCmdBeginDebugUtilsLabelEXT) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = r; label.color[1] = g; label.color[2] = b; label.color[3] = a;
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

[[maybe_unused]] inline void vkCmdEndLabel(VkCommandBuffer cmd) {
    if (!vkCmdEndDebugUtilsLabelEXT) return;
    vkCmdEndDebugUtilsLabelEXT(cmd);
}
