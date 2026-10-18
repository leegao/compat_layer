#include <vulkan/vulkan.h>

struct device;

bool IsYcbcrSampler(VkSampler sampler);

VkSampler GetYcbcrFallbackSampler(struct device *dev,
                                  const VkSamplerCreateInfo *pCreateInfo);

void DestroyYcbcrFallbackSampler(struct device *dev);
