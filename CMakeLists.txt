cmake_minimum_required(VERSION 3.21)

project(peekstep VERSION 0.1.0 LANGUAGES CXX)
if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PEEKSTEP_STATIC_RUNTIME "Link the C/C++ runtime statically where the platform allows it" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(OpenCASCADE CONFIG REQUIRED)

include(cmake/EmbedShaders.cmake)
peekstep_embed_shaders(peekstep_shaders
  shaders/mesh.vert
  shaders/mesh.frag
)

add_executable(peekstep
  src/main.cpp
  src/app/Camera.cpp
  src/app/Viewer.cpp
  src/platform/Platform.cpp
  src/render/Renderer.cpp
  src/step/StepLoader.cpp
)
if(APPLE)
  target_sources(peekstep PRIVATE src/platform/MacOpenFiles.mm)
  target_link_libraries(peekstep PRIVATE "-framework Cocoa")
endif()

# Build identifier shown in the About box: the git commit and the date of
# configuration, unless given (e.g. -DPEEKSTEP_BUILD=...).
if(NOT PEEKSTEP_BUILD)
  find_package(Git QUIET)
  set(_peekstep_commit "unknown")
  if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _peekstep_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT _peekstep_commit)
      set(_peekstep_commit "unknown")
    endif()
  endif()
  string(TIMESTAMP _peekstep_date "%Y-%m-%d" UTC)
  set(PEEKSTEP_BUILD "${_peekstep_commit} ${_peekstep_date}")
endif()

target_include_directories(peekstep PRIVATE src ${OpenCASCADE_INCLUDE_DIR})
# _USE_MATH_DEFINES: OpenCASCADE headers use M_PI, which MinGW/MSVC only
# define on request in strict C++ mode.
target_compile_definitions(peekstep PRIVATE PEEKSTEP_VERSION="${PROJECT_VERSION}"
  PEEKSTEP_BUILD="${PEEKSTEP_BUILD}" _USE_MATH_DEFINES)

# Only the OpenCASCADE toolkits needed to read STEP with colors and to mesh.
# OCCT 7.8+ merged the STEP toolkits into TKDESTEP.
if(TARGET TKDESTEP)
  set(PEEKSTEP_OCCT_STEP TKDESTEP)
else()
  set(PEEKSTEP_OCCT_STEP TKXDESTEP TKSTEP)
endif()
target_link_libraries(peekstep PRIVATE
  peekstep_shaders
  imgui::imgui
  glfw
  Vulkan::Vulkan
  ${PEEKSTEP_OCCT_STEP}
  TKXSBase TKXCAF TKLCAF TKCAF TKMesh TKShHealing TKBRep TKTopAlgo TKG3d TKMath TKernel
)

if(MSVC)
  target_compile_options(peekstep PRIVATE /W3 /utf-8)
else()
  target_compile_options(peekstep PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# Single self-contained binary: pull in the compiler runtime too.
if(PEEKSTEP_STATIC_RUNTIME)
  if(MINGW)
    target_link_options(peekstep PRIVATE -static)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(peekstep PRIVATE -static-libstdc++ -static-libgcc)
  elseif(MSVC)
    set_property(TARGET peekstep PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if(WIN32)
  # GUI app: no console window when started from Explorer.
  set_target_properties(peekstep PROPERTIES WIN32_EXECUTABLE ON)
  if(MINGW)
    target_link_options(peekstep PRIVATE -mwindows)
  endif()
endif()

install(TARGETS peekstep RUNTIME DESTINATION bin)
