// Legacy builds bind the source image/sampler for each adjacent draw run.
// Modern builds select from the bindless arrays using vertex metadata.
#if defined(PVZ_VULKAN_LEGACY)
#define PVZ_IMAGE_COUNT 1
#define PVZ_SAMPLER_COUNT 1
#define nonuniformEXT(index) 0u
#else
#extension GL_EXT_nonuniform_qualifier : require
#define PVZ_IMAGE_COUNT 4096
#define PVZ_SAMPLER_COUNT 4
#endif
