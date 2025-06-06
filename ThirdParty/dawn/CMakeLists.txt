cmake_minimum_required (VERSION 3.11)

find_package(Python3 REQUIRED)

set(JINJA2_INSTALL_CMD ${Python3_EXECUTABLE} -m pip install --upgrade jinja2>=3.0)

if(${Python3_VERSION} VERSION_GREATER_EQUAL "3.12")
    set(JINJA2_INSTALL_CMD ${JINJA2_INSTALL_CMD} --break-system-packages)
endif()

execute_process(COMMAND ${JINJA2_INSTALL_CMD}
                RESULT_VARIABLE PYTHON_PIP_JINJIA_RESULT)
if(NOT PYTHON_PIP_JINJIA_RESULT EQUAL "0")
    message(FATAL_ERROR "Command '${JINJA2_INSTALL_CMD}' failed with error code ${PYTHON_PIP_JINJIA_RESULT}")
endif()

FetchContent_DeclareShallowGit(
    dawn
    GIT_REPOSITORY  https://dawn.googlesource.com/dawn
    GIT_TAG         db55f595741106be83b823a02ebdc58efbe997f1
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (PLATFORM_WEB)
    set(TINT_BUILD_AS_OTHER_OS ON CACHE BOOL "" FORCE)
else()
    set(TINT_BUILD_AS_OTHER_OS OFF CACHE BOOL "" FORCE)
endif()
set(TINT_BUILD_SPV_READER  ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_READER ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_IR          ON  CACHE BOOL "" FORCE)
if (PLATFORM_WEB)
    set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
else()
    set(TINT_BUILD_HLSL_WRITER ON CACHE BOOL "" FORCE)
endif()

set(TINT_BUILD_GLSL_WRITER     OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_MSL_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR  OFF CACHE BOOL "" FORCE)

set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SAMPLES   OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS     OFF CACHE BOOL "" FORCE)

set(DAWN_BUILD_SAMPLES     OFF CACHE BOOL "" FORCE)
set(DAWN_USE_X11           OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WAYLAND       OFF CACHE BOOL "" FORCE)

set(DAWN_ENABLE_VULKAN     OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D11      OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES   OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
# Dawn's CMake defines DAWN_EMSCRIPTEN_TOOLCHAIN through the option command, which
# sets its default value to OFF instead of ""
set(DAWN_EMSCRIPTEN_TOOLCHAIN "" CACHE STRING "" FORCE)
if (PLATFORM_WEB)
    set(DAWN_ENABLE_D3D12 OFF  CACHE BOOL "" FORCE)
else()
    set(DAWN_ENABLE_D3D12 ON CACHE BOOL "" FORCE)
endif()

set(DAWN_USE_GLFW                OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL             OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WINDOWS_UI          OFF CACHE BOOL "" FORCE) 
set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)

set(DAWN_SPIRV_TOOLS_DIR    "${spirv-tools_SOURCE_DIR}"    CACHE STRING "Directory in which to find SPIRV-Tools"    FORCE)
set(DAWN_SPIRV_HEADERS_DIR  "${SPIRV-Headers_SOURCE_DIR}"  CACHE STRING "Directory in which to find SPIRV-Headers"  FORCE)
set(DAWN_ABSEIL_DIR         "${abseil-cpp_SOURCE_DIR}"     CACHE STRING "Directory in which to find Abseil"         FORCE)

set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(dawn)

if (PLATFORM_WEB)
    find_targets_in_directory(DAWN_TARGETS ${dawn_SOURCE_DIR})
    set_targets_emscripten_properties(${DAWN_TARGETS})
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(tint_api PRIVATE -Wno-switch-default)
    target_compile_options(tint_utils_strconv PRIVATE -Wno-nullability-extension)
endif()

install(FILES "${Dawn_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME dawn-License.txt)
