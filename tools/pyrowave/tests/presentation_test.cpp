/**
 * @file presentation_test.cpp
 * @brief Check Android pre-rotation against labeled image corners and expected display coverage.
 */
#include "presentation.h"

#include <cassert>
#include <cmath>

/**
 * @brief Verify every orientation fills a matching landscape display without distorting the source.
 * @return Zero when all orientation, aspect-ratio, and invalid-input checks pass.
 */
int main() {
  // Corner labels in source order: top-left, top-right, bottom-right, bottom-left.
  const std::array<std::array<float, 2>, 4> corners {{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
  const std::array<VkSurfaceTransformFlagBitsKHR, 8> transforms {
    VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
    VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
    VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR,
  };
  const std::array<std::array<int, 4>, 8> destinations {{{0, 1, 2, 3}, {1, 2, 3, 0}, {2, 3, 0, 1}, {3, 0, 1, 2}, {1, 0, 3, 2}, {2, 1, 0, 3}, {3, 2, 1, 0}, {0, 3, 2, 1}}};
  for (size_t i = 0; i < transforms.size(); ++i) {
    const bool quarter_turn = i % 2;
    auto result = iris_pyrowave::make_presentation({1920, 1080}, transforms[i], 2560, 1440);
    assert(result.image_extent.width == (quarter_turn ? 1080u : 1920u));
    assert(result.image_extent.height == (quarter_turn ? 1920u : 1080u));
    assert(result.viewport.x == 0 && result.viewport.y == 0);
    assert(result.viewport.width == result.image_extent.width);
    assert(result.viewport.height == result.image_extent.height);
    assert(result.viewport.minDepth == 0 && result.viewport.maxDepth == 1);
    for (size_t corner = 0; corner < corners.size(); ++corner) {
      auto source = corners[corner];
      auto expected = corners[destinations[i][corner]];
      auto matrix = result.rotation;
      assert(matrix[0] * source[0] + matrix[1] * source[1] == expected[0]);
      assert(matrix[2] * source[0] + matrix[3] * source[1] == expected[1]);
    }
    // A 4:3 source has 240-pixel bars on a 1920x1080 logical display, also after rotation.
    result = iris_pyrowave::make_presentation({1920, 1080}, transforms[i], 1600, 1200);
    assert(result.viewport.x == (quarter_turn ? 0 : 240));
    assert(result.viewport.y == (quarter_turn ? 240 : 0));
    assert(result.viewport.width == (quarter_turn ? 1080 : 1440));
    assert(result.viewport.height == (quarter_turn ? 1440 : 1080));
    // A wider source must preserve top/bottom bars in logical display coordinates.
    result = iris_pyrowave::make_presentation({1920, 1080}, transforms[i], 3840, 1080);
    assert(result.viewport.x == (quarter_turn ? 270 : 0));
    assert(result.viewport.y == (quarter_turn ? 0 : 270));
    assert(result.viewport.width == (quarter_turn ? 540 : 1920));
    assert(result.viewport.height == (quarter_turn ? 1920 : 540));
  }
  // Invalid inputs must fail before producing a zero-size or non-finite viewport.
  for (int invalid = 0; invalid < 8; ++invalid) {
    bool rejected = false;
    try {
      iris_pyrowave::make_presentation({invalid == 0 ? 0u : 1920u, invalid == 1 ? 0u : 1080u}, invalid == 6 ? VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR : invalid == 7 ? VkSurfaceTransformFlagBitsKHR(0) :
                                                                                                                                                                    VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                       invalid == 2 ? 0 : invalid == 3 ? -1 :
                                                                         2560,
                                       invalid == 4 ? 0 : invalid == 5 ? -1 :
                                                                         1440);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
  }
}
