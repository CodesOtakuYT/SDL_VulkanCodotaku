cmake_minimum_required(VERSION 3.21)
project(SDL_VulkanCodotaku LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/EHsc)
endif()

# --- Vulkan SDK ---
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable not set.")
endif()
set(VULKAN_SDK "$ENV{VULKAN_SDK}")

# --- SDL3 ---
find_package(SDL3 REQUIRED CONFIG PATHS "${VULKAN_SDK}/cmake" NO_DEFAULT_PATH)

# --- Volk (prebuilt) ---
add_library(volk STATIC IMPORTED)
set_target_properties(volk PROPERTIES
    IMPORTED_LOCATION       "${VULKAN_SDK}/Lib/volk.lib"
    IMPORTED_LOCATION_DEBUG "${VULKAN_SDK}/Lib/volkd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_SDK}/Include/Volk"
    INTERFACE_COMPILE_DEFINITIONS "VK_NO_PROTOTYPES"
)

# --- GLM (header-only) ---
add_library(glm INTERFACE IMPORTED)
set_target_properties(glm PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_SDK}/Include"
)

# --- VMA (header-only) ---
add_library(vma INTERFACE IMPORTED)
set_target_properties(vma PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_SDK}/Include/vma"
    INTERFACE_COMPILE_DEFINITIONS "VMA_STATIC_VULKAN_FUNCTIONS=0"
)

# --- Shaderc (prebuilt) ---
add_library(shaderc STATIC IMPORTED)
set_target_properties(shaderc PROPERTIES
    IMPORTED_LOCATION       "${VULKAN_SDK}/Lib/shaderc_combined.lib"
    IMPORTED_LOCATION_DEBUG "${VULKAN_SDK}/Lib/shaderc_combinedd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_SDK}/Include"
)

# --- SPIRV-Reflect (source) ---
add_library(spirv_reflect STATIC "${VULKAN_SDK}/Source/SPIRV-Reflect/spirv_reflect.c")
target_include_directories(spirv_reflect PUBLIC
    "${VULKAN_SDK}/Source/SPIRV-Reflect"
    "${VULKAN_SDK}/Include"
)
target_compile_definitions(spirv_reflect PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)

# --- Library ---
add_library(vkbootstrap STATIC
    src/Context.cpp
    src/Swapchain.cpp
    src/FrameSync.cpp
    src/Allocator.cpp
    src/Shader.cpp
    src/Reflection.cpp
    src/Pipeline.cpp
    src/App.cpp
)
target_include_directories(vkbootstrap PUBLIC src)
target_link_libraries(vkbootstrap PUBLIC volk SDL3::SDL3 glm vma shaderc spirv_reflect)

# --- Example ---
add_executable(example examples/main.cpp)
target_link_libraries(example PRIVATE vkbootstrap)

add_custom_command(TARGET example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VULKAN_SDK}/Bin/SDL3.dll"
        "$<TARGET_FILE_DIR:example>"
    COMMENT "Copying SDL3.dll..."
)
