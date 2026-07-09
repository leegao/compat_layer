#ifndef __QUEUE_HPP
#define __QUEUE_HPP
#include "layer.hpp"

#include <vulkan/vulkan.h>

struct queue {
    VkQueue handle;
    struct device *device;
};

struct queue *get_queue(VkQueue queue);

#endif
