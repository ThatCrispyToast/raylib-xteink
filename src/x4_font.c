#include "raylib.h"
#include "x4_font_data.h"
#include <stdlib.h>

Font x4_load_default_font(void)
{
    Font font = { .baseSize=10, .glyphCount=224 };
    font.glyphs = calloc(224,sizeof(GlyphInfo));
    font.recs = calloc(224,sizeof(Rectangle));
    if (!font.glyphs || !font.recs) goto fail;
    extern unsigned int x4_load_font_texture(const void *pixels, int width, int height);
    font.texture = (Texture2D){ .id=x4_load_font_texture(x4_font_atlas,128,128),
        .width=128, .height=128, .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA };
    if (!IsTextureValid(font.texture)) goto fail;
    for (int i=0;i<224;i++) {
        const unsigned short *r=x4_font_rects[i];
        font.recs[i]=(Rectangle){r[0],r[1],r[2],r[3]};
        font.glyphs[i].value=32+i;
        font.glyphs[i].image=(Image){ .data=(void *)(x4_font_glyph_pixels+r[4]),
            .width=r[2], .height=r[3], .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA };
    }
    return font;
fail:
    free(font.glyphs); free(font.recs);
    TraceLog(LOG_ERROR,"X4: not enough memory for default font");
    return (Font){0};
}

void x4_unload_default_font(Font font)
{
    UnloadTexture(font.texture);
    free(font.glyphs); free(font.recs);
}
