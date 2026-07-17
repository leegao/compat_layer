#pragma once

#include "layer.hpp"
#include <vulkan/vulkan.h>

struct image {
    VkImage handle;
    VkFormat format;
    struct device *device;
    VkImageCreateInfo create_info;
};

struct image *find_image(VkImage);
