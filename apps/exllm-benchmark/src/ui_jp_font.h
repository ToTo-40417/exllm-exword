#ifndef XLLMI_UI_JP_FONT_H
#define XLLMI_UI_JP_FONT_H

void render_text_jp(unsigned short x, unsigned short y, const char *text);
void render_text_jp_wrapped(unsigned short x, unsigned short y, unsigned short width, const char *text);
void render_text_jp_wrapped_clipped(unsigned short x, unsigned short y,
                                    unsigned short width, unsigned short bottom,
                                    const char *text);

#endif
