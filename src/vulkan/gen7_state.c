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

void
gen7_fill_buffer_surface_state(void *state, const struct anv_format *format,
                               uint32_t offset, uint32_t range)
{
   /* This assumes RGBA float format. */

   uint32_t stride = 16; /* Depends on whether accessing shader is simd8 or
                          * vec4.  Will need one of each for buffers that are
                          * used in both vec4 and simd8. */

   uint32_t num_elements = range / stride;

   struct GEN7_RENDER_SURFACE_STATE surface_state = {
      .SurfaceType                              = SURFTYPE_BUFFER,
      .SurfaceFormat                            = format->surface_format,
      .SurfaceVerticalAlignment                 = VALIGN_4,
      .SurfaceHorizontalAlignment               = HALIGN_4,
      .TiledSurface                             = false,
      .RenderCacheReadWriteMode                 = false,
      .SurfaceObjectControlState                = GEN7_MOCS,
      .Height                                   = (num_elements >> 7) & 0x3fff,
      .Width                                    = num_elements & 0x7f,
      .Depth                                    = (num_elements >> 21) & 0x3f,
      .SurfacePitch                             = stride - 1,
      .SurfaceBaseAddress                       = { NULL, offset },
   };

   GEN7_RENDER_SURFACE_STATE_pack(NULL, state, &surface_state);
}

VkResult gen7_CreateBufferView(
    VkDevice                                    _device,
    const VkBufferViewCreateInfo*               pCreateInfo,
    VkBufferView*                               pView)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_buffer_view *view;
   VkResult result;

   result = anv_buffer_view_create(device, pCreateInfo, &view);
   if (result != VK_SUCCESS)
      return result;

   const struct anv_format *format =
      anv_format_for_vk_format(pCreateInfo->format);

   gen7_fill_buffer_surface_state(view->view.surface_state.map, format,
                                  view->view.offset, pCreateInfo->range);

   *pView = anv_buffer_view_to_handle(view);

   return VK_SUCCESS;
}

static const uint32_t vk_to_gen_tex_filter[] = {
   [VK_TEX_FILTER_NEAREST]                      = MAPFILTER_NEAREST,
   [VK_TEX_FILTER_LINEAR]                       = MAPFILTER_LINEAR
};

static const uint32_t vk_to_gen_mipmap_mode[] = {
   [VK_TEX_MIPMAP_MODE_BASE]                    = MIPFILTER_NONE,
   [VK_TEX_MIPMAP_MODE_NEAREST]                 = MIPFILTER_NEAREST,
   [VK_TEX_MIPMAP_MODE_LINEAR]                  = MIPFILTER_LINEAR
};

static const uint32_t vk_to_gen_tex_address[] = {
   [VK_TEX_ADDRESS_WRAP]                        = TCM_WRAP,
   [VK_TEX_ADDRESS_MIRROR]                      = TCM_MIRROR,
   [VK_TEX_ADDRESS_CLAMP]                       = TCM_CLAMP,
   [VK_TEX_ADDRESS_MIRROR_ONCE]                 = TCM_MIRROR_ONCE,
   [VK_TEX_ADDRESS_CLAMP_BORDER]                = TCM_CLAMP_BORDER,
};

static const uint32_t vk_to_gen_compare_op[] = {
   [VK_COMPARE_OP_NEVER]                        = PREFILTEROPNEVER,
   [VK_COMPARE_OP_LESS]                         = PREFILTEROPLESS,
   [VK_COMPARE_OP_EQUAL]                        = PREFILTEROPEQUAL,
   [VK_COMPARE_OP_LESS_EQUAL]                   = PREFILTEROPLEQUAL,
   [VK_COMPARE_OP_GREATER]                      = PREFILTEROPGREATER,
   [VK_COMPARE_OP_NOT_EQUAL]                    = PREFILTEROPNOTEQUAL,
   [VK_COMPARE_OP_GREATER_EQUAL]                = PREFILTEROPGEQUAL,
   [VK_COMPARE_OP_ALWAYS]                       = PREFILTEROPALWAYS,
};

VkResult gen7_CreateSampler(
    VkDevice                                    _device,
    const VkSamplerCreateInfo*                  pCreateInfo,
    VkSampler*                                  pSampler)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_sampler *sampler;
   uint32_t mag_filter, min_filter, max_anisotropy;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

   sampler = anv_device_alloc(device, sizeof(*sampler), 8,
                              VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (!sampler)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pCreateInfo->maxAnisotropy > 1) {
      mag_filter = MAPFILTER_ANISOTROPIC;
      min_filter = MAPFILTER_ANISOTROPIC;
      max_anisotropy = (pCreateInfo->maxAnisotropy - 2) / 2;
   } else {
      mag_filter = vk_to_gen_tex_filter[pCreateInfo->magFilter];
      min_filter = vk_to_gen_tex_filter[pCreateInfo->minFilter];
      max_anisotropy = RATIO21;
   }

   struct GEN7_SAMPLER_STATE sampler_state = {
      .SamplerDisable = false,
      .TextureBorderColorMode = DX10OGL,
      .BaseMipLevel = 0.0,
      .MipModeFilter = vk_to_gen_mipmap_mode[pCreateInfo->mipMode],
      .MagModeFilter = mag_filter,
      .MinModeFilter = min_filter,
      .TextureLODBias = pCreateInfo->mipLodBias * 256,
      .AnisotropicAlgorithm = EWAApproximation,
      .MinLOD = pCreateInfo->minLod,
      .MaxLOD = pCreateInfo->maxLod,
      .ChromaKeyEnable = 0,
      .ChromaKeyIndex = 0,
      .ChromaKeyMode = 0,
      .ShadowFunction = vk_to_gen_compare_op[pCreateInfo->compareOp],
      .CubeSurfaceControlMode = 0,

      .BorderColorPointer =
         device->border_colors.offset +
         pCreateInfo->borderColor * sizeof(float) * 4,

      .MaximumAnisotropy = max_anisotropy,
      .RAddressMinFilterRoundingEnable = 0,
      .RAddressMagFilterRoundingEnable = 0,
      .VAddressMinFilterRoundingEnable = 0,
      .VAddressMagFilterRoundingEnable = 0,
      .UAddressMinFilterRoundingEnable = 0,
      .UAddressMagFilterRoundingEnable = 0,
      .TrilinearFilterQuality = 0,
      .NonnormalizedCoordinateEnable = 0,
      .TCXAddressControlMode = vk_to_gen_tex_address[pCreateInfo->addressU],
      .TCYAddressControlMode = vk_to_gen_tex_address[pCreateInfo->addressV],
      .TCZAddressControlMode = vk_to_gen_tex_address[pCreateInfo->addressW],
   };

   GEN7_SAMPLER_STATE_pack(NULL, sampler->state, &sampler_state);

   *pSampler = anv_sampler_to_handle(sampler);

   return VK_SUCCESS;
}
VkResult gen7_CreateDynamicRasterState(
    VkDevice                                    _device,
    const VkDynamicRasterStateCreateInfo*       pCreateInfo,
    VkDynamicRasterState*                       pState)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_dynamic_rs_state *state;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO);

   state = anv_device_alloc(device, sizeof(*state), 8,
                            VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (state == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   bool enable_bias = pCreateInfo->depthBias != 0.0f ||
      pCreateInfo->slopeScaledDepthBias != 0.0f;

   struct GEN7_3DSTATE_SF sf = {
      GEN7_3DSTATE_SF_header,
      .LineWidth                                = pCreateInfo->lineWidth,
      .GlobalDepthOffsetEnableSolid             = enable_bias,
      .GlobalDepthOffsetEnableWireframe         = enable_bias,
      .GlobalDepthOffsetEnablePoint             = enable_bias,
      .GlobalDepthOffsetConstant                = pCreateInfo->depthBias,
      .GlobalDepthOffsetScale                   = pCreateInfo->slopeScaledDepthBias,
      .GlobalDepthOffsetClamp                   = pCreateInfo->depthBiasClamp
   };

   GEN7_3DSTATE_SF_pack(NULL, state->gen7.sf, &sf);

   *pState = anv_dynamic_rs_state_to_handle(state);

   return VK_SUCCESS;
}

VkResult gen7_CreateDynamicDepthStencilState(
    VkDevice                                    _device,
    const VkDynamicDepthStencilStateCreateInfo* pCreateInfo,
    VkDynamicDepthStencilState*                 pState)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_dynamic_ds_state *state;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DYNAMIC_DEPTH_STENCIL_STATE_CREATE_INFO);

   state = anv_device_alloc(device, sizeof(*state), 8,
                            VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
   if (state == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   struct GEN7_DEPTH_STENCIL_STATE depth_stencil_state = {
      .StencilTestMask = pCreateInfo->stencilReadMask & 0xff,
      .StencilWriteMask = pCreateInfo->stencilWriteMask & 0xff,
      .BackfaceStencilTestMask = pCreateInfo->stencilReadMask & 0xff,
      .BackfaceStencilWriteMask = pCreateInfo->stencilWriteMask & 0xff,
   };

   GEN7_DEPTH_STENCIL_STATE_pack(NULL, state->gen7.depth_stencil_state,
                                 &depth_stencil_state);

   struct GEN7_COLOR_CALC_STATE color_calc_state = {
      .StencilReferenceValue = pCreateInfo->stencilFrontRef,
      .BackFaceStencilReferenceValue = pCreateInfo->stencilBackRef
   };

   GEN7_COLOR_CALC_STATE_pack(NULL, state->gen7.color_calc_state, &color_calc_state);

   *pState = anv_dynamic_ds_state_to_handle(state);

   return VK_SUCCESS;
}

static const uint8_t anv_halign[] = {
    [4] = HALIGN_4,
    [8] = HALIGN_8,
};

static const uint8_t anv_valign[] = {
    [2] = VALIGN_2,
    [4] = VALIGN_4,
};

void
gen7_image_view_init(struct anv_image_view *iview,
                     struct anv_device *device,
                     const VkImageViewCreateInfo* pCreateInfo,
                     struct anv_cmd_buffer *cmd_buffer)
{
   ANV_FROM_HANDLE(anv_image, image, pCreateInfo->image);

   const VkImageSubresourceRange *range = &pCreateInfo->subresourceRange;
   struct anv_surface_view *view = &iview->view;
   struct anv_surface *surface =
      anv_image_get_surface_for_aspect(image, range->aspect);

   const struct anv_format *format =
      anv_format_for_vk_format(pCreateInfo->format);

   const struct anv_image_view_info *view_type_info =
      anv_image_view_info_for_vk_image_view_type(pCreateInfo->viewType);

   if (pCreateInfo->viewType != VK_IMAGE_VIEW_TYPE_2D)
      anv_finishme("non-2D image views");

   view->bo = image->bo;
   view->offset = image->offset + surface->offset;
   view->format = anv_format_for_vk_format(pCreateInfo->format);

   iview->extent = (VkExtent3D) {
      .width = anv_minify(image->extent.width, range->baseMipLevel),
      .height = anv_minify(image->extent.height, range->baseMipLevel),
      .depth = anv_minify(image->extent.depth, range->baseMipLevel),
   };

   uint32_t depth = 1;
   if (range->arraySize > 1) {
      depth = range->arraySize;
   } else if (image->extent.depth > 1) {
      depth = image->extent.depth;
   }

   struct GEN7_RENDER_SURFACE_STATE surface_state = {
      .SurfaceType = view_type_info->surface_type,
      .SurfaceArray = image->array_size > 1,
      .SurfaceFormat = format->surface_format,
      .SurfaceVerticalAlignment = anv_valign[surface->v_align],
      .SurfaceHorizontalAlignment = anv_halign[surface->h_align],

      /* From bspec (DevSNB, DevIVB): "Set Tile Walk to TILEWALK_XMAJOR if
       * Tiled Surface is False."
       */
      .TiledSurface = surface->tile_mode > LINEAR,
      .TileWalk = surface->tile_mode == YMAJOR ? TILEWALK_YMAJOR : TILEWALK_XMAJOR,

      .VerticalLineStride = 0,
      .VerticalLineStrideOffset = 0,
      .RenderCacheReadWriteMode = false,

      .Height = image->extent.height - 1,
      .Width = image->extent.width - 1,
      .Depth = depth - 1,
      .SurfacePitch = surface->stride - 1,
      .MinimumArrayElement = range->baseArraySlice,
      .NumberofMultisamples = MULTISAMPLECOUNT_1,
      .XOffset = 0,
      .YOffset = 0,

      .SurfaceObjectControlState = GEN7_MOCS,

      /* For render target surfaces, the hardware interprets field MIPCount/LOD as
       * LOD. The Broadwell PRM says:
       *
       *    MIPCountLOD defines the LOD that will be rendered into.
       *    SurfaceMinLOD is ignored.
       */
      .MIPCountLOD = range->mipLevels - 1,
      .SurfaceMinLOD = range->baseMipLevel,

      .MCSEnable = false,
      .RedClearColor = 0,
      .GreenClearColor = 0,
      .BlueClearColor = 0,
      .AlphaClearColor = 0,
      .ResourceMinLOD = 0.0,
      .SurfaceBaseAddress = { NULL, view->offset },
   };

   if (cmd_buffer) {
      view->surface_state =
         anv_state_stream_alloc(&cmd_buffer->surface_state_stream, 64, 64);
   } else {
      view->surface_state =
         anv_state_pool_alloc(&device->surface_state_pool, 64, 64);
   }

   GEN7_RENDER_SURFACE_STATE_pack(NULL, view->surface_state.map, &surface_state);
}

void
gen7_color_attachment_view_init(struct anv_color_attachment_view *aview,
                                struct anv_device *device,
                                const VkAttachmentViewCreateInfo* pCreateInfo,
                                struct anv_cmd_buffer *cmd_buffer)
{
   ANV_FROM_HANDLE(anv_image, image, pCreateInfo->image);
   struct anv_surface_view *view = &aview->view;
   struct anv_surface *surface =
      anv_image_get_surface_for_color_attachment(image);

   aview->base.attachment_type = ANV_ATTACHMENT_VIEW_TYPE_COLOR;

   anv_assert(pCreateInfo->arraySize > 0);
   anv_assert(pCreateInfo->mipLevel < image->levels);
   anv_assert(pCreateInfo->baseArraySlice + pCreateInfo->arraySize <= image->array_size);

   view->bo = image->bo;
   view->offset = image->offset + surface->offset;
   view->format = anv_format_for_vk_format(pCreateInfo->format);

   aview->base.extent = (VkExtent3D) {
      .width = anv_minify(image->extent.width, pCreateInfo->mipLevel),
      .height = anv_minify(image->extent.height, pCreateInfo->mipLevel),
      .depth = anv_minify(image->extent.depth, pCreateInfo->mipLevel),
   };

   uint32_t depth = 1;
   if (pCreateInfo->arraySize > 1) {
      depth = pCreateInfo->arraySize;
   } else if (image->extent.depth > 1) {
      depth = image->extent.depth;
   }

   if (cmd_buffer) {
      view->surface_state =
         anv_state_stream_alloc(&cmd_buffer->surface_state_stream, 64, 64);
   } else {
      view->surface_state =
         anv_state_pool_alloc(&device->surface_state_pool, 64, 64);
   }

   struct GEN7_RENDER_SURFACE_STATE surface_state = {
      .SurfaceType = SURFTYPE_2D,
      .SurfaceArray = image->array_size > 1,
      .SurfaceFormat = view->format->surface_format,
      .SurfaceVerticalAlignment = anv_valign[surface->v_align],
      .SurfaceHorizontalAlignment = anv_halign[surface->h_align],

      /* From bspec (DevSNB, DevIVB): "Set Tile Walk to TILEWALK_XMAJOR if
       * Tiled Surface is False."
       */
      .TiledSurface = surface->tile_mode > LINEAR,
      .TileWalk = surface->tile_mode == YMAJOR ? TILEWALK_YMAJOR : TILEWALK_XMAJOR,

      .VerticalLineStride = 0,
      .VerticalLineStrideOffset = 0,
      .RenderCacheReadWriteMode = WriteOnlyCache,

      .Height = image->extent.height - 1,
      .Width = image->extent.width - 1,
      .Depth = depth - 1,
      .SurfacePitch = surface->stride - 1,
      .MinimumArrayElement = pCreateInfo->baseArraySlice,
      .NumberofMultisamples = MULTISAMPLECOUNT_1,
      .XOffset = 0,
      .YOffset = 0,

      .SurfaceObjectControlState = GEN7_MOCS,

      /* For render target surfaces, the hardware interprets field MIPCount/LOD as
       * LOD. The Broadwell PRM says:
       *
       *    MIPCountLOD defines the LOD that will be rendered into.
       *    SurfaceMinLOD is ignored.
       */
      .SurfaceMinLOD = 0,
      .MIPCountLOD = pCreateInfo->mipLevel,

      .MCSEnable = false,
      .RedClearColor = 0,
      .GreenClearColor = 0,
      .BlueClearColor = 0,
      .AlphaClearColor = 0,
      .ResourceMinLOD = 0.0,
      .SurfaceBaseAddress = { NULL, view->offset },

   };

   GEN7_RENDER_SURFACE_STATE_pack(NULL, view->surface_state.map, &surface_state);
}