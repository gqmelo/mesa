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
 *
 * Authors:
 *    Jason Ekstrand (jason@jlekstrand.net)
 *
 */

/*
 * A simple executable that opens a SPIR-V shader, converts it to NIR, and
 * dumps out the result.  This should be useful for testing the
 * spirv_to_nir code.
 */

#include "nir_spirv.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
   int fd = open(argv[1], O_RDONLY);
   off_t len = lseek(fd, 0, SEEK_END);

   assert(len % 4 == 0);
   size_t word_count = len / 4;

   const void *map = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
   assert(map != NULL);

   nir_shader *shader = spirv_to_nir(map, word_count, NULL);
   nir_print_shader(shader, stderr);
}