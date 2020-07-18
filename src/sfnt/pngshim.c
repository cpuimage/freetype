/****************************************************************************
 *
 * pngshim.c
 *
 *   PNG Bitmap glyph support.
 *
 * Copyright (C) 2013-2020 by
 * Google, Inc.
 * Written by Stuart Gill and Behdad Esfahbod.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#include <ft2build.h>
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_TRUETYPE_TAGS_H
#include FT_CONFIG_STANDARD_LIBRARY_H


#if defined( TT_CONFIG_OPTION_EMBEDDED_BITMAPS ) && \
    defined( FT_CONFIG_OPTION_USE_PNG )

#include "pngshim.h"
#include "lodepng.h"
#include "lodepng.c"
#include "sferrors.h"


/* This code is freely based on cairo-png.c.  There's so many ways */
/* to call libpng, and the way cairo does it is defacto standard.  */

static unsigned int
multiply_alpha( unsigned int  alpha,
                unsigned int  color )
{
    unsigned int  temp = alpha * color + 0x80;


    return ( temp + ( temp >> 8 ) ) >> 8;
}


typedef union {
    unsigned int color;
    struct {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };
} FT_Vec4;

FT_LOCAL_DEF(FT_Error)
Load_SBit_Png(FT_GlyphSlot slot,
              FT_Int x_offset,
              FT_Int y_offset,
              FT_Int pix_bits,
              TT_SBit_Metrics metrics,
              FT_Memory memory,
              FT_Byte *data,
              FT_UInt png_len,
              FT_Bool populate_map_and_metrics,
              FT_Bool metrics_only) {
    FT_Bitmap *map = &slot->bitmap;
    FT_Error error = FT_Err_Ok;
    if (x_offset < 0 ||
        y_offset < 0) {
        error = FT_THROW(Invalid_Argument);
        goto Exit;
    }
    if (!populate_map_and_metrics &&
        ((FT_UInt) x_offset + metrics->width > map->width ||
         (FT_UInt) y_offset + metrics->height > map->rows ||
         pix_bits != 32 ||
         map->pixel_mode != FT_PIXEL_MODE_BGRA)) {
        error = FT_THROW(Invalid_Argument);
        goto Exit;
    }
    FT_Byte *png_data = memory->alloc(memory, metrics->width * metrics->height * 4);
    if (png_data == 0) {
        error = FT_THROW(Out_Of_Memory);
        goto Exit;
    }
    unsigned imgWidth;
    unsigned imgHeight;
    unsigned err = lodepng_decode32(&png_data, &imgWidth, &imgHeight, data, png_len);
    if (err != 0) {
        error = FT_THROW(Unknown_File_Format);
        goto Exit;
    }
    if (!populate_map_and_metrics &&
        ((FT_Int) imgWidth != metrics->width ||
         (FT_Int) imgHeight != metrics->height))
        goto DestroyExit;

    if (populate_map_and_metrics) {
        metrics->width = (FT_UShort) imgWidth;
        metrics->height = (FT_UShort) imgHeight;

        map->width = metrics->width;
        map->rows = metrics->height;
        map->pixel_mode = FT_PIXEL_MODE_BGRA;
        map->pitch = (int) (map->width * 4);
        map->num_grays = 256;

        /* reject too large bitmaps similarly to the rasterizer */
        if (map->rows > 0x7FFF || map->width > 0x7FFF) {
            error = FT_THROW(Array_Too_Large);
            goto DestroyExit;
        }
    }
    if (metrics_only)
        goto DestroyExit;
    FT_Vec4 *png_data_vec4 = (FT_Vec4 *) png_data;
    FT_Int y;
    FT_Int x;
    for (y = 0; y < imgHeight; y++) {
        FT_Vec4 *pngline = png_data_vec4 + y * imgWidth;
        for (x = 0; x < imgWidth; x++) {
            unsigned int alpha = pngline[x].a;
            if (alpha == 0)
                pngline[x].color = 0;
            else {
                unsigned int red = pngline[x].r;
                unsigned int green = pngline[x].g;
                unsigned int blue = pngline[x].b;
                if (alpha != 0xFF) {
                    red = multiply_alpha(alpha, red);
                    green = multiply_alpha(alpha, green);
                    blue = multiply_alpha(alpha, blue);
                }
                pngline[x].color = (blue) | (green << 8) | (red << 16) | (alpha << 24);
            }
        }
    }
    if (populate_map_and_metrics) {
        /* this doesn't overflow: 0x7FFF * 0x7FFF * 4 < 2^32 */
        FT_ULong size = map->rows * (FT_ULong) map->pitch;
        error = ft_glyphslot_alloc_bitmap(slot, size);
        if (error)
            goto DestroyExit;
    }

    FT_Int i;
    FT_Int x_offset_pos = x_offset * 4;
    for (i = 0; i < (FT_Int) imgHeight; i++)
        memcpy(map->buffer + (y_offset + i) * map->pitch + x_offset_pos, png_data_vec4 + i * imgWidth,
               imgWidth * sizeof(unsigned int));

    DestroyExit:
    memory->free(memory, png_data);
    Exit:
    return error;
}

#else /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */

/* ANSI C doesn't like empty source files */
typedef int  _pngshim_dummy;

#endif /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */


/* END */
