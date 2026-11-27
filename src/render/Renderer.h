// Peekstep - Vulkan renderer for the model and the ImGui overlay.
#pragma once

#include "app/Math.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace peekstep {

struct Model;

// Call before glfwInit(): makes GLFW use the Vulkan loader Peekstep links
// to. Otherwise GLFW searches for one by file name, and on macOS does not
// find the loader bundled in Peekstep.app.
void useLinkedVulkanLoader();

// Makes the Vulkan loader find and load the GPU drivers, which the first
// Vulkan call does. Safe to run on another thread before anything else uses Vulkan.
void warmUpVulkanLoader();

// --vulkan-info: prints what the Vulkan loader finds (drivers, GPUs).
// Returns 0 if a Vulkan instance could be created.
int printVulkanInfo();

class Renderer {
public:
    explicit Renderer(GLFWwindow* window); // throws std::runtime_error
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void uploadModel(const Model& model);
    void newImGuiFrame();

    // Draws the model (if any) and the ImGui draw data from ImGui::Render().
    void drawFrame(const Mat4& viewProj, const Vec3& viewDir);

    void requestResize() { resized_ = true; }
    const char* deviceName() const { return deviceName_; }

private:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };
    struct Image {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };
    struct Frame {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void createInstance();
    void pickDevice();
    void createDevice();
    void createSwapchain();
    void destroySwapchain();
    void recreateSwapchain();
    void createRenderPass();
    void createPipeline();
    void createFrames();
    void initImGui();

    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
    void destroyBuffer(Buffer& b);
    Image createImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void destroyImage(Image& img);
    Buffer uploadToDevice(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

    GLFWwindow* window_;
    char deviceName_[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    bool portabilitySubset_ = false;

    VkSurfaceFormatKHR surfaceFormat_{};
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    uint32_t minImageCount_ = 2;
    std::vector<VkImage> swapImages_;
    std::vector<VkImageView> swapViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkSemaphore> renderFinished_; // one per swapchain image
    Image colorTarget_; // multisampled color, resolved into the swapchain image
    Image depthTarget_;
    bool resized_ = false;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;      // opaque triangles
    VkPipeline blendPipeline_ = VK_NULL_HANDLE; // see-through triangles, drawn after the opaque ones

    static constexpr int kFramesInFlight = 2;
    Frame frames_[kFramesInFlight];
    int frameIndex_ = 0;

    Buffer vertexBuffer_;
    Buffer indexBuffer_;
    uint32_t indexCount_ = 0;
    uint32_t opaqueIndexCount_ = 0;
};

} // namespace peekstep
