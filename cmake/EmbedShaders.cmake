# Compiles GLSL shaders to SPIR-V and embeds them as C arrays in a header
# per shader (e.g. mesh.vert -> <build>/shaders/mesh_vert.h, array mesh_vert).

find_program(GLSLANG_VALIDATOR
  NAMES glslangValidator glslang
  HINTS
    "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/glslang"
    "$ENV{VULKAN_SDK}/bin"
  REQUIRED
)

function(peekstep_embed_shaders target)
  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  file(MAKE_DIRECTORY "${out_dir}")
  set(headers)
  foreach(src IN LISTS ARGN)
    get_filename_component(name "${src}" NAME)
    string(REPLACE "." "_" var "${name}")
    set(header "${out_dir}/${var}.h")
    add_custom_command(
      OUTPUT "${header}"
      COMMAND "${GLSLANG_VALIDATOR}" -V --vn "${var}" -o "${header}" "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
      COMMENT "Compiling shader ${name}"
      VERBATIM
    )
    list(APPEND headers "${header}")
  endforeach()
  add_custom_target(${target}_gen DEPENDS ${headers})
  add_library(${target} INTERFACE)
  add_dependencies(${target} ${target}_gen)
  target_include_directories(${target} INTERFACE "${out_dir}")
endfunction()
