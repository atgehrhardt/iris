/**
 * @file presentation.h
 * @brief Preserve video orientation and pixel coverage on Android's naturally oriented swapchains.
 */
#pragma once

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace iris_pyrowave {
  /**
   * @brief Swapchain geometry and the vertex transform applied before Android presentation.
   */
  struct presentation {
    VkExtent2D image_extent;  ///< Buffer size in the display's natural orientation.
    VkViewport viewport;  ///< Aspect-fit video rectangle in buffer coordinates.
    std::array<float, 4> rotation;  ///< Row-major clip-space transform, including any required mirror.
  };

  /**
   * @brief Rotate the buffer dimensions, video rectangle, and vertices as one operation.
   *
   * @param window Logical surface dimensions reported before swapchain pre-rotation.
   * @param transform Android's current surface transform, also supplied as swapchain preTransform.
   * @param width Negotiated video width before rotation.
   * @param height Negotiated video height before rotation.
   * @return Natural-orientation buffer geometry with full-resolution, aspect-fit presentation.
   * @throws std::invalid_argument If dimensions or the transform are unsupported.
   */
  inline presentation make_presentation(VkExtent2D window, VkSurfaceTransformFlagBitsKHR transform, int width, int height) {
    if (!window.width || !window.height || width <= 0 || height <= 0) {
      throw std::invalid_argument("Invalid PyroWave presentation dimensions");
    }
    std::array<float, 4> rotation;
    bool quarter_turn = false;
    switch (transform) {
      case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
        rotation = {1, 0, 0, 1};
        break;
      case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
        rotation = {0, -1, 1, 0};
        quarter_turn = true;
        break;
      case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
        rotation = {-1, 0, 0, -1};
        break;
      case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
        rotation = {0, 1, -1, 0};
        quarter_turn = true;
        break;
      case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
        rotation = {-1, 0, 0, 1};
        break;
      case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
        rotation = {0, -1, -1, 0};
        quarter_turn = true;
        break;
      case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
        rotation = {1, 0, 0, -1};
        break;
      case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
        rotation = {0, 1, 1, 0};
        quarter_turn = true;
        break;
      default:
        throw std::invalid_argument("Unsupported PyroWave surface transform");
    }
    if (quarter_turn) {
      std::swap(window.width, window.height);
      std::swap(width, height);
    }
    float scale = std::min(float(window.width) / width, float(window.height) / height);
    VkViewport viewport {(window.width - width * scale) / 2, (window.height - height * scale) / 2, width * scale, height * scale, 0, 1};
    return {window, viewport, rotation};
  }
}  // namespace iris_pyrowave
