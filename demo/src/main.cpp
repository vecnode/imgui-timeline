#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#ifndef USE_VULKAN_BACKEND
  #include <backends/imgui_impl_opengl3.h>
  #include <glad/glad.h>
#else
  #include <backends/imgui_impl_vulkan.h>
  #include <vulkan/vulkan.h>
#endif
#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "imgui_timeline.h"

#ifdef USE_VULKAN_BACKEND
  // Minimal Vulkan helper structures
  struct VulkanState {
      VkInstance instance = VK_NULL_HANDLE;
      VkPhysicalDevice physical_device = VK_NULL_HANDLE;
      VkDevice device = VK_NULL_HANDLE;
      VkQueue queue = VK_NULL_HANDLE;
      VkCommandPool cmd_pool = VK_NULL_HANDLE;
      VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
      VkSurfaceKHR surface = VK_NULL_HANDLE;
      VkSwapchainKHR swapchain = VK_NULL_HANDLE;
      std::vector<VkImage> swapchain_images;
      std::vector<VkImageView> swapchain_image_views;
      std::vector<VkFramebuffer> framebuffers;
      VkRenderPass render_pass = VK_NULL_HANDLE;
      VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
      VkExtent2D swapchain_extent = {1280, 720};
      uint32_t queue_family = 0;
      uint32_t current_frame = 0;
      std::vector<VkCommandBuffer> cmd_buffers;
      std::vector<VkSemaphore> image_acquired_semaphores;
      std::vector<VkSemaphore> render_complete_semaphores;
      std::vector<VkFence> in_flight_fences;
  };
  static VulkanState g_vk_state;

  static bool vk_check(VkResult result, const char* context) {
      if (result != VK_SUCCESS) {
          std::fprintf(stderr, "Vulkan error in %s: %d\n", context, (int)result);
          return false;
      }
      return true;
  }

  static void imgui_vulkan_check_result(VkResult result) {
      if (result != VK_SUCCESS) {
          std::fprintf(stderr, "ImGui Vulkan error: %d\n", (int)result);
      }
  }

  // Simplified Vulkan initialization
  static bool init_vulkan_core(int width, int height) {
      // Load Vulkan functions
      if (!glfwVulkanSupported()) {
          std::fprintf(stderr, "Vulkan is not supported\n");
          return false;
      }

      // Create instance
      VkApplicationInfo app_info = {};
      app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      app_info.pApplicationName = "ImGui Timeline Demo";
      app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
      app_info.pEngineName = "No Engine";
      app_info.apiVersion = VK_API_VERSION_1_0;

      // Get required extensions from GLFW
      uint32_t glfw_ext_count = 0;
      const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

      VkInstanceCreateInfo create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      create_info.pApplicationInfo = &app_info;
      create_info.enabledExtensionCount = glfw_ext_count;
      create_info.ppEnabledExtensionNames = glfw_extensions;

      if (!vk_check(vkCreateInstance(&create_info, nullptr, &g_vk_state.instance), "vkCreateInstance")) {
          return false;
      }

      // Find physical device
      uint32_t device_count = 0;
      vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, nullptr);
      if (device_count == 0) {
          std::fprintf(stderr, "No physical devices found\n");
          return false;
      }

      std::vector<VkPhysicalDevice> devices(device_count);
      vkEnumeratePhysicalDevices(g_vk_state.instance, &device_count, devices.data());
      g_vk_state.physical_device = devices[0];

      // Find graphics queue family
      uint32_t qfamily_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(g_vk_state.physical_device, &qfamily_count, nullptr);
      std::vector<VkQueueFamilyProperties> qfamilies(qfamily_count);
      vkGetPhysicalDeviceQueueFamilyProperties(g_vk_state.physical_device, &qfamily_count, qfamilies.data());

      int graphics_family = -1;
      for (uint32_t i = 0; i < qfamily_count; i++) {
          if (qfamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
              graphics_family = i;
              break;
          }
      }

      if (graphics_family < 0) {
          std::fprintf(stderr, "No graphics queue family found\n");
          return false;
      }

      g_vk_state.queue_family = graphics_family;

      // Create device
      float queue_priority = 1.0f;
      VkDeviceQueueCreateInfo qcreate_info = {};
      qcreate_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      qcreate_info.queueFamilyIndex = graphics_family;
      qcreate_info.queueCount = 1;
      qcreate_info.pQueuePriorities = &queue_priority;

      const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

      VkPhysicalDeviceFeatures device_features = {};

      VkDeviceCreateInfo device_create_info = {};
      device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      device_create_info.queueCreateInfoCount = 1;
      device_create_info.pQueueCreateInfos = &qcreate_info;
      device_create_info.enabledExtensionCount = 1;
      device_create_info.ppEnabledExtensionNames = device_extensions;
      device_create_info.pEnabledFeatures = &device_features;

      if (!vk_check(vkCreateDevice(g_vk_state.physical_device, &device_create_info, nullptr, &g_vk_state.device), "vkCreateDevice")) {
          return false;
      }

      vkGetDeviceQueue(g_vk_state.device, graphics_family, 0, &g_vk_state.queue);

      // Create command pool
      VkCommandPoolCreateInfo pool_info = {};
      pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pool_info.queueFamilyIndex = graphics_family;
      pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

      if (!vk_check(vkCreateCommandPool(g_vk_state.device, &pool_info, nullptr, &g_vk_state.cmd_pool), "vkCreateCommandPool")) {
          return false;
      }

      return true;
  }

  static bool init_vulkan_swapchain(GLFWwindow* window) {
      // Create surface from GLFW window
      if (!vk_check((VkResult)glfwCreateWindowSurface(g_vk_state.instance, window, nullptr, &g_vk_state.surface), "glfwCreateWindowSurface")) {
          return false;
      }

      // Get surface capabilities
      VkSurfaceCapabilitiesKHR capabilities;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk_state.physical_device, g_vk_state.surface, &capabilities);

      g_vk_state.swapchain_extent = capabilities.currentExtent;
      if (g_vk_state.swapchain_extent.width == 0xFFFFFFFF) {
          g_vk_state.swapchain_extent.width = 1280;
          g_vk_state.swapchain_extent.height = 720;
      }

      // Get surface formats
      uint32_t format_count = 0;
      vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk_state.physical_device, g_vk_state.surface, &format_count, nullptr);
      std::vector<VkSurfaceFormatKHR> formats(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk_state.physical_device, g_vk_state.surface, &format_count, formats.data());

      VkSurfaceFormatKHR surface_format = formats[0];
      for (const auto& fmt : formats) {
          if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
              surface_format = fmt;
              break;
          }
      }

      g_vk_state.swapchain_format = surface_format.format;

      // Get present modes
      uint32_t present_count = 0;
      vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk_state.physical_device, g_vk_state.surface, &present_count, nullptr);
      std::vector<VkPresentModeKHR> present_modes(present_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk_state.physical_device, g_vk_state.surface, &present_count, present_modes.data());

      VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // vsync, stable for demo

      // Create swapchain
      uint32_t image_count = capabilities.minImageCount + 1;
      if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
          image_count = capabilities.maxImageCount;
      }

      VkSwapchainCreateInfoKHR swapchain_info = {};
      swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      swapchain_info.surface = g_vk_state.surface;
      swapchain_info.minImageCount = image_count;
      swapchain_info.imageFormat = surface_format.format;
      swapchain_info.imageColorSpace = surface_format.colorSpace;
      swapchain_info.imageExtent = g_vk_state.swapchain_extent;
      swapchain_info.imageArrayLayers = 1;
      swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain_info.preTransform = capabilities.currentTransform;
      swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      swapchain_info.presentMode = present_mode;
      swapchain_info.clipped = VK_TRUE;

      if (!vk_check(vkCreateSwapchainKHR(g_vk_state.device, &swapchain_info, nullptr, &g_vk_state.swapchain), "vkCreateSwapchainKHR")) {
          return false;
      }

      // Retrieve swapchain images
      vkGetSwapchainImagesKHR(g_vk_state.device, g_vk_state.swapchain, &image_count, nullptr);
      g_vk_state.swapchain_images.resize(image_count);
      vkGetSwapchainImagesKHR(g_vk_state.device, g_vk_state.swapchain, &image_count, g_vk_state.swapchain_images.data());

      // Create image views
      g_vk_state.swapchain_image_views.resize(image_count);
      for (uint32_t i = 0; i < image_count; i++) {
          VkImageViewCreateInfo view_info = {};
          view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
          view_info.image = g_vk_state.swapchain_images[i];
          view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
          view_info.format = surface_format.format;
          view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
          view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
          view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
          view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
          view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          view_info.subresourceRange.baseMipLevel = 0;
          view_info.subresourceRange.levelCount = 1;
          view_info.subresourceRange.baseArrayLayer = 0;
          view_info.subresourceRange.layerCount = 1;

          if (!vk_check(vkCreateImageView(g_vk_state.device, &view_info, nullptr, &g_vk_state.swapchain_image_views[i]), "vkCreateImageView")) {
              return false;
          }
      }

      // Create render pass
      VkAttachmentDescription color_attachment = {};
      color_attachment.format = surface_format.format;
      color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      VkAttachmentReference color_attachment_ref = {};
      color_attachment_ref.attachment = 0;
      color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &color_attachment_ref;

      VkRenderPassCreateInfo render_pass_info = {};
      render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      render_pass_info.attachmentCount = 1;
      render_pass_info.pAttachments = &color_attachment;
      render_pass_info.subpassCount = 1;
      render_pass_info.pSubpasses = &subpass;

      // Subpass dependency: ensure color attachment output stage waits for image acquisition
      VkSubpassDependency dependency = {};
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.srcAccessMask = 0;
      dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      render_pass_info.dependencyCount = 1;
      render_pass_info.pDependencies = &dependency;

      if (!vk_check(vkCreateRenderPass(g_vk_state.device, &render_pass_info, nullptr, &g_vk_state.render_pass), "vkCreateRenderPass")) {
          return false;
      }

      // Create framebuffers
      g_vk_state.framebuffers.resize(image_count);
      for (uint32_t i = 0; i < image_count; i++) {
          VkImageView attachments[] = {g_vk_state.swapchain_image_views[i]};
          VkFramebufferCreateInfo framebuffer_info = {};
          framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
          framebuffer_info.renderPass = g_vk_state.render_pass;
          framebuffer_info.attachmentCount = 1;
          framebuffer_info.pAttachments = attachments;
          framebuffer_info.width = g_vk_state.swapchain_extent.width;
          framebuffer_info.height = g_vk_state.swapchain_extent.height;
          framebuffer_info.layers = 1;

          if (!vk_check(vkCreateFramebuffer(g_vk_state.device, &framebuffer_info, nullptr, &g_vk_state.framebuffers[i]), "vkCreateFramebuffer")) {
              return false;
          }
      }

      // Create synchronization primitives and command buffers
      // Create descriptor pool for ImGui
      VkDescriptorPoolSize pool_sizes[] = {
          {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
          {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      };

      VkDescriptorPoolCreateInfo pool_create_info = {};
      pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
      pool_create_info.maxSets = 1000;
      pool_create_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
      pool_create_info.pPoolSizes = pool_sizes;

      if (!vk_check(vkCreateDescriptorPool(g_vk_state.device, &pool_create_info, nullptr, &g_vk_state.descriptor_pool), "vkCreateDescriptorPool")) {
          return false;
      }

      g_vk_state.image_acquired_semaphores.resize(2);
      g_vk_state.render_complete_semaphores.resize(2);
      g_vk_state.in_flight_fences.resize(2);
      g_vk_state.cmd_buffers.resize(image_count);

      VkSemaphoreCreateInfo semaphore_info = {};
      semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

      VkFenceCreateInfo fence_info = {};
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

      for (size_t i = 0; i < 2; i++) {
          vkCreateSemaphore(g_vk_state.device, &semaphore_info, nullptr, &g_vk_state.image_acquired_semaphores[i]);
          vkCreateSemaphore(g_vk_state.device, &semaphore_info, nullptr, &g_vk_state.render_complete_semaphores[i]);
          vkCreateFence(g_vk_state.device, &fence_info, nullptr, &g_vk_state.in_flight_fences[i]);
      }

      VkCommandBufferAllocateInfo cmd_buffer_info = {};
      cmd_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmd_buffer_info.commandPool = g_vk_state.cmd_pool;
      cmd_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_buffer_info.commandBufferCount = image_count;

      if (!vk_check(vkAllocateCommandBuffers(g_vk_state.device, &cmd_buffer_info, g_vk_state.cmd_buffers.data()), "vkAllocateCommandBuffers")) {
          return false;
      }

      return true;
  }

  static void cleanup_vulkan() {
      if (g_vk_state.device == VK_NULL_HANDLE) return;

      vkDeviceWaitIdle(g_vk_state.device);

      for (size_t i = 0; i < 2; i++) {
          vkDestroySemaphore(g_vk_state.device, g_vk_state.image_acquired_semaphores[i], nullptr);
          vkDestroySemaphore(g_vk_state.device, g_vk_state.render_complete_semaphores[i], nullptr);
          vkDestroyFence(g_vk_state.device, g_vk_state.in_flight_fences[i], nullptr);
      }

      for (auto fb : g_vk_state.framebuffers) {
          vkDestroyFramebuffer(g_vk_state.device, fb, nullptr);
      }

      vkDestroyRenderPass(g_vk_state.device, g_vk_state.render_pass, nullptr);

      for (auto view : g_vk_state.swapchain_image_views) {
          vkDestroyImageView(g_vk_state.device, view, nullptr);
      }

      vkDestroySwapchainKHR(g_vk_state.device, g_vk_state.swapchain, nullptr);
      vkDestroySurfaceKHR(g_vk_state.instance, g_vk_state.surface, nullptr);
      vkDestroyDescriptorPool(g_vk_state.device, g_vk_state.descriptor_pool, nullptr);
      vkDestroyCommandPool(g_vk_state.device, g_vk_state.cmd_pool, nullptr);
      vkDestroyDevice(g_vk_state.device, nullptr);
      vkDestroyInstance(g_vk_state.instance, nullptr);
  }
#endif

static void glfw_error_callback(int error, const char* description){
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(){
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

#ifndef USE_VULKAN_BACKEND
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  #if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Timeline Demo (OpenGL3)", nullptr, nullptr);
#else
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Timeline Demo (Vulkan)", nullptr, nullptr);
#endif
    if (!window){ glfwTerminate(); return 1; }

#ifndef USE_VULKAN_BACKEND
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::fprintf(stderr, "Failed to init GLAD\n");
        return 1;
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    // Reduce font size by 2px
    ImGui::GetIO().FontGlobalScale = 0.85f;

#ifndef USE_VULKAN_BACKEND
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#else
    ImGui_ImplGlfw_InitForVulkan(window, true);
#endif
#ifndef USE_VULKAN_BACKEND
    ImGui_ImplOpenGL3_Init("#version 330");
#else
    // Initialize Vulkan backend
    std::fprintf(stderr, "Initializing Vulkan core...\n");
    if (!init_vulkan_core(1280, 720)) {
        std::fprintf(stderr, "Failed to initialize Vulkan core\n");
        return 1;
    }

    std::fprintf(stderr, "Initializing Vulkan swapchain...\n");
    if (!init_vulkan_swapchain(window)) {
        std::fprintf(stderr, "Failed to initialize Vulkan swapchain\n");
        cleanup_vulkan();
        return 1;
    }

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_vk_state.instance;
    init_info.PhysicalDevice = g_vk_state.physical_device;
    init_info.Device = g_vk_state.device;
    init_info.QueueFamily = g_vk_state.queue_family;
    init_info.Queue = g_vk_state.queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = g_vk_state.descriptor_pool;
    init_info.MinImageCount = static_cast<uint32_t>(g_vk_state.swapchain_images.size());
    init_info.ImageCount = static_cast<uint32_t>(g_vk_state.swapchain_images.size());
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = imgui_vulkan_check_result;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_PipelineInfo pipeline_info = {};
    pipeline_info.RenderPass = g_vk_state.render_pass;
    ImGui_ImplVulkan_CreateMainPipeline(&pipeline_info);
#endif

    using namespace ImGuiX;
    TimelineConfig cfg;
    cfg.min_time = 0.0;
    cfg.max_time = 120.0;
    cfg.view_min = 0.0;
    cfg.view_max = 60.0;
    cfg.label_width = 270.f;
    cfg.ruler_height = 28.f;
    cfg.timeline_width = -1.0f;

    Timeline* timeline = nullptr;
    std::vector<TimelineTrack>* tracks = nullptr;
    bool initialized = false;

    while (!glfwWindowShouldClose(window)){
        // Initialize on first frame
        if (!initialized) {
            timeline = new Timeline(cfg);
            tracks = new std::vector<TimelineTrack>();

            tracks->push_back(TimelineTrack{"Video", 28.f, 20.f, 100.f, false, false, false,
                { {1,  0.0,  8.5, IM_COL32(255,120,120,255), "Intro", false}, {2, 10.0, 22.0, IM_COL32(255,180,120,255), "Scene A", false} }});
            tracks->push_back(TimelineTrack{"Audio", 26.f, 20.f, 100.f, false, false, false,
                { {3,  5.0, 18.0, IM_COL32(120,200,255,255), "Music", false} }});
            tracks->push_back(TimelineTrack{"Effects", 26.f, 20.f, 100.f, false, false, false,
                { {4, 12.0, 15.0, IM_COL32(140,220,140,255), "Particles", false}, {5, 16.0, 19.0, IM_COL32(200,120,220,255), "Glow", false} }});
            tracks->push_back(TimelineTrack{"Titles", 26.f, 20.f, 100.f, false, false, false,
                { {6, 2.0, 5.0, IM_COL32(255,200,100,255), "Fade In", false}, {7, 80.0, 88.0, IM_COL32(100,200,255,255), "Fade Out", false}, {8, 55.0, 60.0, IM_COL32(200,100,255,255), "Peak", false} }});
            tracks->push_back(TimelineTrack{"Motion", 26.f, 20.f, 100.f, false, false, false,
                { {9, 3.0, 12.0, IM_COL32(255,150,150,255), "Pan Left", false}, {10, 40.0, 55.0, IM_COL32(150,255,150,255), "Zoom In", false} }});
            tracks->push_back(TimelineTrack{"Markers", 26.f, 20.f, 100.f, false, false, false,
                { {11, 30.0, 30.0, IM_COL32(150,150,255,255), "Act 1", false}, {12, 60.0, 60.0, IM_COL32(255,255,150,255), "Act 2", false} }});
            initialized = true;
        }

        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
#ifndef USE_VULKAN_BACKEND
        ImGui_ImplOpenGL3_NewFrame();
#else
        ImGui_ImplVulkan_NewFrame();
#endif
        ImGui::NewFrame();

        static TimelineEdit last_edit{};
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::Begin("Timeline Demo", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextUnformatted("Zoom: Ctrl/Alt+Wheel  Pan: Wheel or Middle/Right-Drag  Select: Click  Move: Drag  Resize: Drag edges");
        if (last_edit.changed && (last_edit.moved || last_edit.resized)) {
            ImGui::SameLine();
            ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f}, "  item %d  [%.2f, %.2f]",
                last_edit.item_id, last_edit.new_t0, last_edit.new_t1);
        }
        ImGui::Separator();
        TimelineEdit edit{};
        timeline->Frame("##timeline", *tracks, &edit);
        if (edit.changed) last_edit = edit;
        ImGui::End();

        ImGui::Render();
#ifndef USE_VULKAN_BACKEND
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
#else
        // Vulkan rendering
        uint32_t img_idx = 0;
        VkResult res = vkAcquireNextImageKHR(g_vk_state.device, g_vk_state.swapchain, UINT64_MAX,
                                               g_vk_state.image_acquired_semaphores[g_vk_state.current_frame],
                                               VK_NULL_HANDLE, &img_idx);
        if (res != VK_SUCCESS) continue;

        vkWaitForFences(g_vk_state.device, 1, &g_vk_state.in_flight_fences[g_vk_state.current_frame], VK_TRUE, UINT64_MAX);
        vkResetFences(g_vk_state.device, 1, &g_vk_state.in_flight_fences[g_vk_state.current_frame]);

        VkCommandBuffer cmd = g_vk_state.cmd_buffers[img_idx];
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);

        VkRenderPassBeginInfo rp_begin_info = {};
        rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin_info.renderPass = g_vk_state.render_pass;
        rp_begin_info.framebuffer = g_vk_state.framebuffers[img_idx];
        rp_begin_info.renderArea.extent = g_vk_state.swapchain_extent;

        VkClearValue clear_value = {};
        clear_value.color = {{0.10f, 0.10f, 0.12f, 1.00f}};
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(cmd, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &g_vk_state.image_acquired_semaphores[g_vk_state.current_frame];
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &g_vk_state.render_complete_semaphores[g_vk_state.current_frame];

        vkQueueSubmit(g_vk_state.queue, 1, &submit_info, g_vk_state.in_flight_fences[g_vk_state.current_frame]);

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &g_vk_state.render_complete_semaphores[g_vk_state.current_frame];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_vk_state.swapchain;
        present_info.pImageIndices = &img_idx;

        vkQueuePresentKHR(g_vk_state.queue, &present_info);

        g_vk_state.current_frame = (g_vk_state.current_frame + 1) % 2;
#endif
    }

    // Cleanup data structures
    if (timeline) delete timeline;
    if (tracks) delete tracks;

#ifndef USE_VULKAN_BACKEND
    ImGui_ImplOpenGL3_Shutdown();
#else
    ImGui_ImplVulkan_Shutdown();
    cleanup_vulkan();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
