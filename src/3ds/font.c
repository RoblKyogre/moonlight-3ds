// Note: temporary font.c file until i decide to implement better font support
#include "font.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <3ds/console.h>

static uint8_t font_r = 255, font_g = 255, font_b = 255, font_a = 255;

static PrintConsole* bottomScreenConsole;

void Font_Init(void)
{
    consoleInit(GFX_BOTTOM, bottomScreenConsole);
    consoleSelect(bottomScreenConsole);

    // Clear the texture buffer
    Font_Clear();
}

void Font_Deinit(void)
{
    // no-op to deinit
}

void Font_Draw(void)
{
    // no-op to draw
}

void Font_Draw_Bottom(void)
{
    Font_Draw();
    gfxFlushBuffers();
	gfxSwapBuffers();
}

void Font_Clear(void)
{
    printf("\x1b[2J");
}

void Font_Printw(uint32_t x, uint32_t y, const wchar_t* string)
{
    printf(string);
}

void Font_Print(uint32_t x, uint32_t y, const char* string)
{
    wchar_t *buffer = calloc(strlen(string) + 1, sizeof(wchar_t));

    size_t num = mbstowcs(buffer, string, strlen(string));
    if (num > 0) {
        buffer[num] = 0;
    } 
    else {
        wchar_t* tmp = buffer;
        while ((*tmp++ = *string++));
    }

    Font_Printw(x, y, buffer);
    free(buffer);
}

void Font_Printf(uint32_t x, uint32_t y, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);

    char* tmp = NULL;
    if((vasprintf(&tmp, msg, args) >= 0) && tmp) {
        Font_Print(x, y, tmp);
    }

    va_end(args);
    free(tmp);
}

void Font_SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    font_r = r;
    font_g = g;
    font_b = b;
    font_a = a;
}

void Font_SetSize(uint32_t size)
{
    // no-op because size isn't supported yet
}
