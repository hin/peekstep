// Peekstep - window, input, UI and background loading.
#pragma once

#include "app/Camera.h"
#include "step/StepLoader.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct GLFWwindow;

namespace peekstep {

class Renderer;

class Viewer {
public:
    Viewer();
    ~Viewer();

    // Starts loading in the background; can be called before run().
    void open(const std::string& path);
    int run();
    bool loading() const { return job_ != nullptr; }

private:
    // State shared with a loading thread. Kept alive by the thread itself,
    // so quitting during a load never waits for it.
    struct Job {
        std::string path;
        LoadProgress progress;
        std::atomic<bool> done{false};
        std::mutex mutex;
        std::optional<Model> model;
        std::string error;
    };

    void initWindow();
    void pollLoad();
    void handleInput();
    void drawUi();
    void drawLoadingOverlay();
    void drawAbout();
    void drawVersionLabel();
    void updateTitle();

    static void onDrop(GLFWwindow* window, int count, const char** paths);
    static void onFramebufferSize(GLFWwindow* window, int w, int h);

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<Renderer> renderer_;
    Camera camera_;

    std::shared_ptr<Job> job_;
    std::chrono::steady_clock::time_point loadStart_;
    std::string path_;
    std::string error_;
    std::optional<Model> info_; // stats/colors/materials of the shown model (no geometry)
    size_t triangleCount_ = 0;
    double loadSeconds_ = 0;
    bool showPanel_ = true;
    bool showAbout_ = false;
};

} // namespace peekstep
