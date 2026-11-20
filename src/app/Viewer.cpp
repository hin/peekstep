// Peekstep - window, input, UI and background loading.
#include "app/Viewer.h"

#include "platform/Platform.h"
#include "render/Renderer.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace peekstep {
namespace {

std::string fileName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void drawSpinner(ImVec2 center, float radius, float thickness)
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float t = float(ImGui::GetTime());
    const int segments = 30;
    const float start = t * 6.0f;
    dl->PathClear();
    for (int i = 0; i <= segments; ++i) {
        const float a = start + float(i) / float(segments) * 4.5f;
        dl->PathLineTo(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
    dl->PathStroke(IM_COL32(220, 220, 230, 255), 0, thickness);
}

} // namespace

Viewer::Viewer() = default;

Viewer::~Viewer()
{
    renderer_.reset();
    if (ImGui::GetCurrentContext()) {
        if (ImGui::GetIO().BackendPlatformUserData)
            ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

void Viewer::initWindow()
{
    // Inside an app bundle GLFW would change to Contents/Resources, which
    // breaks relative file names given on the command line.
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
    installOpenFileHandler();
    useLinkedVulkanLoader();
    if (!glfwInit())
        throw std::runtime_error("Could not initialize the window system");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(1280, 800, "Peekstep", nullptr, nullptr);
    if (!window_)
        throw std::runtime_error("Could not create a window");
    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, onDrop);
    glfwSetFramebufferSizeCallback(window_, onFramebufferSize);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini files next to the user's STEP files
    ImGui::StyleColorsDark();
    float xscale = 1, yscale = 1;
    glfwGetWindowContentScale(window_, &xscale, &yscale);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(xscale);
    style.FontScaleDpi = xscale;
    style.WindowRounding = 4.0f;

    // Installs ImGui's GLFW callbacks, chaining to the ones set above.
    ImGui_ImplGlfw_InitForVulkan(window_, true);
    renderer_ = std::make_unique<Renderer>(window_);
}

void Viewer::open(const std::string& path)
{
    if (job_ && !job_->done)
        return; // one load at a time

    path_ = path;
    error_.clear();
    loadStart_ = std::chrono::steady_clock::now();
    auto job = std::make_shared<Job>();
    job->path = path;
    job_ = job;
    updateTitle();

    std::thread([job] {
        try {
            Model m = loadStep(job->path, &job->progress);
            std::lock_guard lock(job->mutex);
            job->model = std::move(m);
        } catch (const std::exception& e) {
            std::lock_guard lock(job->mutex);
            job->error = e.what();
        } catch (...) {
            std::lock_guard lock(job->mutex);
            job->error = "Unknown error while loading";
        }
        job->done = true;
        glfwPostEmptyEvent(); // wake up the UI thread
    }).detach();
}

void Viewer::pollLoad()
{
    if (!job_ || !job_->done)
        return;
    std::shared_ptr<Job> job = std::move(job_);
    std::lock_guard lock(job->mutex);
    if (!job->error.empty()) {
        error_ = job->error;
        return;
    }
    Model& m = *job->model;
    renderer_->uploadModel(m);
    loadSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - loadStart_).count();
    camera_.setView(Camera::View::Iso);
    camera_.fit(Vec3(m.bboxMin[0], m.bboxMin[1], m.bboxMin[2]), Vec3(m.bboxMax[0], m.bboxMax[1], m.bboxMax[2]));

    // Keep the stats but drop the geometry: it lives on the GPU now.
    triangleCount_ = m.indices.size() / 3;
    std::vector<Vertex>().swap(m.vertices);
    std::vector<uint32_t>().swap(m.indices);
    info_ = std::move(m);
}

void Viewer::handleInput()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        const ImVec2 d = io.MouseDelta;
        const bool panDrag = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                          || (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyShift);
        if (panDrag)
            camera_.pan(d.x, d.y, io.DisplaySize.y);
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            camera_.orbit(d.x, d.y);
        if (io.MouseWheel != 0)
            camera_.zoom(io.MouseWheel);
    }
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_F) && info_)
            camera_.fit(Vec3(info_->bboxMin[0], info_->bboxMin[1], info_->bboxMin[2]),
                        Vec3(info_->bboxMax[0], info_->bboxMax[1], info_->bboxMax[2]));
        if (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0))
            camera_.setView(Camera::View::Iso);
        if (ImGui::IsKeyPressed(ImGuiKey_1) || ImGui::IsKeyPressed(ImGuiKey_Keypad1))
            camera_.setView(io.KeyCtrl ? Camera::View::Back : Camera::View::Front);
        if (ImGui::IsKeyPressed(ImGuiKey_3) || ImGui::IsKeyPressed(ImGuiKey_Keypad3))
            camera_.setView(io.KeyCtrl ? Camera::View::Left : Camera::View::Right);
        if (ImGui::IsKeyPressed(ImGuiKey_7) || ImGui::IsKeyPressed(ImGuiKey_Keypad7))
            camera_.setView(io.KeyCtrl ? Camera::View::Bottom : Camera::View::Top);
        if (ImGui::IsKeyPressed(ImGuiKey_Tab))
            showPanel_ = !showPanel_;
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            showAbout_ = !showAbout_;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && showAbout_)
            showAbout_ = false;
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)))
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void Viewer::drawUi()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float pad = 10.0f * ImGui::GetStyle().FontScaleDpi;

    if (path_.empty()) {
        const char* msg = "Drop a STEP file here, or start Peekstep with a file name.";
        const ImVec2 size = ImGui::CalcTextSize(msg);
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(vp->Pos.x + (vp->Size.x - size.x) * 0.5f, vp->Pos.y + (vp->Size.y - size.y) * 0.5f),
            IM_COL32(200, 200, 210, 255), msg);
        return;
    }

    if (!showPanel_)
        return;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + pad, vp->Pos.y + pad));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(vp->Size.x * 0.4f, vp->Size.y - 2 * pad));
    ImGui::SetNextWindowBgAlpha(0.85f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("Peekstep", nullptr, flags)) {
        ImGui::TextUnformatted(fileName(path_).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("?"))
            showAbout_ = !showAbout_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("About Peekstep (F1)");

        if (!error_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 120, 110, 255));
            ImGui::TextWrapped("Error: %s", error_.c_str());
            ImGui::PopStyleColor();
        }

        if (info_) {
            const Model& m = *info_;
            ImGui::Separator();
            ImGui::Text("Parts: %zu   Faces: %zu   Triangles: %zu", m.parts, m.faces, triangleCount_);
            ImGui::Text("Size: %.2f x %.2f x %.2f mm", m.bboxMax[0] - m.bboxMin[0], m.bboxMax[1] - m.bboxMin[1],
                        m.bboxMax[2] - m.bboxMin[2]);
            ImGui::Text("Loaded in %.2f s", loadSeconds_);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Parse %.2f s\nConvert %.2f s\nMesh %.2f s\nBuild %.2f s", m.parseSeconds,
                                  m.transferSeconds, m.meshSeconds, m.buildSeconds);

            ImGui::Separator();
            if (ImGui::Button("Iso")) camera_.setView(Camera::View::Iso);
            ImGui::SameLine();
            if (ImGui::Button("Front")) camera_.setView(Camera::View::Front);
            ImGui::SameLine();
            if (ImGui::Button("Right")) camera_.setView(Camera::View::Right);
            ImGui::SameLine();
            if (ImGui::Button("Top")) camera_.setView(Camera::View::Top);
            ImGui::SameLine();
            if (ImGui::Button("Fit"))
                camera_.fit(Vec3(m.bboxMin[0], m.bboxMin[1], m.bboxMin[2]), Vec3(m.bboxMax[0], m.bboxMax[1], m.bboxMax[2]));

            if (ImGui::CollapsingHeader("Colors")) {
                int i = 0;
                for (const ColorInfo& c : m.colors) {
                    ImGui::PushID(i++);
                    ImGui::ColorButton("##c", ImVec4(c.rgba[0], c.rgba[1], c.rgba[2], 1.0f),
                                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
                    ImGui::SameLine();
                    ImGui::Text("#%02X%02X%02X  %zu faces", int(std::lround(c.rgba[0] * 255)),
                                int(std::lround(c.rgba[1] * 255)), int(std::lround(c.rgba[2] * 255)), c.faces);
                    ImGui::PopID();
                }
            }
            if (ImGui::CollapsingHeader("Materials")) {
                if (m.materials.empty())
                    ImGui::TextDisabled("No materials in this file");
                for (const MaterialInfo& mat : m.materials) {
                    ImGui::BulletText("%s%s", mat.name.c_str(), mat.visual ? " (appearance)" : "");
                    if (!mat.description.empty())
                        ImGui::TextDisabled("    %s", mat.description.c_str());
                    if (mat.density > 0)
                        ImGui::TextDisabled("    Density %g %s", mat.density, mat.densityUnit.c_str());
                }
            }
            if (ImGui::CollapsingHeader("Controls")) {
                ImGui::TextUnformatted("Left drag: rotate\n"
                                       "Right/middle drag, Shift+left: pan\n"
                                       "Wheel: zoom\n"
                                       "F: fit   0: iso   1/3/7: front/right/top\n"
                                       "Ctrl+1/3/7: back/left/bottom\n"
                                       "Tab: hide panel   F1: about   Esc: quit");
            }
        }
    }
    ImGui::End();
}

void Viewer::drawLoadingOverlay()
{
    if (!job_)
        return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
    drawSpinner(center, 22.0f * scale, 4.0f * scale);

    static const char* stages[] = {"Reading", "Meshing", "Preparing"};
    const int stage = std::clamp(job_->progress.stage.load(), 0, 2);
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - loadStart_).count();
    char text[512];
    std::snprintf(text, sizeof(text), "%s %s  (%.1f s)", stages[stage], fileName(job_->path).c_str(), elapsed);
    const ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::GetForegroundDrawList()->AddText(ImVec2(center.x - size.x * 0.5f, center.y + 36.0f * scale),
                                            IM_COL32(220, 220, 230, 255), text);
}

// "Peekstep vX.Y.Z" in the lower right corner; clicking it opens About.
void Viewer::drawVersionLabel()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float pad = 6.0f * ImGui::GetStyle().FontScaleDpi;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - pad, vp->Pos.y + vp->Size.y - pad), ImGuiCond_Always,
                            ImVec2(1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                                 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                                 | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##version", nullptr, flags)) {
        const char* label = "Peekstep v" PEEKSTEP_VERSION;
        const ImVec2 size = ImGui::CalcTextSize(label);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##about", size))
            showAbout_ = !showAbout_;
        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::GetWindowDrawList()->AddText(pos, hovered ? IM_COL32(230, 230, 240, 255) : IM_COL32(140, 140, 150, 200),
                                            label);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void Viewer::drawAbout()
{
    if (!showAbout_)
        return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.95f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("About Peekstep", &showAbout_, flags)) {
        ImGui::TextUnformatted("Peekstep");
        ImGui::TextUnformatted("A small, fast-starting STEP file viewer.");
        ImGui::Spacing();
        ImGui::Text("Version %s", PEEKSTEP_VERSION);
        ImGui::TextDisabled("Build %s, %s", PEEKSTEP_BUILD, platformName());
        ImGui::Spacing();
        const char* url = "https://github.com/hin/peekstep";
        if (ImGui::TextLink(url))
            openUrl(url);
        ImGui::Spacing();
        ImGui::TextUnformatted("Developed with AI assistance.");
        ImGui::TextDisabled("BSD 2-Clause license. Uses OpenCASCADE, GLFW,\n"
                            "Dear ImGui and Vulkan; see the license files.");
    }
    ImGui::End();
}

void Viewer::updateTitle()
{
    if (!window_)
        return;
    const std::string title = path_.empty() ? std::string("Peekstep") : fileName(path_) + " - Peekstep";
    glfwSetWindowTitle(window_, title.c_str());
}

int Viewer::run()
{
    // The window and Vulkan come up while a file given on the command line
    // is already loading on its own thread.
    initWindow();
    updateTitle();

    int idleFrames = 0;
    while (!glfwWindowShouldClose(window_)) {
        // Redraw continuously only while something moves; otherwise sleep
        // until there is input. A few extra frames let ImGui settle hovers.
        const bool busy = job_ != nullptr;
        if (busy)
            glfwWaitEventsTimeout(1.0 / 30.0);
        else if (idleFrames > 2)
            glfwWaitEvents();
        else
            glfwPollEvents();

        // Files opened from Finder on macOS (empty elsewhere). Only the last
        // one is shown; open() ignores it while another file is loading.
        const std::vector<std::string> opened = takeOpenedFiles();
        if (!opened.empty())
            open(opened.back());
        pollLoad();

        renderer_->newImGuiFrame();
        handleInput();
        drawUi();
        drawVersionLabel();
        drawLoadingOverlay();
        drawAbout();
        ImGui::Render();

        int w = 0, h = 0;
        glfwGetFramebufferSize(window_, &w, &h);
        const float aspect = h > 0 ? float(w) / float(h) : 1.0f;
        renderer_->drawFrame(camera_.viewProj(aspect), camera_.forward());

        const ImGuiIO& io = ImGui::GetIO();
        const bool active = busy || io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || io.MouseWheel != 0
                         || ImGui::IsAnyMouseDown() || ImGui::IsAnyItemActive();
        idleFrames = active ? 0 : idleFrames + 1;
    }
    return 0;
}

void Viewer::onDrop(GLFWwindow* window, int count, const char** paths)
{
    if (count > 0)
        static_cast<Viewer*>(glfwGetWindowUserPointer(window))->open(paths[0]);
}

void Viewer::onFramebufferSize(GLFWwindow* window, int, int)
{
    auto* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
    if (viewer->renderer_)
        viewer->renderer_->requestResize();
}

} // namespace peekstep
