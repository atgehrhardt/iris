/**
 * @file renderer.cpp
 * @brief Vulkan PyroWave decoding and direct Android surface presentation.
 */
#define VK_USE_PLATFORM_ANDROID_KHR
#include "api.h"
#include "presentation.h"
#include "protocol.h"
#include "shaders.h"

#include <algorithm>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <jni.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
  /**
   * @brief Reject a failed Vulkan operation.
   * @param result Vulkan status.
   * @param operation Diagnostic operation name.
   */
  void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS) {
      throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
    }
  }

  /**
   * @brief Reject a failed codec operation.
   * @param result Codec status.
   * @param operation Diagnostic operation name.
   */
  void check(pyrowave_result result, const char *operation) {
    if (result != PYROWAVE_SUCCESS) {
      throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
    }
  }

  /**
   * @brief Check a driver's advertised extension list.
   * @param extensions Driver extension list.
   * @param name Requested extension.
   * @return True if the extension is present.
   */
  bool has_extension(const std::vector<VkExtensionProperties> &extensions, const char *name) {
    return std::any_of(extensions.begin(), extensions.end(), [&](const auto &extension) {
      return !std::strcmp(extension.extensionName, name);
    });
  }

  /**
   * @brief One decoded component's GPU storage.
   */
  struct plane {
    VkImage image = VK_NULL_HANDLE;  ///< Device-local decoded component.
    VkDeviceMemory memory = VK_NULL_HANDLE;  ///< Bound storage.
    VkImageView view = VK_NULL_HANDLE;  ///< View used by the presentation shader.
  };

  /**
   * @brief One session's Vulkan, codec, and presentation resources.
   */
  class renderer {
  public:
    std::mutex mutex;  ///< Serializes rendering, metadata updates, and cleanup.
    VkInstance instance = VK_NULL_HANDLE;  ///< Owned Vulkan instance.
    VkPhysicalDevice physical = VK_NULL_HANDLE;  ///< Selected Android GPU.
    VkDevice device = VK_NULL_HANDLE;  ///< Owned device, borrowed by PyroWave.
    VkSurfaceKHR surface = VK_NULL_HANDLE;  ///< Android presentation surface.
    ANativeWindow *window = nullptr;  ///< Retained Java surface's native window.
    VkQueue queue = VK_NULL_HANDLE;  ///< Graphics/compute/presentation queue.
    uint32_t family = 0;  ///< Queue family index.
    pyrowave_device codec_device = nullptr;  ///< Borrowed-device PyroWave wrapper.
    pyrowave_decoder decoder = nullptr;  ///< Owned decoder.
    VkApplicationInfo app {VK_STRUCTURE_TYPE_APPLICATION_INFO};  ///< Persistent creation record required by PyroWave.
    VkInstanceCreateInfo instance_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};  ///< Persistent instance creation record.
    VkDeviceCreateInfo device_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};  ///< Persistent device creation record.
    VkDeviceQueueCreateInfo queue_info {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};  ///< Persistent queue creation record.
    VkPhysicalDeviceFeatures2 features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};  ///< Enabled core features.
    VkPhysicalDeviceVulkan11Features f11 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};  ///< Enabled Vulkan 1.1 features.
    VkPhysicalDeviceVulkan12Features f12 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};  ///< Enabled Vulkan 1.2 features.
    VkPhysicalDeviceVulkan13Features f13 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};  ///< Enabled Vulkan 1.3 features.
    float priority = 1.0f;  ///< Persistent queue priority.
    std::vector<const char *> instance_extensions;  ///< Persistent enabled instance extension names.
    std::vector<const char *> device_extensions;  ///< Persistent enabled device extension names.
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;  ///< Presentation chain.
    VkExtent2D extent {};  ///< Swapchain dimensions in the display's natural orientation.
    iris_pyrowave::presentation geometry {};  ///< Matching buffer, viewport, and vertex orientation.

    /**
     * @brief Shared vertex/fragment push constants with matching GLSL offsets.
     */
    struct presentation_constants {
      std::array<float, 4> rotation;  ///< Vertex pre-rotation at byte offset zero.
      int hdr;  ///< Fragment color-space selector at byte offset sixteen.
    };
    static_assert(offsetof(presentation_constants, hdr) == 16 && sizeof(presentation_constants) == 20);

    VkFormat output_format = VK_FORMAT_UNDEFINED;  ///< SDR or HDR display format.
    std::vector<VkImage> swap_images;  ///< Swapchain-owned images.
    std::vector<VkImageView> swap_views;  ///< Owned presentation image views.
    std::vector<VkFramebuffer> framebuffers;  ///< Presentation render targets.
    std::array<plane, 3> planes;  ///< Y, Cb, and Cr storage images.
    pyrowave_gpu_buffers buffers {};  ///< Codec views of component images.
    VkCommandPool pool = VK_NULL_HANDLE;  ///< Per-session command pool.
    VkCommandBuffer command = VK_NULL_HANDLE;  ///< Serialized decode-and-present command buffer.
    VkFence fence = VK_NULL_HANDLE;  ///< Completion fence protecting decoded storage.
    VkSemaphore acquired = VK_NULL_HANDLE;  ///< Swapchain acquisition semaphore.
    std::vector<VkSemaphore> ready;  ///< Per-swapchain-image presentation semaphores.
    VkRenderPass render_pass = VK_NULL_HANDLE;  ///< RGB presentation render pass.
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;  ///< Three sampled component bindings.
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;  ///< Component descriptor allocation.
    VkDescriptorSet descriptors = VK_NULL_HANDLE;  ///< Bound component images.
    VkSampler sampler = VK_NULL_HANDLE;  ///< Centered chroma interpolation.
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;  ///< Presentation shader layout.
    VkPipeline pipeline = VK_NULL_HANDLE;  ///< YCbCr-to-RGB graphics pipeline.
    VkQueryPool timestamps = VK_NULL_HANDLE;  ///< Optional GPU timing queries.
    float timestamp_period = 0;  ///< Nanoseconds per GPU timestamp tick.
    uint32_t timestamp_bits = 0;  ///< Number of valid timestamp bits on the decode queue.
    bool query_pending = false;  ///< Whether the completed frame has unread GPU timing.
    int64_t decode_us = -1;  ///< Latest unconsumed GPU decode duration in microseconds.
    bool low_latency = true;  ///< Prefer mailbox presentation for latency-oriented pacing.
    bool recreate = false;  ///< Rebuild presentation resources after surface configuration changes.
    bool has_metadata = false;  ///< Whether HDR metadata must be reapplied to a new swapchain.
    VkHdrMetadataEXT last_metadata {VK_STRUCTURE_TYPE_HDR_METADATA_EXT};  ///< Most recent static HDR metadata.
    bool initialized_images = false;  ///< Whether decoded planes have established layouts.
    bool fragment = false;  ///< Mobile fragment decoder selection.
    bool hdr = false;  ///< Negotiated PQ presentation mode.
    bool chroma444 = false;  ///< Negotiated chroma resolution.
    int width = 0;  ///< Stream luma width.
    int height = 0;  ///< Stream luma height.

    /**
     * @brief Wait for outstanding work and destroy resources in dependency order.
     */
    ~renderer() {
      if (device) {
        vkDeviceWaitIdle(device);
      }
      if (decoder) {
        pyrowave_decoder_destroy(decoder);
      }
      if (codec_device) {
        pyrowave_device_destroy(codec_device);
      }
      if (device) {
        for (auto framebuffer : framebuffers) {
          vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        for (auto view : swap_views) {
          vkDestroyImageView(device, view, nullptr);
        }
        for (auto semaphore : ready) {
          vkDestroySemaphore(device, semaphore, nullptr);
        }
        for (auto &component : planes) {
          if (component.view) {
            vkDestroyImageView(device, component.view, nullptr);
          }
          if (component.image) {
            vkDestroyImage(device, component.image, nullptr);
          }
          if (component.memory) {
            vkFreeMemory(device, component.memory, nullptr);
          }
        }
        if (timestamps) {
          vkDestroyQueryPool(device, timestamps, nullptr);
        }
        if (pipeline) {
          vkDestroyPipeline(device, pipeline, nullptr);
        }
        if (pipeline_layout) {
          vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        }
        if (render_pass) {
          vkDestroyRenderPass(device, render_pass, nullptr);
        }
        if (descriptor_pool) {
          vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        }
        if (descriptor_layout) {
          vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
        }
        if (sampler) {
          vkDestroySampler(device, sampler, nullptr);
        }
        if (fence) {
          vkDestroyFence(device, fence, nullptr);
        }
        if (acquired) {
          vkDestroySemaphore(device, acquired, nullptr);
        }
        if (pool) {
          vkDestroyCommandPool(device, pool, nullptr);
        }
        if (swapchain) {
          vkDestroySwapchainKHR(device, swapchain, nullptr);
        }
        vkDestroyDevice(device, nullptr);
      }
      if (surface) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
      }
      if (instance) {
        vkDestroyInstance(instance, nullptr);
      }
      if (window) {
        ANativeWindow_release(window);
      }
    }

    /**
     * @brief Create and validate the GPU, codec, and presentation pipeline.
     * @param native_window Retained Android window; ownership transfers to this object.
     * @param stream_width Negotiated luma width.
     * @param stream_height Negotiated luma height.
     * @param mode Negotiated HDR/chroma flags.
     * @param latency_mode Prefer mailbox presentation when supported.
     */
    void init(ANativeWindow *native_window, int stream_width, int stream_height, int mode, bool latency_mode) {
      low_latency = latency_mode;
      window = native_window;
      width = stream_width;
      height = stream_height;
      hdr = (mode & 1) != 0;
      chroma444 = (mode & 2) != 0;
      if (!window || mode < 0 || mode > 3 || width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
          (!chroma444 && ((width | height) & 1))) {
        throw std::runtime_error("Invalid PyroWave surface or dimensions");
      }
      uint32_t major, minor, patch;
      pyrowave_get_api_version(&major, &minor, &patch);
      if (major != PYROWAVE_API_VERSION_MAJOR || minor != PYROWAVE_API_VERSION_MINOR) {
        throw std::runtime_error("PyroWave API mismatch");
      }
      uint32_t count = 0;
      check(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr), "Instance extensions");
      std::vector<VkExtensionProperties> extensions(count);
      check(vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()), "Instance extensions");
      instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
      if (has_extension(extensions, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME)) {
        instance_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
      } else if (hdr) {
        throw std::runtime_error("HDR Vulkan color spaces are unavailable");
      }
      app.apiVersion = VK_API_VERSION_1_3;
      app.pApplicationName = "Iris PyroWave";
      instance_info.pApplicationInfo = &app;
      instance_info.enabledExtensionCount = instance_extensions.size();
      instance_info.ppEnabledExtensionNames = instance_extensions.data();
      check(vkCreateInstance(&instance_info, nullptr, &instance), "Vulkan 1.3 instance");
      VkAndroidSurfaceCreateInfoKHR surface_info {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
      surface_info.window = window;
      check(vkCreateAndroidSurfaceKHR(instance, &surface_info, nullptr, &surface), "Android surface");
      check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "GPU enumeration");
      std::vector<VkPhysicalDevice> devices(count);
      check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "GPU enumeration");
      for (auto candidate : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3) {
          continue;
        }
        uint32_t queues = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queues, nullptr);
        std::vector<VkQueueFamilyProperties> queue_properties(queues);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queues, queue_properties.data());
        for (uint32_t i = 0; i < queues; ++i) {
          VkBool32 present = VK_FALSE;
          check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &present), "Surface support");
          if (present && (queue_properties[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
                           (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            physical = candidate;
            family = i;
            timestamp_period = properties.limits.timestampPeriod;
            timestamp_bits = queue_properties[i].timestampValidBits;
            break;
          }
        }
        if (physical) {
          break;
        }
      }
      if (!physical) {
        throw std::runtime_error("No Vulkan 1.3 GPU with compute and presentation support");
      }
      check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr), "Device extensions");
      extensions.resize(count);
      check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extensions.data()), "Device extensions");
      device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
      if (has_extension(extensions, VK_EXT_HDR_METADATA_EXTENSION_NAME)) {
        device_extensions.push_back(VK_EXT_HDR_METADATA_EXTENSION_NAME);
      } else if (hdr) {
        throw std::runtime_error("HDR metadata presentation is unavailable");
      }
      features.pNext = &f11;
      f11.pNext = &f12;
      f12.pNext = &f13;
      auto get_features = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"));
      if (!get_features) {
        throw std::runtime_error("Vulkan feature query is unavailable");
      }
      get_features(physical, &features);
      if (!f13.subgroupSizeControl || !f13.synchronization2) {
        throw std::runtime_error("Required Vulkan subgroup/synchronization features are unavailable");
      }
      queue_info.queueFamilyIndex = family;
      queue_info.queueCount = 1;
      queue_info.pQueuePriorities = &priority;
      device_info.pNext = &features;
      device_info.queueCreateInfoCount = 1;
      device_info.pQueueCreateInfos = &queue_info;
      device_info.enabledExtensionCount = device_extensions.size();
      device_info.ppEnabledExtensionNames = device_extensions.data();
      check(vkCreateDevice(physical, &device_info, nullptr, &device), "Vulkan device");
      vkGetDeviceQueue(device, family, 0, &queue);
      pyrowave_device_create_info codec_info {};
      codec_info.GetInstanceProcAddr = vkGetInstanceProcAddr;
      codec_info.instance = instance;
      codec_info.physical_device = physical;
      codec_info.device = device;
      codec_info.instance_create_info = &instance_info;
      codec_info.device_create_info = &device_info;
      check(pyrowave_create_device(&codec_info, &codec_device), "PyroWave device features");
      fragment = pyrowave_decoder_device_prefers_fragment_path(codec_device);
      pyrowave_decoder_create_info decode_info {codec_device, width, height, chroma444 ? PYROWAVE_CHROMA_SUBSAMPLING_444 : PYROWAVE_CHROMA_SUBSAMPLING_420, fragment};
      check(pyrowave_decoder_create(&decode_info, &decoder), "PyroWave decoder");
      if (timestamp_bits) {
        VkQueryPoolCreateInfo query {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query.queryCount = 2;
        check(vkCreateQueryPool(device, &query, nullptr, &timestamps), "GPU timestamp queries");
      }
      create_swapchain();
      create_planes();
      create_pipeline();
      VkCommandPoolCreateInfo pool_info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      pool_info.queueFamilyIndex = family;
      check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "Command pool");
      VkCommandBufferAllocateInfo allocate {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      allocate.commandPool = pool;
      allocate.commandBufferCount = 1;
      allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      check(vkAllocateCommandBuffers(device, &allocate, &command), "Command buffer");
      VkFenceCreateInfo fence_info {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
      fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      check(vkCreateFence(device, &fence_info, nullptr, &fence), "Frame fence");
      VkSemaphoreCreateInfo semaphore_info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      check(vkCreateSemaphore(device, &semaphore_info, nullptr, &acquired), "Acquire semaphore");
    }

    /**
     * @brief Create a swapchain that preserves the explicitly requested dynamic range.
     */
    void create_swapchain() {
      VkSurfaceCapabilitiesKHR caps;
      check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps), "Surface capabilities");
      uint32_t count = 0;
      check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr), "Surface formats");
      std::vector<VkSurfaceFormatKHR> formats(count);
      check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data()), "Surface formats");
      VkSurfaceFormatKHR chosen {};
      for (auto format : formats) {
        bool match = hdr ? (format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
                            (format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32)) :
                           (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
                            (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM));
        if (match) {
          chosen = format;
          break;
        }
      }
      if (!chosen.format) {
        throw std::runtime_error(hdr ? "HDR10 Vulkan presentation is unavailable" : "SDR Vulkan presentation is unavailable");
      }
      output_format = chosen.format;
      extent = caps.currentExtent;
      if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(uint32_t(ANativeWindow_getWidth(window)), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(uint32_t(ANativeWindow_getHeight(window)), caps.minImageExtent.height, caps.maxImageExtent.height);
      }
      geometry = iris_pyrowave::make_presentation(extent, caps.currentTransform, width, height);
      extent = geometry.image_extent;
      __android_log_print(ANDROID_LOG_INFO, "IrisPyroWave", "Video %dx%d; window %dx%d; buffer %ux%u; viewport %.0fx%.0f; transform 0x%x; %s decoder", width, height, ANativeWindow_getWidth(window), ANativeWindow_getHeight(window), extent.width, extent.height, geometry.viewport.width, geometry.viewport.height, caps.currentTransform, fragment ? "fragment" : "compute");
      VkSwapchainCreateInfoKHR info {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
      info.surface = surface;
      info.minImageCount = caps.minImageCount + 1;
      if (caps.maxImageCount) {
        info.minImageCount = std::min(info.minImageCount, caps.maxImageCount);
      }
      info.imageFormat = chosen.format;
      info.imageColorSpace = chosen.colorSpace;
      info.imageExtent = extent;
      info.imageArrayLayers = 1;
      info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      info.preTransform = caps.currentTransform;
      info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      if (!(caps.supportedCompositeAlpha & info.compositeAlpha)) {
        info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
      }
      info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
      info.clipped = VK_TRUE;
      if (low_latency) {
        uint32_t mode_count = 0;
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, nullptr), "Presentation modes");
        std::vector<VkPresentModeKHR> modes(mode_count);
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, modes.data()), "Presentation modes");
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
          info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
          info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
      }
      check(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Swapchain");
      check(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr), "Swapchain images");
      swap_images.resize(count);
      check(vkGetSwapchainImagesKHR(device, swapchain, &count, swap_images.data()), "Swapchain images");
      for (auto image : swap_images) {
        VkImageViewCreateInfo view {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = output_format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView handle;
        check(vkCreateImageView(device, &view, nullptr, &handle), "Swapchain view");
        swap_views.push_back(handle);
        VkSemaphoreCreateInfo semaphore_info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore semaphore;
        check(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore), "Present semaphore");
        ready.push_back(semaphore);
      }
    }

    /**
     * @brief Allocate three high-precision component images for direct GPU decoding.
     */
    void create_planes() {
      VkPhysicalDeviceMemoryProperties memory;
      vkGetPhysicalDeviceMemoryProperties(physical, &memory);
      for (unsigned i = 0; i < 3; ++i) {
        auto &component = planes[i];
        uint32_t divisor = i && !chroma444 ? 2 : 1;
        VkImageCreateInfo info {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R16_UNORM;
        info.extent = {uint32_t(width) / divisor, uint32_t(height) / divisor, 1};
        info.mipLevels = info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | (fragment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_STORAGE_BIT);
        check(vkCreateImage(device, &info, nullptr, &component.image), "Decoded plane");
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, component.image, &requirements);
        uint32_t type = UINT32_MAX;
        for (uint32_t j = 0; j < memory.memoryTypeCount; ++j) {
          if ((requirements.memoryTypeBits & (1u << j)) && (memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            type = j;
            break;
          }
        }
        if (type == UINT32_MAX) {
          throw std::runtime_error("No GPU memory for decoded planes");
        }
        VkMemoryAllocateInfo allocate {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = type;
        check(vkAllocateMemory(device, &allocate, nullptr, &component.memory), "Decoded memory");
        check(vkBindImageMemory(device, component.image, component.memory, 0), "Decoded image binding");
        VkImageViewCreateInfo view {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = component.image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = info.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(device, &view, nullptr, &component.view), "Decoded view");
        auto &codec_view = buffers.planes[i];
        codec_view.image = component.image;
        codec_view.width = info.extent.width;
        codec_view.height = info.extent.height;
        codec_view.image_format = codec_view.view_format = info.format;
        codec_view.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        codec_view.layout = VK_IMAGE_LAYOUT_GENERAL;
      }
    }

    /**
     * @brief Create YCbCr sampling descriptors and the presentation graphics pipeline.
     */
    void create_pipeline() {
      std::array<VkDescriptorSetLayoutBinding, 3> bindings {};
      for (unsigned i = 0; i < 3; ++i) {
        bindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
      }
      VkDescriptorSetLayoutCreateInfo layout {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      layout.bindingCount = bindings.size();
      layout.pBindings = bindings.data();
      check(vkCreateDescriptorSetLayout(device, &layout, nullptr, &descriptor_layout), "Descriptor layout");
      VkDescriptorPoolSize pool_size {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3};
      VkDescriptorPoolCreateInfo pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
      pool_info.maxSets = 1;
      pool_info.poolSizeCount = 1;
      pool_info.pPoolSizes = &pool_size;
      check(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool), "Descriptor pool");
      VkDescriptorSetAllocateInfo allocation {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      allocation.descriptorPool = descriptor_pool;
      allocation.descriptorSetCount = 1;
      allocation.pSetLayouts = &descriptor_layout;
      check(vkAllocateDescriptorSets(device, &allocation, &descriptors), "Descriptor set");
      VkSamplerCreateInfo sampler_info {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      sampler_info.magFilter = sampler_info.minFilter = VK_FILTER_LINEAR;
      sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      check(vkCreateSampler(device, &sampler_info, nullptr, &sampler), "Plane sampler");
      std::array<VkDescriptorImageInfo, 3> images {};
      std::array<VkWriteDescriptorSet, 3> writes {};
      for (unsigned i = 0; i < 3; ++i) {
        images[i] = {sampler, planes[i].view, VK_IMAGE_LAYOUT_GENERAL};
        writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i].dstSet = descriptors;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &images[i];
      }
      vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
      VkAttachmentDescription attachment {};
      attachment.format = output_format;
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      VkAttachmentReference reference {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkSubpassDescription subpass {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &reference;
      VkSubpassDependency dependency {};
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      VkRenderPassCreateInfo pass {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
      pass.attachmentCount = pass.subpassCount = pass.dependencyCount = 1;
      pass.pAttachments = &attachment;
      pass.pSubpasses = &subpass;
      pass.pDependencies = &dependency;
      check(vkCreateRenderPass(device, &pass, nullptr, &render_pass), "Presentation render pass");
      for (auto view : swap_views) {
        VkFramebufferCreateInfo info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = render_pass;
        info.attachmentCount = 1;
        info.pAttachments = &view;
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;
        VkFramebuffer framebuffer;
        check(vkCreateFramebuffer(device, &info, nullptr, &framebuffer), "Presentation framebuffer");
        framebuffers.push_back(framebuffer);
      }
      VkPushConstantRange push {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(presentation_constants)};
      VkPipelineLayoutCreateInfo pipeline_info {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      pipeline_info.setLayoutCount = 1;
      pipeline_info.pSetLayouts = &descriptor_layout;
      pipeline_info.pushConstantRangeCount = 1;
      pipeline_info.pPushConstantRanges = &push;
      check(vkCreatePipelineLayout(device, &pipeline_info, nullptr, &pipeline_layout), "Pipeline layout");
      VkShaderModule vertex = VK_NULL_HANDLE, fragment_shader = VK_NULL_HANDLE;
      VkShaderModuleCreateInfo shader {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      shader.codeSize = sizeof(vert_spv);
      shader.pCode = vert_spv;
      check(vkCreateShaderModule(device, &shader, nullptr, &vertex), "Vertex shader");
      shader.codeSize = sizeof(frag_spv);
      shader.pCode = frag_spv;
      auto fragment_result = vkCreateShaderModule(device, &shader, nullptr, &fragment_shader);
      if (fragment_result != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertex, nullptr);
        check(fragment_result, "Fragment shader");
      }
      std::array<VkPipelineShaderStageCreateInfo, 2> stages {};
      stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = vertex;
      stages[0].pName = "main";
      stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = fragment_shader;
      stages[1].pName = "main";
      VkPipelineVertexInputStateCreateInfo vertices {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
      VkPipelineInputAssemblyStateCreateInfo assembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      VkRect2D scissor {{0, 0}, extent};
      VkPipelineViewportStateCreateInfo viewport_info {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
      viewport_info.viewportCount = viewport_info.scissorCount = 1;
      viewport_info.pViewports = &geometry.viewport;
      viewport_info.pScissors = &scissor;
      VkPipelineRasterizationStateCreateInfo raster {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
      raster.polygonMode = VK_POLYGON_MODE_FILL;
      raster.lineWidth = 1;
      VkPipelineMultisampleStateCreateInfo samples {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
      samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      VkPipelineColorBlendAttachmentState blend {};
      blend.colorWriteMask = 15;
      VkPipelineColorBlendStateCreateInfo blend_info {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
      blend_info.attachmentCount = 1;
      blend_info.pAttachments = &blend;
      VkGraphicsPipelineCreateInfo graphics {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      graphics.stageCount = stages.size();
      graphics.pStages = stages.data();
      graphics.pVertexInputState = &vertices;
      graphics.pInputAssemblyState = &assembly;
      graphics.pViewportState = &viewport_info;
      graphics.pRasterizationState = &raster;
      graphics.pMultisampleState = &samples;
      graphics.pColorBlendState = &blend_info;
      graphics.layout = pipeline_layout;
      graphics.renderPass = render_pass;
      auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics, nullptr, &pipeline);
      vkDestroyShaderModule(device, vertex, nullptr);
      vkDestroyShaderModule(device, fragment_shader, nullptr);
      check(result, "Presentation pipeline");
    }

    /**
     * @brief Rebuild only surface-dependent resources after a resize or refresh-rate transition.
     */
    void recreate_presentation() {
      check(vkDeviceWaitIdle(device), "Wait before surface recreation");
      for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
      }
      framebuffers.clear();
      for (auto view : swap_views) {
        vkDestroyImageView(device, view, nullptr);
      }
      swap_views.clear();
      for (auto semaphore : ready) {
        vkDestroySemaphore(device, semaphore, nullptr);
      }
      ready.clear();
      vkDestroyPipeline(device, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
      vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
      pipeline_layout = VK_NULL_HANDLE;
      vkDestroyRenderPass(device, render_pass, nullptr);
      render_pass = VK_NULL_HANDLE;
      vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
      descriptor_pool = VK_NULL_HANDLE;
      vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
      descriptor_layout = VK_NULL_HANDLE;
      vkDestroySampler(device, sampler, nullptr);
      sampler = VK_NULL_HANDLE;
      vkDestroySwapchainKHR(device, swapchain, nullptr);
      swapchain = VK_NULL_HANDLE;
      create_swapchain();
      create_pipeline();
      if (has_metadata) {
        auto set_metadata = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(vkGetDeviceProcAddr(device, "vkSetHdrMetadataEXT"));
        set_metadata(device, 1, &swapchain, &last_metadata);
      }
      recreate = false;
    }

    /**
     * @brief Decode and present a complete validated frame without accumulating a CPU queue.
     * @param data Envelope bytes.
     * @param size Envelope size.
     * @return True if submitted, false when the display is busy and this independent frame was dropped.
     */
    bool submit(const uint8_t *data, size_t size) {
      auto packets = prism_pyrowave::unpack(data, size, uint8_t(hdr | chroma444 << 1));
      if (packets.empty() || !prism_pyrowave::validate_bitstream(packets, width, height, uint8_t(hdr | chroma444 << 1))) {
        throw std::runtime_error("Malformed PyroWave frame envelope");
      }
      if (recreate) {
        recreate_presentation();
      }
      auto status = vkGetFenceStatus(device, fence);
      if (status == VK_NOT_READY) {
        return false;
      }
      check(status, "GPU completion");
      if (timestamps && query_pending) {
        uint64_t times[2] {};
        check(vkGetQueryPoolResults(device, timestamps, 0, 2, sizeof(times), times, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT), "GPU timing");
        uint64_t mask = timestamp_bits >= 64 ? UINT64_MAX : (uint64_t(1) << timestamp_bits) - 1;
        decode_us = int64_t(double((times[1] - times[0]) & mask) * timestamp_period / 1000.0);
        query_pending = false;
      }
      uint32_t index = 0;
      status = vkAcquireNextImageKHR(device, swapchain, 0, acquired, VK_NULL_HANDLE, &index);
      if (status == VK_NOT_READY || status == VK_TIMEOUT) {
        return false;
      }
      if (status == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate = true;
        return false;
      }
      if (status != VK_SUBOPTIMAL_KHR) {
        check(status, "Acquire presentation image");
      }
      pyrowave_decoder_clear(decoder);
      for (const auto &packet : packets) {
        check(pyrowave_decoder_push_packet(decoder, packet.data, packet.size), "Decode packet");
      }
      if (!pyrowave_decoder_decode_is_ready(decoder, false)) {
        throw std::runtime_error("Incomplete PyroWave frame");
      }
      check(vkResetCommandBuffer(command, 0), "Reset decode commands");
      VkCommandBufferBeginInfo begin {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      check(vkBeginCommandBuffer(command, &begin), "Begin decode commands");
      if (timestamps) {
        vkCmdResetQueryPool(command, timestamps, 0, 2);
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamps, 0);
      }
      std::array<VkImageMemoryBarrier, 3> barriers {};
      for (unsigned i = 0; i < 3; ++i) {
        auto &barrier = barriers[i];
        barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = initialized_images ? VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask = fragment ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = initialized_images ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = planes[i].image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      }
      auto decode_stage = fragment ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      vkCmdPipelineBarrier(command, initialized_images ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, decode_stage, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
      pyrowave_device_set_command_buffer(codec_device, command);
      auto decoded = pyrowave_decoder_decode_gpu_buffer(decoder, nullptr, nullptr, &buffers);
      pyrowave_device_set_command_buffer(codec_device, VK_NULL_HANDLE);
      check(decoded, "GPU decoding");
      if (timestamps) {
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamps, 1);
      }
      for (auto &barrier : barriers) {
        barrier.srcAccessMask = barrier.dstAccessMask;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      }
      vkCmdPipelineBarrier(command, decode_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());
      VkClearValue clear {};
      VkRenderPassBeginInfo render {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      render.renderPass = render_pass;
      render.framebuffer = framebuffers[index];
      render.renderArea.extent = extent;
      render.clearValueCount = 1;
      render.pClearValues = &clear;
      vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptors, 0, nullptr);
      presentation_constants constants {geometry.rotation, int(hdr)};
      vkCmdPushConstants(command, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
      vkCmdDraw(command, 3, 1, 0, 0);
      vkCmdEndRenderPass(command);
      check(vkEndCommandBuffer(command), "End decode commands");
      VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo submission {VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submission.waitSemaphoreCount = 1;
      submission.pWaitSemaphores = &acquired;
      submission.pWaitDstStageMask = &wait_stage;
      submission.commandBufferCount = 1;
      submission.pCommandBuffers = &command;
      submission.signalSemaphoreCount = 1;
      submission.pSignalSemaphores = &ready[index];
      check(vkResetFences(device, 1, &fence), "Reset completion fence");
      check(vkQueueSubmit(queue, 1, &submission, fence), "Submit decoded frame");
      initialized_images = true;
      query_pending = timestamps != VK_NULL_HANDLE;
      VkPresentInfoKHR present {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
      present.waitSemaphoreCount = 1;
      present.pWaitSemaphores = &ready[index];
      present.swapchainCount = 1;
      present.pSwapchains = &swapchain;
      present.pImageIndices = &index;
      status = vkQueuePresentKHR(queue, &present);
      if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR) {
        recreate = true;
      } else {
        check(status, "Present decoded frame");
      }
      return true;
    }

    /**
     * @brief Apply HDR10 static metadata using the existing Moonlight metadata encoding.
     * @param values Twelve unsigned little-endian 16-bit metadata values.
     */
    void metadata(const uint16_t *values) {
      if (!hdr) {
        return;
      }
      auto set_metadata = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(vkGetDeviceProcAddr(device, "vkSetHdrMetadataEXT"));
      if (!set_metadata) {
        throw std::runtime_error("HDR metadata extension entry point missing");
      }
      VkHdrMetadataEXT info {VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
      info.displayPrimaryRed = {values[0] / 50000.f, values[1] / 50000.f};
      info.displayPrimaryGreen = {values[2] / 50000.f, values[3] / 50000.f};
      info.displayPrimaryBlue = {values[4] / 50000.f, values[5] / 50000.f};
      info.whitePoint = {values[6] / 50000.f, values[7] / 50000.f};
      info.maxLuminance = values[8];
      info.minLuminance = values[9] / 10000.f;
      info.maxContentLightLevel = values[10];
      info.maxFrameAverageLightLevel = values[11];
      last_metadata = info;
      has_metadata = true;
      set_metadata(device, 1, &swapchain, &info);
    }
  };

  /**
   * @brief Deliver a native failure as an actionable Java exception.
   * @param env JNI environment.
   * @param error Failure description.
   */
  void fail(JNIEnv *env, const std::exception &error) {
    __android_log_print(ANDROID_LOG_ERROR, "IrisPyroWave", "%s", error.what());
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), error.what());
  }
}  // namespace

/**
 * @brief Create one native renderer after the Android surface is available.
 * @param env JNI environment.
 * @param surface Java presentation surface.
 * @param width Stream width.
 * @param height Stream height.
 * @param mode Negotiated HDR/chroma flags.
 * @param low_latency Prefer mailbox presentation.
 * @return Native handle, or zero with a pending exception.
 */
extern "C" JNIEXPORT jlong JNICALL Java_com_limelight_binding_video_PyroWaveDecoderRenderer_nativeCreate(
  JNIEnv *env,
  jclass,
  jobject surface,
  jint width,
  jint height,
  jint mode,
  jboolean low_latency
) {
  try {
    auto result = std::make_unique<renderer>();
    result->init(ANativeWindow_fromSurface(env, surface), width, height, mode, low_latency);
    return reinterpret_cast<jlong>(result.release());
  } catch (const std::exception &error) {
    fail(env, error);
    return 0;
  }
}

/**
 * @brief Submit an envelope while Java holds the renderer lifecycle lock.
 * @param env JNI environment.
 * @param handle Native renderer handle.
 * @param data Java frame bytes.
 * @param length Number of frame bytes.
 * @return True if the frame was submitted to the GPU.
 */
extern "C" JNIEXPORT jboolean JNICALL Java_com_limelight_binding_video_PyroWaveDecoderRenderer_nativeSubmit(
  JNIEnv *env,
  jclass,
  jlong handle,
  jbyteArray data,
  jint length
) {
  try {
    if (!handle || !data || length < 0 || length > env->GetArrayLength(data) || size_t(length) > prism_pyrowave::maximum_frame_size) {
      throw std::runtime_error("Invalid frame buffer");
    }
    std::vector<uint8_t> bytes(length);
    env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte *>(bytes.data()));
    if (env->ExceptionCheck()) {
      return false;
    }
    auto *state = reinterpret_cast<renderer *>(handle);
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->submit(bytes.data(), bytes.size());
  } catch (const std::exception &error) {
    fail(env, error);
    return false;
  }
}

/**
 * @brief Apply the existing Android HDR static metadata blob.
 * @param env JNI environment.
 * @param handle Native renderer handle.
 * @param metadata Moonlight SS_HDR_METADATA: thirteen little-endian uint16 values.
 */
extern "C" JNIEXPORT void JNICALL Java_com_limelight_binding_video_PyroWaveDecoderRenderer_nativeHdr(
  JNIEnv *env,
  jclass,
  jlong handle,
  jbyteArray metadata
) {
  try {
    if (!handle || !metadata || env->GetArrayLength(metadata) != 26) {
      throw std::runtime_error("Missing HDR10 metadata");
    }
    std::array<uint8_t, 26> bytes {};
    env->GetByteArrayRegion(metadata, 0, bytes.size(), reinterpret_cast<jbyte *>(bytes.data()));
    if (env->ExceptionCheck()) {
      return;
    }
    std::array<uint16_t, 12> values {};
    for (unsigned i = 0; i < values.size(); ++i) {
      values[i] = uint16_t(bytes[2 * i]) | uint16_t(bytes[1 + 2 * i]) << 8;
    }
    auto *state = reinterpret_cast<renderer *>(handle);
    std::lock_guard<std::mutex> lock(state->mutex);
    state->metadata(values.data());
  } catch (const std::exception &error) {
    fail(env, error);
  }
}

/**
 * @brief Release the renderer after Java serializes all submissions and metadata callbacks.
 * @param handle Native renderer handle, possibly zero.
 */
extern "C" JNIEXPORT void JNICALL Java_com_limelight_binding_video_PyroWaveDecoderRenderer_nativeDestroy(JNIEnv *, jclass, jlong handle) {
  delete reinterpret_cast<renderer *>(handle);
}

/**
 * @brief Consume the latest GPU decode measurement without reporting CPU submission time as latency.
 * @param handle Native renderer handle, accessed under the Java lifecycle lock.
 * @return Decode duration in microseconds, or -1 when no new measurement is available.
 */
extern "C" JNIEXPORT jlong JNICALL Java_com_limelight_binding_video_PyroWaveDecoderRenderer_nativeTakeDecodeTimeUs(JNIEnv *, jclass, jlong handle) {
  if (!handle) {
    return -1;
  }
  auto *state = reinterpret_cast<renderer *>(handle);
  std::lock_guard<std::mutex> lock(state->mutex);
  auto result = state->decode_us;
  state->decode_us = -1;
  return result;
}
