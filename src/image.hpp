#pragma once

#include "layer.hpp"
#include "sparse_binding.hpp"
#include <memory>
#include <vulkan/vulkan.h>

struct image {
    VkImage handle;
    VkFormat format;
    struct device *device;
    VkImageCreateInfo create_info;
    bool emulate_sparse_binding;
    std::unique_ptr<dense_sparse_resource> sparse_resource;
};

struct image *find_image(VkImage);

struct dense_sparse_resource *find_sparse_image(VkImage image);
