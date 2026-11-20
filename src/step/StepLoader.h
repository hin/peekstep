// Peekstep - STEP loading and meshing.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace peekstep {

struct Vertex {
    float pos[3];
    float normal[3];
    uint32_t color; // RGBA8, R in the low byte
};

struct ColorInfo {
    std::array<float, 4> rgba;
    size_t faces = 0;
};

struct MaterialInfo {
    std::string name;
    std::string description;
    double density = 0.0;        // 0 if unknown
    std::string densityUnit;
    bool visual = false;         // true: rendering material, false: physical material
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::array<float, 3> bboxMin{0, 0, 0};
    std::array<float, 3> bboxMax{0, 0, 0};

    size_t parts = 0;
    size_t faces = 0;
    size_t unmeshedFaces = 0; // faces the mesher could not triangulate
    size_t healedFaces = 0;   // faces that needed shape healing to mesh
    std::vector<ColorInfo> colors;
    std::vector<MaterialInfo> materials;

    double parseSeconds = 0;    // reading the STEP text
    double transferSeconds = 0; // converting STEP entities to shapes, colors and names
    double meshSeconds = 0;
    double buildSeconds = 0;    // vertex/index buffers, colors, materials
};

// Progress shared between the loading thread and the UI.
struct LoadProgress {
    std::atomic<int> stage{0}; // 0 = reading, 1 = meshing, 2 = building buffers
};

// Reads a STEP file and turns it into a single triangle mesh with per-vertex
// colors. Throws std::runtime_error on failure. Safe to run on a worker thread.
Model loadStep(const std::string& path, LoadProgress* progress = nullptr);

} // namespace peekstep
