/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "anv_private.h"
#include "mesa/main/git_sha1.h"
#include "util/strtod.h"

#include "gen7_pack.h"

struct anv_dispatch_table dtable;

static void
compiler_debug_log(void *data, const char *fmt, ...)
{ }

static void
compiler_perf_log(void *data, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);

   if (unlikely(INTEL_DEBUG & DEBUG_PERF))
      vfprintf(stderr, fmt, args);

   va_end(args);
}

static VkResult
anv_physical_device_init(struct anv_physical_device *device,
                         struct anv_instance *instance,
                         const char *path)
{
   VkResult result;
   int fd;

   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                       "failed to open %s: %m", path);

   device->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   device->instance = instance;
   device->path = path;

   device->chipset_id = anv_gem_get_param(fd, I915_PARAM_CHIPSET_ID);
   if (!device->chipset_id) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "failed to get chipset id: %m");
      goto fail;
   }

   device->name = brw_get_device_name(device->chipset_id);
   device->info = brw_get_device_info(device->chipset_id);
   if (!device->info) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "failed to get device info");
      goto fail;
   }

   if (device->info->is_haswell) {
      fprintf(stderr, "WARNING: Haswell Vulkan support is incomplete\n");
   } else if (device->info->gen == 7 && !device->info->is_baytrail) {
      fprintf(stderr, "WARNING: Ivy Bridge Vulkan support is incomplete\n");
   } else if (device->info->gen == 9) {
      fprintf(stderr, "WARNING: Skylake Vulkan support is incomplete\n");
   } else if (device->info->gen == 8 && !device->info->is_cherryview) {
      /* Broadwell is as fully supported as anything */
   } else {
      result = vk_errorf(VK_UNSUPPORTED,
                         "Vulkan not yet supported on %s", device->name);
      goto fail;
   }

   if (anv_gem_get_aperture(fd, &device->aperture_size) == -1) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "failed to get aperture size: %m");
      goto fail;
   }

   if (!anv_gem_get_param(fd, I915_PARAM_HAS_WAIT_TIMEOUT)) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "kernel missing gem wait");
      goto fail;
   }

   if (!anv_gem_get_param(fd, I915_PARAM_HAS_EXECBUF2)) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "kernel missing execbuf2");
      goto fail;
   }

   if (!anv_gem_get_param(fd, I915_PARAM_HAS_LLC)) {
      result = vk_errorf(VK_ERROR_INITIALIZATION_FAILED,
                         "non-llc gpu");
      goto fail;
   }

   close(fd);

   brw_process_intel_debug_variable();

   device->compiler = brw_compiler_create(NULL, device->info);
   if (device->compiler == NULL) {
      result = vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail;
   }
   device->compiler->shader_debug_log = compiler_debug_log;
   device->compiler->shader_perf_log = compiler_perf_log;

   isl_device_init(&device->isl_dev, device->info);

   return VK_SUCCESS;

fail:
   close(fd);
   return result;
}

static void
anv_physical_device_finish(struct anv_physical_device *device)
{
   ralloc_free(device->compiler);
}

static void *default_alloc(
    void*                                       pUserData,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocType                           allocType)
{
   return malloc(size);
}

static void default_free(
    void*                                       pUserData,
    void*                                       pMem)
{
   free(pMem);
}

static const VkAllocCallbacks default_alloc_callbacks = {
   .pUserData = NULL,
   .pfnAlloc = default_alloc,
   .pfnFree = default_free
};

static const VkExtensionProperties global_extensions[] = {
   {
      .extName = VK_EXT_KHR_SWAPCHAIN_EXTENSION_NAME,
      .specVersion = 17,
   },
};

static const VkExtensionProperties device_extensions[] = {
   {
      .extName = VK_EXT_KHR_DEVICE_SWAPCHAIN_EXTENSION_NAME,
      .specVersion = 53,
   },
};

VkResult anv_CreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    VkInstance*                                 pInstance)
{
   struct anv_instance *instance;
   const VkAllocCallbacks *alloc_callbacks = &default_alloc_callbacks;
   void *user_data = NULL;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

   if (pCreateInfo->pAppInfo->apiVersion != VK_MAKE_VERSION(0, 170, 2))
      return vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);

   for (uint32_t i = 0; i < pCreateInfo->extensionCount; i++) {
      bool found = false;
      for (uint32_t j = 0; j < ARRAY_SIZE(global_extensions); j++) {
         if (strcmp(pCreateInfo->ppEnabledExtensionNames[i],
                    global_extensions[j].extName) == 0) {
            found = true;
            break;
         }
      }
      if (!found)
         return vk_error(VK_ERROR_EXTENSION_NOT_PRESENT);
   }

   if (pCreateInfo->pAllocCb) {
      alloc_callbacks = pCreateInfo->pAllocCb;
      user_data = pCreateInfo->pAllocCb->pUserData;
   }
   instance = alloc_callbacks->pfnAlloc(user_data, sizeof(*instance), 8,
                                        VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (!instance)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   instance->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   instance->pAllocUserData = alloc_callbacks->pUserData;
   instance->pfnAlloc = alloc_callbacks->pfnAlloc;
   instance->pfnFree = alloc_callbacks->pfnFree;
   instance->apiVersion = pCreateInfo->pAppInfo->apiVersion;
   instance->physicalDeviceCount = -1;

   _mesa_locale_init();

   VG(VALGRIND_CREATE_MEMPOOL(instance, 0, false));

   anv_init_wsi(instance);

   *pInstance = anv_instance_to_handle(instance);

   return VK_SUCCESS;
}

void anv_DestroyInstance(
    VkInstance                                  _instance)
{
   ANV_FROM_HANDLE(anv_instance, instance, _instance);

   if (instance->physicalDeviceCount > 0) {
      /* We support at most one physical device. */
      assert(instance->physicalDeviceCount == 1);
      anv_physical_device_finish(&instance->physicalDevice);
   }

   anv_finish_wsi(instance);

   VG(VALGRIND_DESTROY_MEMPOOL(instance));

   _mesa_locale_fini();

   instance->pfnFree(instance->pAllocUserData, instance);
}

void *
anv_instance_alloc(struct anv_instance *instance, size_t size,
                   size_t alignment, VkSystemAllocType allocType)
{
   void *mem = instance->pfnAlloc(instance->pAllocUserData,
                                  size, alignment, allocType);
   if (mem) {
      VG(VALGRIND_MEMPOOL_ALLOC(instance, mem, size));
      VG(VALGRIND_MAKE_MEM_UNDEFINED(mem, size));
   }
   return mem;
}

void
anv_instance_free(struct anv_instance *instance, void *mem)
{
   if (mem == NULL)
      return;

   VG(VALGRIND_MEMPOOL_FREE(instance, mem));

   instance->pfnFree(instance->pAllocUserData, mem);
}

VkResult anv_EnumeratePhysicalDevices(
    VkInstance                                  _instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices)
{
   ANV_FROM_HANDLE(anv_instance, instance, _instance);
   VkResult result;

   if (instance->physicalDeviceCount < 0) {
      result = anv_physical_device_init(&instance->physicalDevice,
                                        instance, "/dev/dri/renderD128");
      if (result == VK_UNSUPPORTED) {
         instance->physicalDeviceCount = 0;
      } else if (result == VK_SUCCESS) {
         instance->physicalDeviceCount = 1;
      } else {
         return result;
      }
   }

   /* pPhysicalDeviceCount is an out parameter if pPhysicalDevices is NULL;
    * otherwise it's an inout parameter.
    *
    * The Vulkan spec (git aaed022) says:
    *
    *    pPhysicalDeviceCount is a pointer to an unsigned integer variable
    *    that is initialized with the number of devices the application is
    *    prepared to receive handles to. pname:pPhysicalDevices is pointer to
    *    an array of at least this many VkPhysicalDevice handles [...].
    *
    *    Upon success, if pPhysicalDevices is NULL, vkEnumeratePhysicalDevices
    *    overwrites the contents of the variable pointed to by
    *    pPhysicalDeviceCount with the number of physical devices in in the
    *    instance; otherwise, vkEnumeratePhysicalDevices overwrites
    *    pPhysicalDeviceCount with the number of physical handles written to
    *    pPhysicalDevices.
    */
   if (!pPhysicalDevices) {
      *pPhysicalDeviceCount = instance->physicalDeviceCount;
   } else if (*pPhysicalDeviceCount >= 1) {
      pPhysicalDevices[0] = anv_physical_device_to_handle(&instance->physicalDevice);
      *pPhysicalDeviceCount = 1;
   } else {
      *pPhysicalDeviceCount = 0;
   }

   return VK_SUCCESS;
}

void anv_GetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
   anv_finishme("Get correct values for PhysicalDeviceFeatures");

   *pFeatures = (VkPhysicalDeviceFeatures) {
      .robustBufferAccess                       = false,
      .fullDrawIndexUint32                      = false,
      .imageCubeArray                           = false,
      .independentBlend                         = false,
      .geometryShader                           = true,
      .tessellationShader                       = false,
      .sampleRateShading                        = false,
      .dualSourceBlend                          = true,
      .logicOp                                  = true,
      .multiDrawIndirect                        = true,
      .depthClip                                = false,
      .depthBiasClamp                           = false,
      .fillModeNonSolid                         = true,
      .depthBounds                              = false,
      .wideLines                                = true,
      .largePoints                              = true,
      .textureCompressionETC2                   = true,
      .textureCompressionASTC_LDR               = true,
      .textureCompressionBC                     = true,
      .occlusionQueryNonConservative            = false, /* FINISHME */
      .pipelineStatisticsQuery                  = true,
      .vertexSideEffects                        = false,
      .tessellationSideEffects                  = false,
      .geometrySideEffects                      = false,
      .fragmentSideEffects                      = false,
      .shaderTessellationPointSize              = false,
      .shaderGeometryPointSize                  = true,
      .shaderImageGatherExtended                = true,
      .shaderStorageImageExtendedFormats        = false,
      .shaderStorageImageMultisample            = false,
      .shaderUniformBufferArrayDynamicIndexing  = true,
      .shaderSampledImageArrayDynamicIndexing   = false,
      .shaderStorageBufferArrayDynamicIndexing  = false,
      .shaderStorageImageArrayDynamicIndexing   = false,
      .shaderClipDistance                       = false,
      .shaderCullDistance                       = false,
      .shaderFloat64                            = false,
      .shaderInt64                              = false,
      .shaderInt16                              = false,
      .alphaToOne                               = true,
   };
}

void anv_GetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
   ANV_FROM_HANDLE(anv_physical_device, pdevice, physicalDevice);
   const struct brw_device_info *devinfo = pdevice->info;

   anv_finishme("Get correct values for VkPhysicalDeviceLimits");

   VkPhysicalDeviceLimits limits = {
      .maxImageDimension1D                      = (1 << 14),
      .maxImageDimension2D                      = (1 << 14),
      .maxImageDimension3D                      = (1 << 10),
      .maxImageDimensionCube                    = (1 << 14),
      .maxImageArrayLayers                      = (1 << 10),

      /* Broadwell supports 1, 2, 4, and 8 samples. */
      .sampleCounts                             = 4,

      .maxTexelBufferSize                       = (1 << 14),
      .maxUniformBufferSize                     = UINT32_MAX,
      .maxStorageBufferSize                     = UINT32_MAX,
      .maxPushConstantsSize                     = MAX_PUSH_CONSTANTS_SIZE,
      .maxMemoryAllocationCount                 = UINT32_MAX,
      .bufferImageGranularity                   = 64, /* A cache line */
      .sparseAddressSpaceSize                   = 0,
      .maxBoundDescriptorSets                   = MAX_SETS,
      .maxDescriptorSets                        = UINT32_MAX,
      .maxPerStageDescriptorSamplers            = 64,
      .maxPerStageDescriptorUniformBuffers      = 64,
      .maxPerStageDescriptorStorageBuffers      = 64,
      .maxPerStageDescriptorSampledImages       = 64,
      .maxPerStageDescriptorStorageImages       = 64,
      .maxDescriptorSetSamplers                 = 256,
      .maxDescriptorSetUniformBuffers           = 256,
      .maxDescriptorSetUniformBuffersDynamic    = 256,
      .maxDescriptorSetStorageBuffers           = 256,
      .maxDescriptorSetStorageBuffersDynamic    = 256,
      .maxDescriptorSetSampledImages            = 256,
      .maxDescriptorSetStorageImages            = 256,
      .maxVertexInputAttributes                 = 32,
      .maxVertexInputBindings                   = 32,
      .maxVertexInputAttributeOffset            = 256,
      .maxVertexInputBindingStride              = 256,
      .maxVertexOutputComponents                = 32,
      .maxTessGenLevel                          = 0,
      .maxTessPatchSize                         = 0,
      .maxTessControlPerVertexInputComponents   = 0,
      .maxTessControlPerVertexOutputComponents  = 0,
      .maxTessControlPerPatchOutputComponents   = 0,
      .maxTessControlTotalOutputComponents      = 0,
      .maxTessEvaluationInputComponents         = 0,
      .maxTessEvaluationOutputComponents        = 0,
      .maxGeometryShaderInvocations             = 6,
      .maxGeometryInputComponents               = 16,
      .maxGeometryOutputComponents              = 16,
      .maxGeometryOutputVertices                = 16,
      .maxGeometryTotalOutputComponents         = 16,
      .maxFragmentInputComponents               = 16,
      .maxFragmentOutputBuffers                 = 8,
      .maxFragmentDualSourceBuffers             = 2,
      .maxFragmentCombinedOutputResources       = 8,
      .maxComputeSharedMemorySize               = 1024,
      .maxComputeWorkGroupCount = {
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
      },
      .maxComputeWorkGroupInvocations           = 16 * devinfo->max_cs_threads,
      .maxComputeWorkGroupSize = {
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
      },
      .subPixelPrecisionBits                    = 4 /* FIXME */,
      .subTexelPrecisionBits                    = 4 /* FIXME */,
      .mipmapPrecisionBits                      = 4 /* FIXME */,
      .maxDrawIndexedIndexValue                 = UINT32_MAX,
      .maxDrawIndirectInstanceCount             = UINT32_MAX,
      .primitiveRestartForPatches               = UINT32_MAX,
      .maxSamplerLodBias                        = 16,
      .maxSamplerAnisotropy                     = 16,
      .maxViewports                             = MAX_VIEWPORTS,
      .maxViewportDimensions                    = { (1 << 14), (1 << 14) },
      .viewportBoundsRange                      = { -1.0, 1.0 }, /* FIXME */
      .viewportSubPixelBits                     = 13, /* We take a float? */
      .minMemoryMapAlignment                    = 64, /* A cache line */
      .minTexelBufferOffsetAlignment            = 1,
      .minUniformBufferOffsetAlignment          = 1,
      .minStorageBufferOffsetAlignment          = 1,
      .minTexelOffset                           = 0, /* FIXME */
      .maxTexelOffset                           = 0, /* FIXME */
      .minTexelGatherOffset                     = 0, /* FIXME */
      .maxTexelGatherOffset                     = 0, /* FIXME */
      .minInterpolationOffset                   = 0, /* FIXME */
      .maxInterpolationOffset                   = 0, /* FIXME */
      .subPixelInterpolationOffsetBits          = 0, /* FIXME */
      .maxFramebufferWidth                      = (1 << 14),
      .maxFramebufferHeight                     = (1 << 14),
      .maxFramebufferLayers                     = (1 << 10),
      .maxFramebufferColorSamples               = 8,
      .maxFramebufferDepthSamples               = 8,
      .maxFramebufferStencilSamples             = 8,
      .maxColorAttachments                      = MAX_RTS,
      .maxSampledImageColorSamples              = 8,
      .maxSampledImageDepthSamples              = 8,
      .maxSampledImageIntegerSamples            = 1,
      .maxStorageImageSamples                   = 1,
      .maxSampleMaskWords                       = 1,
      .timestampFrequency                       = 1000 * 1000 * 1000 / 80,
      .maxClipDistances                         = 0 /* FIXME */,
      .maxCullDistances                         = 0 /* FIXME */,
      .maxCombinedClipAndCullDistances          = 0 /* FIXME */,
      .pointSizeRange                           = { 0.125, 255.875 },
      .lineWidthRange                           = { 0.0, 7.9921875 },
      .pointSizeGranularity                     = (1.0 / 8.0),
      .lineWidthGranularity                     = (1.0 / 128.0),
   };

   *pProperties = (VkPhysicalDeviceProperties) {
      .apiVersion = VK_MAKE_VERSION(0, 170, 2),
      .driverVersion = 1,
      .vendorId = 0x8086,
      .deviceId = pdevice->chipset_id,
      .deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
      .limits = limits,
      .sparseProperties = {0}, /* Broadwell doesn't do sparse. */
   };

   strcpy(pProperties->deviceName, pdevice->name);
   snprintf((char *)pProperties->pipelineCacheUUID, VK_UUID_LENGTH,
            "anv-%s", MESA_GIT_SHA1 + 4);
}

void anv_GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
   if (pQueueFamilyProperties == NULL) {
      *pCount = 1;
      return;
   }

   assert(*pCount >= 1);

   *pQueueFamilyProperties = (VkQueueFamilyProperties) {
      .queueFlags = VK_QUEUE_GRAPHICS_BIT |
                    VK_QUEUE_COMPUTE_BIT |
                    VK_QUEUE_TRANSFER_BIT,
      .queueCount = 1,
      .supportsTimestamps = true,
   };
}

void anv_GetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
   ANV_FROM_HANDLE(anv_physical_device, physical_device, physicalDevice);
   VkDeviceSize heap_size;

   /* Reserve some wiggle room for the driver by exposing only 75% of the
    * aperture to the heap.
    */
   heap_size = 3 * physical_device->aperture_size / 4;

   /* The property flags below are valid only for llc platforms. */
   pMemoryProperties->memoryTypeCount = 1;
   pMemoryProperties->memoryTypes[0] = (VkMemoryType) {
      .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      .heapIndex = 1,
   };

   pMemoryProperties->memoryHeapCount = 1;
   pMemoryProperties->memoryHeaps[0] = (VkMemoryHeap) {
      .size = heap_size,
      .flags = VK_MEMORY_HEAP_HOST_LOCAL_BIT,
   };
}

PFN_vkVoidFunction anv_GetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
   return anv_lookup_entrypoint(pName);
}

PFN_vkVoidFunction anv_GetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName)
{
   return anv_lookup_entrypoint(pName);
}

static VkResult
anv_queue_init(struct anv_device *device, struct anv_queue *queue)
{
   queue->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   queue->device = device;
   queue->pool = &device->surface_state_pool;

   return VK_SUCCESS;
}

static void
anv_queue_finish(struct anv_queue *queue)
{
}

static void
anv_device_init_border_colors(struct anv_device *device)
{
   static const VkClearColorValue border_colors[] = {
      [VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK] =  { .float32 = { 0.0, 0.0, 0.0, 0.0 } },
      [VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK] =       { .float32 = { 0.0, 0.0, 0.0, 1.0 } },
      [VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE] =       { .float32 = { 1.0, 1.0, 1.0, 1.0 } },
      [VK_BORDER_COLOR_INT_TRANSPARENT_BLACK] =    { .uint32 = { 0, 0, 0, 0 } },
      [VK_BORDER_COLOR_INT_OPAQUE_BLACK] =         { .uint32 = { 0, 0, 0, 1 } },
      [VK_BORDER_COLOR_INT_OPAQUE_WHITE] =         { .uint32 = { 1, 1, 1, 1 } },
   };

   device->border_colors =
      anv_state_pool_alloc(&device->dynamic_state_pool,
                           sizeof(border_colors), 32);
   memcpy(device->border_colors.map, border_colors, sizeof(border_colors));
}

VkResult anv_CreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    VkDevice*                                   pDevice)
{
   ANV_FROM_HANDLE(anv_physical_device, physical_device, physicalDevice);
   struct anv_instance *instance = physical_device->instance;
   struct anv_device *device;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

   for (uint32_t i = 0; i < pCreateInfo->extensionCount; i++) {
      bool found = false;
      for (uint32_t j = 0; j < ARRAY_SIZE(device_extensions); j++) {
         if (strcmp(pCreateInfo->ppEnabledExtensionNames[i],
                    device_extensions[j].extName) == 0) {
            found = true;
            break;
         }
      }
      if (!found)
         return vk_error(VK_ERROR_EXTENSION_NOT_PRESENT);
   }

   anv_set_dispatch_devinfo(physical_device->info);

   device = anv_instance_alloc(instance, sizeof(*device), 8,
                               VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (!device)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   device->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   device->instance = physical_device->instance;

   /* XXX(chadv): Can we dup() physicalDevice->fd here? */
   device->fd = open(physical_device->path, O_RDWR | O_CLOEXEC);
   if (device->fd == -1)
      goto fail_device;

   device->context_id = anv_gem_create_context(device);
   if (device->context_id == -1)
      goto fail_fd;

   pthread_mutex_init(&device->mutex, NULL);

   anv_bo_pool_init(&device->batch_bo_pool, device, ANV_CMD_BUFFER_BATCH_SIZE);

   anv_block_pool_init(&device->dynamic_state_block_pool, device, 2048);

   anv_state_pool_init(&device->dynamic_state_pool,
                       &device->dynamic_state_block_pool);

   anv_block_pool_init(&device->instruction_block_pool, device, 4096);
   anv_block_pool_init(&device->surface_state_block_pool, device, 4096);

   anv_state_pool_init(&device->surface_state_pool,
                       &device->surface_state_block_pool);

   anv_bo_init_new(&device->workaround_bo, device, 1024);

   anv_block_pool_init(&device->scratch_block_pool, device, 0x10000);

   device->info = *physical_device->info;
   device->isl_dev = physical_device->isl_dev;

   anv_queue_init(device, &device->queue);

   anv_device_init_meta(device);

   anv_device_init_border_colors(device);

   *pDevice = anv_device_to_handle(device);

   return VK_SUCCESS;

 fail_fd:
   close(device->fd);
 fail_device:
   anv_device_free(device, device);

   return vk_error(VK_ERROR_INITIALIZATION_FAILED);
}

void anv_DestroyDevice(
    VkDevice                                    _device)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   anv_queue_finish(&device->queue);

   anv_device_finish_meta(device);

#ifdef HAVE_VALGRIND
   /* We only need to free these to prevent valgrind errors.  The backing
    * BO will go away in a couple of lines so we don't actually leak.
    */
   anv_state_pool_free(&device->dynamic_state_pool, device->border_colors);
#endif

   anv_gem_munmap(device->workaround_bo.map, device->workaround_bo.size);
   anv_gem_close(device, device->workaround_bo.gem_handle);

   anv_bo_pool_finish(&device->batch_bo_pool);
   anv_state_pool_finish(&device->dynamic_state_pool);
   anv_block_pool_finish(&device->dynamic_state_block_pool);
   anv_block_pool_finish(&device->instruction_block_pool);
   anv_state_pool_finish(&device->surface_state_pool);
   anv_block_pool_finish(&device->surface_state_block_pool);
   anv_block_pool_finish(&device->scratch_block_pool);

   close(device->fd);

   anv_instance_free(device->instance, device);
}

VkResult anv_EnumerateInstanceExtensionProperties(
    const char*                                 pLayerName,
    uint32_t*                                   pCount,
    VkExtensionProperties*                      pProperties)
{
   if (pProperties == NULL) {
      *pCount = ARRAY_SIZE(global_extensions);
      return VK_SUCCESS;
   }

   assert(*pCount >= ARRAY_SIZE(global_extensions));

   *pCount = ARRAY_SIZE(global_extensions);
   memcpy(pProperties, global_extensions, sizeof(global_extensions));

   return VK_SUCCESS;
}

VkResult anv_EnumerateDeviceExtensionProperties(
    VkPhysicalDevice                            physicalDevice,
    const char*                                 pLayerName,
    uint32_t*                                   pCount,
    VkExtensionProperties*                      pProperties)
{
   if (pProperties == NULL) {
      *pCount = ARRAY_SIZE(device_extensions);
      return VK_SUCCESS;
   }

   assert(*pCount >= ARRAY_SIZE(device_extensions));

   *pCount = ARRAY_SIZE(device_extensions);
   memcpy(pProperties, device_extensions, sizeof(device_extensions));

   return VK_SUCCESS;
}

VkResult anv_EnumerateInstanceLayerProperties(
    uint32_t*                                   pCount,
    VkLayerProperties*                          pProperties)
{
   if (pProperties == NULL) {
      *pCount = 0;
      return VK_SUCCESS;
   }

   /* None supported at this time */
   return vk_error(VK_ERROR_LAYER_NOT_PRESENT);
}

VkResult anv_EnumerateDeviceLayerProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkLayerProperties*                          pProperties)
{
   if (pProperties == NULL) {
      *pCount = 0;
      return VK_SUCCESS;
   }

   /* None supported at this time */
   return vk_error(VK_ERROR_LAYER_NOT_PRESENT);
}

void anv_GetDeviceQueue(
    VkDevice                                    _device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   assert(queueIndex == 0);

   *pQueue = anv_queue_to_handle(&device->queue);
}

VkResult anv_QueueSubmit(
    VkQueue                                     _queue,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers,
    VkFence                                     _fence)
{
   ANV_FROM_HANDLE(anv_queue, queue, _queue);
   ANV_FROM_HANDLE(anv_fence, fence, _fence);
   struct anv_device *device = queue->device;
   int ret;

   for (uint32_t i = 0; i < commandBufferCount; i++) {
      ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, pCommandBuffers[i]);

      assert(cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);

      ret = anv_gem_execbuffer(device, &cmd_buffer->execbuf2.execbuf);
      if (ret != 0) {
         /* We don't know the real error. */
         return vk_errorf(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                          "execbuf2 failed: %m");
      }

      if (fence) {
         ret = anv_gem_execbuffer(device, &fence->execbuf);
         if (ret != 0) {
            /* We don't know the real error. */
            return vk_errorf(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                             "execbuf2 failed: %m");
         }
      }

      for (uint32_t i = 0; i < cmd_buffer->execbuf2.bo_count; i++)
         cmd_buffer->execbuf2.bos[i]->offset = cmd_buffer->execbuf2.objects[i].offset;
   }

   return VK_SUCCESS;
}

VkResult anv_QueueWaitIdle(
    VkQueue                                     _queue)
{
   ANV_FROM_HANDLE(anv_queue, queue, _queue);

   return ANV_CALL(DeviceWaitIdle)(anv_device_to_handle(queue->device));
}

VkResult anv_DeviceWaitIdle(
    VkDevice                                    _device)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_state state;
   struct anv_batch batch;
   struct drm_i915_gem_execbuffer2 execbuf;
   struct drm_i915_gem_exec_object2 exec2_objects[1];
   struct anv_bo *bo = NULL;
   VkResult result;
   int64_t timeout;
   int ret;

   state = anv_state_pool_alloc(&device->dynamic_state_pool, 32, 32);
   bo = &device->dynamic_state_pool.block_pool->bo;
   batch.start = batch.next = state.map;
   batch.end = state.map + 32;
   anv_batch_emit(&batch, GEN7_MI_BATCH_BUFFER_END);
   anv_batch_emit(&batch, GEN7_MI_NOOP);

   exec2_objects[0].handle = bo->gem_handle;
   exec2_objects[0].relocation_count = 0;
   exec2_objects[0].relocs_ptr = 0;
   exec2_objects[0].alignment = 0;
   exec2_objects[0].offset = bo->offset;
   exec2_objects[0].flags = 0;
   exec2_objects[0].rsvd1 = 0;
   exec2_objects[0].rsvd2 = 0;

   execbuf.buffers_ptr = (uintptr_t) exec2_objects;
   execbuf.buffer_count = 1;
   execbuf.batch_start_offset = state.offset;
   execbuf.batch_len = batch.next - state.map;
   execbuf.cliprects_ptr = 0;
   execbuf.num_cliprects = 0;
   execbuf.DR1 = 0;
   execbuf.DR4 = 0;

   execbuf.flags =
      I915_EXEC_HANDLE_LUT | I915_EXEC_NO_RELOC | I915_EXEC_RENDER;
   execbuf.rsvd1 = device->context_id;
   execbuf.rsvd2 = 0;

   ret = anv_gem_execbuffer(device, &execbuf);
   if (ret != 0) {
      /* We don't know the real error. */
      result = vk_errorf(VK_ERROR_OUT_OF_DEVICE_MEMORY, "execbuf2 failed: %m");
      goto fail;
   }

   timeout = INT64_MAX;
   ret = anv_gem_wait(device, bo->gem_handle, &timeout);
   if (ret != 0) {
      /* We don't know the real error. */
      result = vk_errorf(VK_ERROR_OUT_OF_DEVICE_MEMORY, "execbuf2 failed: %m");
      goto fail;
   }

   anv_state_pool_free(&device->dynamic_state_pool, state);

   return VK_SUCCESS;

 fail:
   anv_state_pool_free(&device->dynamic_state_pool, state);

   return result;
}

void *
anv_device_alloc(struct anv_device *            device,
                 size_t                         size,
                 size_t                         alignment,
                 VkSystemAllocType              allocType)
{
   return anv_instance_alloc(device->instance, size, alignment, allocType);
}

void
anv_device_free(struct anv_device *             device,
                void *                          mem)
{
   anv_instance_free(device->instance, mem);
}

VkResult
anv_bo_init_new(struct anv_bo *bo, struct anv_device *device, uint64_t size)
{
   bo->gem_handle = anv_gem_create(device, size);
   if (!bo->gem_handle)
      return vk_error(VK_ERROR_OUT_OF_DEVICE_MEMORY);

   bo->map = NULL;
   bo->index = 0;
   bo->offset = 0;
   bo->size = size;

   return VK_SUCCESS;
}

VkResult anv_AllocMemory(
    VkDevice                                    _device,
    const VkMemoryAllocInfo*                    pAllocInfo,
    VkDeviceMemory*                             pMem)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_device_memory *mem;
   VkResult result;

   assert(pAllocInfo->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO);

   /* We support exactly one memory heap. */
   assert(pAllocInfo->memoryTypeIndex == 0);

   /* FINISHME: Fail if allocation request exceeds heap size. */

   mem = anv_device_alloc(device, sizeof(*mem), 8,
                          VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (mem == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   result = anv_bo_init_new(&mem->bo, device, pAllocInfo->allocationSize);
   if (result != VK_SUCCESS)
      goto fail;

   *pMem = anv_device_memory_to_handle(mem);

   return VK_SUCCESS;

 fail:
   anv_device_free(device, mem);

   return result;
}

void anv_FreeMemory(
    VkDevice                                    _device,
    VkDeviceMemory                              _mem)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_device_memory, mem, _mem);

   if (mem->bo.map)
      anv_gem_munmap(mem->bo.map, mem->bo.size);

   if (mem->bo.gem_handle != 0)
      anv_gem_close(device, mem->bo.gem_handle);

   anv_device_free(device, mem);
}

VkResult anv_MapMemory(
    VkDevice                                    _device,
    VkDeviceMemory                              _mem,
    VkDeviceSize                                offset,
    VkDeviceSize                                size,
    VkMemoryMapFlags                            flags,
    void**                                      ppData)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_device_memory, mem, _mem);

   /* FIXME: Is this supposed to be thread safe? Since vkUnmapMemory() only
    * takes a VkDeviceMemory pointer, it seems like only one map of the memory
    * at a time is valid. We could just mmap up front and return an offset
    * pointer here, but that may exhaust virtual memory on 32 bit
    * userspace. */

   mem->map = anv_gem_mmap(device, mem->bo.gem_handle, offset, size);
   mem->map_size = size;

   *ppData = mem->map;

   return VK_SUCCESS;
}

void anv_UnmapMemory(
    VkDevice                                    _device,
    VkDeviceMemory                              _mem)
{
   ANV_FROM_HANDLE(anv_device_memory, mem, _mem);

   anv_gem_munmap(mem->map, mem->map_size);
}

VkResult anv_FlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memRangeCount,
    const VkMappedMemoryRange*                  pMemRanges)
{
   /* clflush here for !llc platforms */

   return VK_SUCCESS;
}

VkResult anv_InvalidateMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memRangeCount,
    const VkMappedMemoryRange*                  pMemRanges)
{
   return anv_FlushMappedMemoryRanges(device, memRangeCount, pMemRanges);
}

void anv_GetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    _buffer,
    VkMemoryRequirements*                       pMemoryRequirements)
{
   ANV_FROM_HANDLE(anv_buffer, buffer, _buffer);

   /* The Vulkan spec (git aaed022) says:
    *
    *    memoryTypeBits is a bitfield and contains one bit set for every
    *    supported memory type for the resource. The bit `1<<i` is set if and
    *    only if the memory type `i` in the VkPhysicalDeviceMemoryProperties
    *    structure for the physical device is supported.
    *
    * We support exactly one memory type.
    */
   pMemoryRequirements->memoryTypeBits = 1;

   pMemoryRequirements->size = buffer->size;
   pMemoryRequirements->alignment = 16;
}

void anv_GetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     _image,
    VkMemoryRequirements*                       pMemoryRequirements)
{
   ANV_FROM_HANDLE(anv_image, image, _image);

   /* The Vulkan spec (git aaed022) says:
    *
    *    memoryTypeBits is a bitfield and contains one bit set for every
    *    supported memory type for the resource. The bit `1<<i` is set if and
    *    only if the memory type `i` in the VkPhysicalDeviceMemoryProperties
    *    structure for the physical device is supported.
    *
    * We support exactly one memory type.
    */
   pMemoryRequirements->memoryTypeBits = 1;

   pMemoryRequirements->size = image->size;
   pMemoryRequirements->alignment = image->alignment;
}

void anv_GetImageSparseMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    uint32_t*                                   pNumRequirements,
    VkSparseImageMemoryRequirements*            pSparseMemoryRequirements)
{
   stub();
}

void anv_GetDeviceMemoryCommitment(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize*                               pCommittedMemoryInBytes)
{
   *pCommittedMemoryInBytes = 0;
}

VkResult anv_BindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    _buffer,
    VkDeviceMemory                              _mem,
    VkDeviceSize                                memOffset)
{
   ANV_FROM_HANDLE(anv_device_memory, mem, _mem);
   ANV_FROM_HANDLE(anv_buffer, buffer, _buffer);

   buffer->bo = &mem->bo;
   buffer->offset = memOffset;

   return VK_SUCCESS;
}

VkResult anv_BindImageMemory(
    VkDevice                                    device,
    VkImage                                     _image,
    VkDeviceMemory                              _mem,
    VkDeviceSize                                memOffset)
{
   ANV_FROM_HANDLE(anv_device_memory, mem, _mem);
   ANV_FROM_HANDLE(anv_image, image, _image);

   image->bo = &mem->bo;
   image->offset = memOffset;

   return VK_SUCCESS;
}

VkResult anv_QueueBindSparseBufferMemory(
    VkQueue                                     queue,
    VkBuffer                                    buffer,
    uint32_t                                    numBindings,
    const VkSparseMemoryBindInfo*               pBindInfo)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_QueueBindSparseImageOpaqueMemory(
    VkQueue                                     queue,
    VkImage                                     image,
    uint32_t                                    numBindings,
    const VkSparseMemoryBindInfo*               pBindInfo)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_QueueBindSparseImageMemory(
    VkQueue                                     queue,
    VkImage                                     image,
    uint32_t                                    numBindings,
    const VkSparseImageMemoryBindInfo*          pBindInfo)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_CreateFence(
    VkDevice                                    _device,
    const VkFenceCreateInfo*                    pCreateInfo,
    VkFence*                                    pFence)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_fence *fence;
   struct anv_batch batch;
   VkResult result;

   const uint32_t fence_size = 128;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);

   fence = anv_device_alloc(device, sizeof(*fence), 8,
                            VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (fence == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   result = anv_bo_init_new(&fence->bo, device, fence_size);
   if (result != VK_SUCCESS)
      goto fail;

   fence->bo.map =
      anv_gem_mmap(device, fence->bo.gem_handle, 0, fence->bo.size);
   batch.next = batch.start = fence->bo.map;
   batch.end = fence->bo.map + fence->bo.size;
   anv_batch_emit(&batch, GEN7_MI_BATCH_BUFFER_END);
   anv_batch_emit(&batch, GEN7_MI_NOOP);

   fence->exec2_objects[0].handle = fence->bo.gem_handle;
   fence->exec2_objects[0].relocation_count = 0;
   fence->exec2_objects[0].relocs_ptr = 0;
   fence->exec2_objects[0].alignment = 0;
   fence->exec2_objects[0].offset = fence->bo.offset;
   fence->exec2_objects[0].flags = 0;
   fence->exec2_objects[0].rsvd1 = 0;
   fence->exec2_objects[0].rsvd2 = 0;

   fence->execbuf.buffers_ptr = (uintptr_t) fence->exec2_objects;
   fence->execbuf.buffer_count = 1;
   fence->execbuf.batch_start_offset = 0;
   fence->execbuf.batch_len = batch.next - fence->bo.map;
   fence->execbuf.cliprects_ptr = 0;
   fence->execbuf.num_cliprects = 0;
   fence->execbuf.DR1 = 0;
   fence->execbuf.DR4 = 0;

   fence->execbuf.flags =
      I915_EXEC_HANDLE_LUT | I915_EXEC_NO_RELOC | I915_EXEC_RENDER;
   fence->execbuf.rsvd1 = device->context_id;
   fence->execbuf.rsvd2 = 0;

   *pFence = anv_fence_to_handle(fence);

   return VK_SUCCESS;

 fail:
   anv_device_free(device, fence);

   return result;
}

void anv_DestroyFence(
    VkDevice                                    _device,
    VkFence                                     _fence)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_fence, fence, _fence);

   anv_gem_munmap(fence->bo.map, fence->bo.size);
   anv_gem_close(device, fence->bo.gem_handle);
   anv_device_free(device, fence);
}

VkResult anv_ResetFences(
    VkDevice                                    _device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences)
{
   for (uint32_t i = 0; i < fenceCount; i++) {
      ANV_FROM_HANDLE(anv_fence, fence, pFences[i]);
      fence->ready = false;
   }

   return VK_SUCCESS;
}

VkResult anv_GetFenceStatus(
    VkDevice                                    _device,
    VkFence                                     _fence)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_fence, fence, _fence);
   int64_t t = 0;
   int ret;

   if (fence->ready)
      return VK_SUCCESS;

   ret = anv_gem_wait(device, fence->bo.gem_handle, &t);
   if (ret == 0) {
      fence->ready = true;
      return VK_SUCCESS;
   }

   return VK_NOT_READY;
}

VkResult anv_WaitForFences(
    VkDevice                                    _device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences,
    VkBool32                                    waitAll,
    uint64_t                                    timeout)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   /* DRM_IOCTL_I915_GEM_WAIT uses a signed 64 bit timeout and is supposed
    * to block indefinitely timeouts <= 0.  Unfortunately, this was broken
    * for a couple of kernel releases.  Since there's no way to know
    * whether or not the kernel we're using is one of the broken ones, the
    * best we can do is to clamp the timeout to INT64_MAX.  This limits the
    * maximum timeout from 584 years to 292 years - likely not a big deal.
    */
   if (timeout > INT64_MAX)
      timeout = INT64_MAX;

   int64_t t = timeout;

   /* FIXME: handle !waitAll */

   for (uint32_t i = 0; i < fenceCount; i++) {
      ANV_FROM_HANDLE(anv_fence, fence, pFences[i]);
      int ret = anv_gem_wait(device, fence->bo.gem_handle, &t);
      if (ret == -1 && errno == ETIME) {
         return VK_TIMEOUT;
      } else if (ret == -1) {
         /* We don't know the real error. */
         return vk_errorf(VK_ERROR_OUT_OF_DEVICE_MEMORY,
                          "gem wait failed: %m");
      }
   }

   return VK_SUCCESS;
}

// Queue semaphore functions

VkResult anv_CreateSemaphore(
    VkDevice                                    device,
    const VkSemaphoreCreateInfo*                pCreateInfo,
    VkSemaphore*                                pSemaphore)
{
   *pSemaphore = (VkSemaphore)1;
   stub_return(VK_SUCCESS);
}

void anv_DestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore)
{
   stub();
}

VkResult anv_QueueSignalSemaphore(
    VkQueue                                     queue,
    VkSemaphore                                 semaphore)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_QueueWaitSemaphore(
    VkQueue                                     queue,
    VkSemaphore                                 semaphore)
{
   stub_return(VK_UNSUPPORTED);
}

// Event functions

VkResult anv_CreateEvent(
    VkDevice                                    device,
    const VkEventCreateInfo*                    pCreateInfo,
    VkEvent*                                    pEvent)
{
   stub_return(VK_UNSUPPORTED);
}

void anv_DestroyEvent(
    VkDevice                                    device,
    VkEvent                                     event)
{
   stub();
}

VkResult anv_GetEventStatus(
    VkDevice                                    device,
    VkEvent                                     event)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_SetEvent(
    VkDevice                                    device,
    VkEvent                                     event)
{
   stub_return(VK_UNSUPPORTED);
}

VkResult anv_ResetEvent(
    VkDevice                                    device,
    VkEvent                                     event)
{
   stub_return(VK_UNSUPPORTED);
}

// Buffer functions

VkResult anv_CreateBuffer(
    VkDevice                                    _device,
    const VkBufferCreateInfo*                   pCreateInfo,
    VkBuffer*                                   pBuffer)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_buffer *buffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

   buffer = anv_device_alloc(device, sizeof(*buffer), 8,
                            VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (buffer == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   buffer->size = pCreateInfo->size;
   buffer->bo = NULL;
   buffer->offset = 0;

   *pBuffer = anv_buffer_to_handle(buffer);

   return VK_SUCCESS;
}

void anv_DestroyBuffer(
    VkDevice                                    _device,
    VkBuffer                                    _buffer)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_buffer, buffer, _buffer);

   anv_device_free(device, buffer);
}

void
anv_fill_buffer_surface_state(struct anv_device *device, void *state,
                              const struct anv_format *format,
                              uint32_t offset, uint32_t range, uint32_t stride)
{
   switch (device->info.gen) {
   case 7:
      if (device->info.is_haswell)
         gen75_fill_buffer_surface_state(state, format, offset, range, stride);
      else
         gen7_fill_buffer_surface_state(state, format, offset, range, stride);
      break;
   case 8:
      gen8_fill_buffer_surface_state(state, format, offset, range, stride);
      break;
   case 9:
      gen9_fill_buffer_surface_state(state, format, offset, range, stride);
      break;
   default:
      unreachable("unsupported gen\n");
   }
}

VkResult anv_CreateBufferView(
    VkDevice                                    _device,
    const VkBufferViewCreateInfo*               pCreateInfo,
    VkBufferView*                               pView)
{
   stub_return(VK_UNSUPPORTED);
}

void anv_DestroyBufferView(
    VkDevice                                    _device,
    VkBufferView                                _bview)
{
   stub();
}

void anv_DestroySampler(
    VkDevice                                    _device,
    VkSampler                                   _sampler)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_sampler, sampler, _sampler);

   anv_device_free(device, sampler);
}

VkResult anv_CreateFramebuffer(
    VkDevice                                    _device,
    const VkFramebufferCreateInfo*              pCreateInfo,
    VkFramebuffer*                              pFramebuffer)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_framebuffer *framebuffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);

   size_t size = sizeof(*framebuffer) +
                 sizeof(struct anv_image_view *) * pCreateInfo->attachmentCount;
   framebuffer = anv_device_alloc(device, size, 8,
                                  VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (framebuffer == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   framebuffer->attachment_count = pCreateInfo->attachmentCount;
   for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++) {
      VkImageView _iview = pCreateInfo->pAttachments[i];
      framebuffer->attachments[i] = anv_image_view_from_handle(_iview);
   }

   framebuffer->width = pCreateInfo->width;
   framebuffer->height = pCreateInfo->height;
   framebuffer->layers = pCreateInfo->layers;

   *pFramebuffer = anv_framebuffer_to_handle(framebuffer);

   return VK_SUCCESS;
}

void anv_DestroyFramebuffer(
    VkDevice                                    _device,
    VkFramebuffer                               _fb)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_framebuffer, fb, _fb);

   anv_device_free(device, fb);
}

void vkCmdDbgMarkerBegin(
    VkCommandBuffer                              commandBuffer,
    const char*                                 pMarker)
   __attribute__ ((visibility ("default")));

void vkCmdDbgMarkerEnd(
   VkCommandBuffer                              commandBuffer)
   __attribute__ ((visibility ("default")));

void vkCmdDbgMarkerBegin(
    VkCommandBuffer                              commandBuffer,
    const char*                                 pMarker)
{
}

void vkCmdDbgMarkerEnd(
    VkCommandBuffer                              commandBuffer)
{
}