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

/*
 * Descriptor set layouts.
 */

VkResult anv_CreateDescriptorSetLayout(
    VkDevice                                    _device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    VkDescriptorSetLayout*                      pSetLayout)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_descriptor_set_layout *set_layout;
   uint32_t s;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);

   uint32_t immutable_sampler_count = 0;
   for (uint32_t b = 0; b < pCreateInfo->count; b++) {
      if (pCreateInfo->pBinding[b].pImmutableSamplers)
         immutable_sampler_count += pCreateInfo->pBinding[b].arraySize;
   }

   size_t size = sizeof(struct anv_descriptor_set_layout) +
                 pCreateInfo->count * sizeof(set_layout->binding[0]) +
                 immutable_sampler_count * sizeof(struct anv_sampler *);

   set_layout = anv_device_alloc(device, size, 8,
                                 VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (!set_layout)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* We just allocate all the samplers at the end of the struct */
   struct anv_sampler **samplers =
      (struct anv_sampler **)&set_layout->binding[pCreateInfo->count];

   set_layout->binding_count = pCreateInfo->count;
   set_layout->shader_stages = 0;
   set_layout->size = 0;

   /* Initialize all binding_layout entries to -1 */
   memset(set_layout->binding, -1,
          pCreateInfo->count * sizeof(set_layout->binding[0]));

   /* Initialize all samplers to 0 */
   memset(samplers, 0, immutable_sampler_count * sizeof(*samplers));

   uint32_t sampler_count[VK_SHADER_STAGE_NUM] = { 0, };
   uint32_t surface_count[VK_SHADER_STAGE_NUM] = { 0, };
   uint32_t dynamic_offset_count = 0;

   for (uint32_t b = 0; b < pCreateInfo->count; b++) {
      uint32_t array_size = MAX2(1, pCreateInfo->pBinding[b].arraySize);
      set_layout->binding[b].array_size = array_size;
      set_layout->binding[b].descriptor_index = set_layout->size;
      set_layout->size += array_size;

      switch (pCreateInfo->pBinding[b].descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         for_each_bit(s, pCreateInfo->pBinding[b].stageFlags) {
            set_layout->binding[b].stage[s].sampler_index = sampler_count[s];
            sampler_count[s] += array_size;
         }
         break;
      default:
         break;
      }

      switch (pCreateInfo->pBinding[b].descriptorType) {
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         for_each_bit(s, pCreateInfo->pBinding[b].stageFlags) {
            set_layout->binding[b].stage[s].surface_index = surface_count[s];
            surface_count[s] += array_size;
         }
         break;
      default:
         break;
      }

      switch (pCreateInfo->pBinding[b].descriptorType) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         set_layout->binding[b].dynamic_offset_index = dynamic_offset_count;
         dynamic_offset_count += array_size;
         break;
      default:
         break;
      }

      if (pCreateInfo->pBinding[b].pImmutableSamplers) {
         set_layout->binding[b].immutable_samplers = samplers;
         samplers += array_size;

         for (uint32_t i = 0; i < array_size; i++)
            set_layout->binding[b].immutable_samplers[i] =
               anv_sampler_from_handle(pCreateInfo->pBinding[b].pImmutableSamplers[i]);
      } else {
         set_layout->binding[b].immutable_samplers = NULL;
      }

      set_layout->shader_stages |= pCreateInfo->pBinding[b].stageFlags;
   }

   set_layout->dynamic_offset_count = dynamic_offset_count;

   *pSetLayout = anv_descriptor_set_layout_to_handle(set_layout);

   return VK_SUCCESS;
}

void anv_DestroyDescriptorSetLayout(
    VkDevice                                    _device,
    VkDescriptorSetLayout                       _set_layout)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_descriptor_set_layout, set_layout, _set_layout);

   anv_device_free(device, set_layout);
}

/*
 * Pipeline layouts.  These have nothing to do with the pipeline.  They are
 * just muttiple descriptor set layouts pasted together
 */

VkResult anv_CreatePipelineLayout(
    VkDevice                                    _device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    VkPipelineLayout*                           pPipelineLayout)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_pipeline_layout l, *layout;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

   l.num_sets = pCreateInfo->descriptorSetCount;

   unsigned dynamic_offset_count = 0;

   memset(l.stage, 0, sizeof(l.stage));
   for (uint32_t set = 0; set < pCreateInfo->descriptorSetCount; set++) {
      ANV_FROM_HANDLE(anv_descriptor_set_layout, set_layout,
                      pCreateInfo->pSetLayouts[set]);
      l.set[set].layout = set_layout;

      l.set[set].dynamic_offset_start = dynamic_offset_count;
      for (uint32_t b = 0; b < set_layout->binding_count; b++) {
         if (set_layout->binding[b].dynamic_offset_index >= 0)
            dynamic_offset_count += set_layout->binding[b].array_size;
      }

      for (VkShaderStage s = 0; s < VK_SHADER_STAGE_NUM; s++) {
         l.set[set].stage[s].surface_start = l.stage[s].surface_count;
         l.set[set].stage[s].sampler_start = l.stage[s].sampler_count;

         for (uint32_t b = 0; b < set_layout->binding_count; b++) {
            unsigned array_size = set_layout->binding[b].array_size;

            if (set_layout->binding[b].stage[s].surface_index >= 0) {
               l.stage[s].surface_count += array_size;

               if (set_layout->binding[b].dynamic_offset_index >= 0)
                  l.stage[s].has_dynamic_offsets = true;
            }

            if (set_layout->binding[b].stage[s].sampler_index >= 0)
               l.stage[s].sampler_count += array_size;
         }
      }
   }

   unsigned num_bindings = 0;
   for (VkShaderStage s = 0; s < VK_SHADER_STAGE_NUM; s++)
      num_bindings += l.stage[s].surface_count + l.stage[s].sampler_count;

   size_t size = sizeof(*layout) + num_bindings * sizeof(layout->entries[0]);

   layout = anv_device_alloc(device, size, 8, VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (layout == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* Now we can actually build our surface and sampler maps */
   struct anv_pipeline_binding *entry = layout->entries;
   for (VkShaderStage s = 0; s < VK_SHADER_STAGE_NUM; s++) {
      l.stage[s].surface_to_descriptor = entry;
      entry += l.stage[s].surface_count;
      l.stage[s].sampler_to_descriptor = entry;
      entry += l.stage[s].sampler_count;

      int surface = 0;
      int sampler = 0;
      for (uint32_t set = 0; set < pCreateInfo->descriptorSetCount; set++) {
         struct anv_descriptor_set_layout *set_layout = l.set[set].layout;

         for (uint32_t b = 0; b < set_layout->binding_count; b++) {
            unsigned array_size = set_layout->binding[b].array_size;
            unsigned set_offset = set_layout->binding[b].descriptor_index;

            if (set_layout->binding[b].stage[s].surface_index >= 0) {
               assert(surface == l.set[set].stage[s].surface_start +
                                 set_layout->binding[b].stage[s].surface_index);
               for (unsigned i = 0; i < array_size; i++) {
                  l.stage[s].surface_to_descriptor[surface + i].set = set;
                  l.stage[s].surface_to_descriptor[surface + i].offset = set_offset + i;
               }
               surface += array_size;
            }

            if (set_layout->binding[b].stage[s].sampler_index >= 0) {
               assert(sampler == l.set[set].stage[s].sampler_start +
                                 set_layout->binding[b].stage[s].sampler_index);
               for (unsigned i = 0; i < array_size; i++) {
                  l.stage[s].sampler_to_descriptor[sampler + i].set = set;
                  l.stage[s].sampler_to_descriptor[sampler + i].offset = set_offset + i;
               }
               sampler += array_size;
            }
         }
      }
   }

   /* Finally, we're done setting it up, copy into the allocated version */
   *layout = l;

   *pPipelineLayout = anv_pipeline_layout_to_handle(layout);

   return VK_SUCCESS;
}

void anv_DestroyPipelineLayout(
    VkDevice                                    _device,
    VkPipelineLayout                            _pipelineLayout)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_pipeline_layout, pipeline_layout, _pipelineLayout);

   anv_device_free(device, pipeline_layout);
}

/*
 * Descriptor pools.  These are a no-op for now.
 */

VkResult anv_CreateDescriptorPool(
    VkDevice                                    device,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    VkDescriptorPool*                           pDescriptorPool)
{
   anv_finishme("VkDescriptorPool is a stub");
   *pDescriptorPool = (VkDescriptorPool)1;
   return VK_SUCCESS;
}

void anv_DestroyDescriptorPool(
    VkDevice                                    _device,
    VkDescriptorPool                            _pool)
{
   anv_finishme("VkDescriptorPool is a stub: free the pool's descriptor sets");
}

VkResult anv_ResetDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool)
{
   anv_finishme("VkDescriptorPool is a stub: free the pool's descriptor sets");
   return VK_SUCCESS;
}

VkResult
anv_descriptor_set_create(struct anv_device *device,
                          const struct anv_descriptor_set_layout *layout,
                          struct anv_descriptor_set **out_set)
{
   struct anv_descriptor_set *set;
   size_t size = sizeof(*set) + layout->size * sizeof(set->descriptors[0]);

   set = anv_device_alloc(device, size, 8, VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (!set)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* A descriptor set may not be 100% filled. Clear the set so we can can
    * later detect holes in it.
    */
   memset(set, 0, size);

   set->layout = layout;

   /* Go through and fill out immutable samplers if we have any */
   struct anv_descriptor *desc = set->descriptors;
   for (uint32_t b = 0; b < layout->binding_count; b++) {
      if (layout->binding[b].immutable_samplers) {
         for (uint32_t i = 0; i < layout->binding[b].array_size; i++)
            desc[i].sampler = layout->binding[b].immutable_samplers[i];
      }
      desc += layout->binding[b].array_size;
   }

   *out_set = set;

   return VK_SUCCESS;
}

void
anv_descriptor_set_destroy(struct anv_device *device,
                           struct anv_descriptor_set *set)
{
   anv_device_free(device, set);
}

VkResult anv_AllocDescriptorSets(
    VkDevice                                    _device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorSetUsage                        setUsage,
    uint32_t                                    count,
    const VkDescriptorSetLayout*                pSetLayouts,
    VkDescriptorSet*                            pDescriptorSets)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   VkResult result = VK_SUCCESS;
   struct anv_descriptor_set *set;
   uint32_t i;

   for (i = 0; i < count; i++) {
      ANV_FROM_HANDLE(anv_descriptor_set_layout, layout, pSetLayouts[i]);

      result = anv_descriptor_set_create(device, layout, &set);
      if (result != VK_SUCCESS)
         break;

      pDescriptorSets[i] = anv_descriptor_set_to_handle(set);
   }

   if (result != VK_SUCCESS)
      anv_FreeDescriptorSets(_device, descriptorPool, i, pDescriptorSets);

   return result;
}

VkResult anv_FreeDescriptorSets(
    VkDevice                                    _device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    count,
    const VkDescriptorSet*                      pDescriptorSets)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   for (uint32_t i = 0; i < count; i++) {
      ANV_FROM_HANDLE(anv_descriptor_set, set, pDescriptorSets[i]);

      anv_descriptor_set_destroy(device, set);
   }

   return VK_SUCCESS;
}

void anv_UpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    writeCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    copyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies)
{
   for (uint32_t i = 0; i < writeCount; i++) {
      const VkWriteDescriptorSet *write = &pDescriptorWrites[i];
      ANV_FROM_HANDLE(anv_descriptor_set, set, write->destSet);
      const struct anv_descriptor_set_binding_layout *bind_layout =
         &set->layout->binding[write->destBinding];
      struct anv_descriptor *desc =
         &set->descriptors[bind_layout->descriptor_index];

      switch (write->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         for (uint32_t j = 0; j < write->count; j++) {
            ANV_FROM_HANDLE(anv_sampler, sampler,
                            write->pDescriptors[j].sampler);

            desc[j] = (struct anv_descriptor) {
               .type = VK_DESCRIPTOR_TYPE_SAMPLER,
               .sampler = sampler,
            };
         }
         break;

      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         for (uint32_t j = 0; j < write->count; j++) {
            ANV_FROM_HANDLE(anv_image_view, iview,
                            write->pDescriptors[j].imageView);
            ANV_FROM_HANDLE(anv_sampler, sampler,
                            write->pDescriptors[j].sampler);

            desc[j].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            desc[j].image_view = iview;

            /* If this descriptor has an immutable sampler, we don't want
             * to stomp on it.
             */
            if (sampler)
               desc[j].sampler = sampler;
         }
         break;

      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         for (uint32_t j = 0; j < write->count; j++) {
            ANV_FROM_HANDLE(anv_image_view, iview,
                            write->pDescriptors[j].imageView);

            desc[j] = (struct anv_descriptor) {
               .type = write->descriptorType,
               .image_view = iview,
            };
         }
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         anv_finishme("texel buffers not implemented");
         break;

      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         anv_finishme("input attachments not implemented");
         break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         for (uint32_t j = 0; j < write->count; j++) {
            assert(write->pDescriptors[j].bufferInfo.buffer);
            ANV_FROM_HANDLE(anv_buffer, buffer,
                            write->pDescriptors[j].bufferInfo.buffer);
            assert(buffer);

            desc[j] = (struct anv_descriptor) {
               .type = write->descriptorType,
               .buffer = buffer,
               .offset = write->pDescriptors[j].bufferInfo.offset,
               .range = write->pDescriptors[j].bufferInfo.range,
            };

            /* For buffers with dynamic offsets, we use the full possible
             * range in the surface state and do the actual range-checking
             * in the shader.
             */
            if (bind_layout->dynamic_offset_index >= 0)
               desc[j].range = buffer->size - desc[j].offset;
         }

      default:
         break;
      }
   }

   for (uint32_t i = 0; i < copyCount; i++) {
      const VkCopyDescriptorSet *copy = &pDescriptorCopies[i];
      ANV_FROM_HANDLE(anv_descriptor_set, src, copy->destSet);
      ANV_FROM_HANDLE(anv_descriptor_set, dest, copy->destSet);
      for (uint32_t j = 0; j < copy->count; j++) {
         dest->descriptors[copy->destBinding + j] =
            src->descriptors[copy->srcBinding + j];
      }
   }
}