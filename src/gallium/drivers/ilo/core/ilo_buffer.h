/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2012-2013 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#ifndef ILO_BUFFER_H
#define ILO_BUFFER_H

#include "intel_winsys.h"

#include "ilo_core.h"
#include "ilo_debug.h"
#include "ilo_dev.h"

struct ilo_buffer {
   unsigned bo_size;

   /* managed by users */
   struct intel_bo *bo;
};

static inline void
ilo_buffer_init(struct ilo_buffer *buf, const struct ilo_dev *dev,
                unsigned size, uint32_t bind, uint32_t flags)
{
   assert(ilo_is_zeroed(buf, sizeof(*buf)));

   buf->bo_size = size;

   /*
    * From the Sandy Bridge PRM, volume 1 part 1, page 118:
    *
    *     "For buffers, which have no inherent "height," padding requirements
    *      are different. A buffer must be padded to the next multiple of 256
    *      array elements, with an additional 16 bytes added beyond that to
    *      account for the L1 cache line."
    */
   if (bind & PIPE_BIND_SAMPLER_VIEW)
      buf->bo_size = align(buf->bo_size, 256) + 16;
}

#endif /* ILO_BUFFER_H */