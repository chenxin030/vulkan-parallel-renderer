cmake_minimum_required(VERSION 3.22)

# ═══════════════════════════════════════════════
# VulkanEngine — 基于 Vulkan 1.1 + 扩展的游戏渲染器
# 构建: cmake -G "Visual Studio 17 2022" -B build
# ═══════════════════════════════════════════════

project(VulkanEngine VERSION 1.0.0 LANGUAGES CXX)

# ── C++20 ──
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── MSVC ──
if(MSVC)
    add_compile_options(/W4 /Zm300 /MP /utf-8)
    # 禁用无关警告
    add_compile_options(/wd4100 /wd4189 /wd4505 /wd4996)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ═══════════════════════════════════════════════
# 外部依赖
# ═══════════════════════════════════════════════

# ── Vulkan SDK ──
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan: ${Vulkan_INCLUDE_DIRS}")

# ── GLFW3 (external) ──
set(GLFW3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/GLFW3)
if(NOT EXISTS ${GLFW3_DIR})
    message(FATAL_ERROR "GLFW3 not found at ${GLFW3_DIR}. Copy it from VulkanRenderer/external/GLFW3/")
endif()

find_library(GLFW_LIBRARY
    NAMES glfw3 glfw3dll
    PATHS ${GLFW3_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)
message(STATUS "GLFW: ${GLFW_LIBRARY}")

# GLFW DLL 路径（运行时复制）
set(GLFW_DLL "${GLFW3_DIR}/bin/glfw3.dll")

# ── Slang (Vulkan SDK 自带) ──
find_program(SLANGC_EXECUTABLE slangc
    HINTS ${Vulkan_SDK_PATH}/Bin
          ${Vulkan_INSTALL_PREFIX}/Bin
          ${CMAKE_CURRENT_SOURCE_DIR}/external/slang/bin
)
if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found. Install Slang or check Vulkan SDK.")
endif()
message(STATUS "slangc: ${SLANGC_EXECUTABLE}")

# ── Tracy (可选，profiling) ──
option(ENABLE_TRACY "Enable Tracy profiler" OFF)
if(ENABLE_TRACY)
    set(TRACY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/public)
    if(EXISTS ${TRACY_DIR})
        set(TRACY_FOUND TRUE)
        message(STATUS "Tracy: found at ${TRACY_DIR}")
    else()
        set(TRACY_FOUND FALSE)
        message(WARNING "Tracy requested but not found at ${TRACY_DIR}. Compiling with no-op stubs.")
    endif()
endif()

# ═══════════════════════════════════════════════
# Shader 编译 (Slang → SPIR-V)
# ═══════════════════════════════════════════════

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

# 收集所有 .slang 文件
file(GLOB_RECURSE SLANG_SHADERS ${SHADER_SRC_DIR}/*.slang)

# 为每个 shader 生成编译命令
foreach(SHADER_FILE ${SLANG_SHADERS})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)

    # 根据 NAME_WE 中的 stage 后缀推断 stage
    # 命名规则：<name>_vert.slang / <name>_frag.slang / <name>_comp.slang
    if(SHADER_NAME MATCHES "_vert$")
        set(SHADER_STAGE vertex)
    elseif(SHADER_NAME MATCHES "_frag$")
        set(SHADER_STAGE fragment)
    elseif(SHADER_NAME MATCHES "_comp$")
        set(SHADER_STAGE compute)
    else()
        # 从文件内容推断或使用通用编译
        set(SHADER_STAGE "")
    endif()

    set(SPIRV_OUT ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)

    if(SHADER_STAGE)
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${SLANGC_EXECUTABLE}
                ${SHADER_FILE}
                -o ${SPIRV_OUT}
                -target spirv
                -entry main
                -stage ${SHADER_STAGE}
                -matrix-layout-column-major
                -O2
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME} (${SHADER_STAGE}) → SPIR-V"
        )
    else()
        # 无 stage 指定：让 slangc 自动处理（用于多入口 shader）
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${SLANGC_EXECUTABLE}
                ${SHADER_FILE}
                -o ${SPIRV_OUT}
                -target spirv
                -matrix-layout-column-major
                -O2
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME} → SPIR-V"
        )
    endif()

    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUT})
endforeach()

# Shader 编译目标
if(SPIRV_OUTPUTS)
    add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})
    set_source_files_properties(${SPIRV_OUTPUTS} PROPERTIES GENERATED TRUE)
endif()

# ═══════════════════════════════════════════════
# 源码收集
# ═══════════════════════════════════════════════

file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# ImGui 源文件
file(GLOB IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_impl_vulkan.cpp
)

# ═══════════════════════════════════════════════
# 可执行文件
# ═══════════════════════════════════════════════

add_executable(${PROJECT_NAME}
    ${ENGINE_SOURCES}
    ${IMGUI_SOURCES}
)

# ── Include 路径 ──
# Vulkan SDK 必须优先于 vcpkg
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
    ${Vulkan_INCLUDE_DIRS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/GLFW3/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/external/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/external/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/vma
)

# ── 链接 ──
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    ${GLFW_LIBRARY}
)

# ── 预处理器定义 ──
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # 平台
    VK_USE_PLATFORM_WIN32_KHR
    # 基础
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    # GLM 配置
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_ENABLE_EXPERIMENTAL
    # Vulkan-Hpp 配置
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL
    # 资产路径（编译期常量）
    VK_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/"
    VK_SHADERS_DIR="${SHADER_OUT_DIR}/"
)

# ── 资源文件分组（Visual Studio） ──
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${ENGINE_SOURCES})
source_group("ImGui" FILES ${IMGUI_SOURCES})

if(SPIRV_OUTPUTS)
    source_group("Shaders\\SPIR-V" FILES ${SPIRV_OUTPUTS})
endif()

# ── Pre-link: 杀掉旧进程，避免 LNK1168 ──
add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
    COMMAND taskkill /F /IM $<TARGET_FILE_NAME:${PROJECT_NAME}> 2>nul || cmd /c "exit 0"
    COMMENT "Killing old ${PROJECT_NAME}.exe if running..."
)

# ── Post-build: 复制 GLFW DLL 和 shaders ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GLFW_DLL}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying GLFW DLL..."
)

# ── Tracy (必须在 target 定义之后) ──
if(ENABLE_TRACY AND TRACY_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${TRACY_DIR})
    target_sources(${PROJECT_NAME} PRIVATE ${TRACY_DIR}/TracyClient.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
elseif(ENABLE_TRACY)
    # Tracy 未找到：宏定义允许 Tracy.hpp 中的 no-op stubs
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
endif()

# ═══════════════════════════════════════════════
# 打印配置摘要
# ═══════════════════════════════════════════════

message(STATUS "═══════════════════════════════════")
message(STATUS "VulkanEngine Configuration")
message(STATUS "  C++ Standard: 20")
message(STATUS "  Vulkan SDK:   ${Vulkan_INCLUDE_DIRS}")
message(STATUS "  slange:       ${SLANGC_EXECUTABLE}")
message(STATUS "  Tracy:        ${ENABLE_TRACY}")
message(STATUS "  GLFW:         ${GLFW_LIBRARY}")
message(STATUS "═══════════════════════════════════")
