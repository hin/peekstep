cmake_minimum_required(VERSION 3.21)

project(peekstep VERSION 0.1.0 LANGUAGES CXX)

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
  src/render/Renderer.cpp
  src/step/StepLoader.cpp
)

target_include_directories(peekstep PRIVATE src ${OpenCASCADE_INCLUDE_DIR})
target_compile_definitions(peekstep PRIVATE PEEKSTEP_VERSION="${PROJECT_VERSION}")

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
  TKXCAF TKLCAF TKCAF TKMesh TKBRep TKTopAlgo TKG3d TKMath TKernel
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
