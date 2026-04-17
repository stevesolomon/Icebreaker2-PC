/******************************************************************************/
/*                                                                            */
/*                          FontHandler.cpp                                   */
/*                                                                            */
/*    SDL_ttf-based text rendering (replaces 3DO TextLib implementation).     */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/*    (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.       */
/*                                                                            */
/******************************************************************************/

#include "platform/platform.h"
#include "FontHandler.h"

/* Default font size in points — tuned to approximate the original 3DO bitmap font
 * glyph size at 320×240 native resolution. */
static const int DEFAULT_FONT_SIZE = 10;

/* The original 3DO Helvetica.3do bitmap font had a 32-pixel character cell with
 * significant internal leading above the glyphs.  The game's text Y-coordinates
 * were calibrated for that extra top padding.  TTF fonts have very little leading
 * so, without compensation, text appears several pixels too high.  This constant
 * adds transparent padding at the top of every text surface to re-align with the
 * pre-rendered labels baked into the background artwork. */
static const int FONT_TOP_PADDING = 11;

/******************************************************************************/
/*                  C_FontHandler Class methods start below:                   */
/******************************************************************************/


/*********************  C_FontHandler Class - Constructor  ********************/

C_FontHandler::C_FontHandler()
{
    gFont = nullptr;
    numTextCels = 0;
    std::memset(gTextCels, 0, sizeof(gTextCels));
}


/*********************  C_FontHandler Class - Destructor  *********************/

C_FontHandler::~C_FontHandler()
{
    /* Destroy any textures we created for the text cels */
    for (unsigned i = 0; i < MAX_FONT_TEXT_CELS; i++)
    {
        if (gTextCels[i].texture)
        {
            SDL_DestroyTexture(gTextCels[i].texture);
            gTextCels[i].texture = nullptr;
        }
        if (gTextCels[i].surface)
        {
            SDL_FreeSurface(gTextCels[i].surface);
            gTextCels[i].surface = nullptr;
        }
    }

    /* Close the font */
    if (gFont)
    {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
}


/*****************  C_FontHandler Class - CreateCelFromFont()  ****************/
/*                                                                            */
/*  Loads a TTF font file.  The first call loads the font and allocates the   */
/*  first text cel slot.  Subsequent calls allocate additional slots (max 10).*/
/*                                                                            */
/******************************************************************************/

int C_FontHandler::CreateCelFromFont(char *fontFileName)
{
    (void)fontFileName; /* original 3DO path is unused; we load the TTF equivalent */

    if (numTextCels == 0)
    {
        /* Open the TTF font */
        gFont = TTF_OpenFont("assets/MetaArt/Helvetica.ttf", DEFAULT_FONT_SIZE);
        if (!gFont)
        {
            printf("ERROR - TTF_OpenFont() failed: %s\n", TTF_GetError());
            return -1;
        }

        /* The original 3DO Helvetica bitmap font was bold weight.
         * Our TTF is "55 Roman" (regular), so apply synthetic bold. */
        TTF_SetFontStyle(gFont, TTF_STYLE_BOLD);

        numTextCels++;
        return 1;
    }

    /* Font already loaded — just allocate another text cel slot */
    return CreateCelFromFont();
}


/*****************  C_FontHandler Class - CreateCelFromFont()  ****************/
/*                                                                            */
/*  Allocates an additional text cel slot (no-arg overload).                  */
/*                                                                            */
/******************************************************************************/

int C_FontHandler::CreateCelFromFont(void)
{
    if (!gFont)
    {
        printf("ERROR - CreateCelFromFont() failed! A font may not have been loaded yet.\n");
        return -1;
    }

    if (numTextCels >= MAX_FONT_TEXT_CELS)
    {
        printf("ERROR - CreateCelFromFont() failed! "
               "Only %u text cels concurrently are supported.\n", MAX_FONT_TEXT_CELS);
        return -1;
    }

    numTextCels++;
    return static_cast<int>(numTextCels);
}


/*****************  C_FontHandler Class - RenderTextToCel()  ******************/
/*                                                                            */
/*  Internal helper: renders a string into a text cel slot with the given     */
/*  foreground / background colours, returns a CCB* (Sprite*).               */
/*                                                                            */
/******************************************************************************/

CCB *C_FontHandler::RenderTextToCel(char *textContent, SDL_Color fgColor,
                                    SDL_Color bgColor, unsigned textCelID)
{
    if (!gFont)
    {
        printf("ERROR - No font loaded in RenderTextToCel().\n");
        return nullptr;
    }

    if (textCelID < 1 || textCelID > numTextCels)
    {
        printf("ERROR - Invalid Cel ID %u passed to RenderTextToCel().\n", textCelID);
        return nullptr;
    }

    unsigned idx = textCelID - 1;   /* 1-based ID -> 0-based array index */

    /* Free any previous texture / surface for this slot */
    if (gTextCels[idx].texture)
    {
        SDL_DestroyTexture(gTextCels[idx].texture);
        gTextCels[idx].texture = nullptr;
    }
    if (gTextCels[idx].surface)
    {
        SDL_FreeSurface(gTextCels[idx].surface);
        gTextCels[idx].surface = nullptr;
    }

    /* Render the text to an SDL_Surface with transparent background.
     * On the 3DO, background color 0 (MakeRGB15(0,0,0)) was transparent since
     * pixel value 0x0000 was the CEL transparency key. TTF_RenderText_Blended
     * gives us anti-aliased text on a fully transparent RGBA surface. */
    (void)bgColor; /* background is always transparent in blended mode */
    SDL_Surface *rawSurface = TTF_RenderText_Blended(gFont, textContent, fgColor);
    if (!rawSurface)
    {
        printf("ERROR - TTF_RenderText_Blended() failed: %s\n", TTF_GetError());
        return nullptr;
    }

    /* Add top-padding to match the original 3DO bitmap font's internal leading.
     * The game positions text by the top of the cel; the original font had extra
     * space above the glyphs that our TTF font lacks.  We create a taller surface
     * with the text placed FONT_TOP_PADDING pixels from the top. */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, rawSurface->w, rawSurface->h + FONT_TOP_PADDING,
        32, SDL_PIXELFORMAT_RGBA32);
    if (surface)
    {
        SDL_FillRect(surface, nullptr, 0); /* clear to fully transparent */
        SDL_SetSurfaceBlendMode(rawSurface, SDL_BLENDMODE_NONE);
        SDL_Rect dstRect = { 0, FONT_TOP_PADDING, rawSurface->w, rawSurface->h };
        SDL_BlitSurface(rawSurface, nullptr, surface, &dstRect);
        SDL_FreeSurface(rawSurface);
    }
    else
    {
        /* Fallback: use the raw surface if padding allocation fails */
        surface = rawSurface;
    }

    /* Create a GPU texture from the surface */
    SDL_Renderer *renderer = GetRenderer();
    SDL_Texture *texture = nullptr;
    if (renderer)
    {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
    }

    if (!texture)
    {
        printf("ERROR - SDL_CreateTextureFromSurface() failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return nullptr;
    }

    /* Enable alpha blending so transparent background pixels show through */
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    /* Populate the CCB (Sprite) for this slot */
    gTextCels[idx].texture   = texture;
    gTextCels[idx].surface   = surface;
    gTextCels[idx].ccb_Width  = surface->w;
    gTextCels[idx].ccb_Height = surface->h;

    return &gTextCels[idx];
}


/*******************  C_FontHandler Class - PlaceTextInCel()  *****************/
/*                                                                            */
/*  Renders coloured text into the specified cel slot and returns a CCB*.     */
/*  Colour values use the 3DO convention (0-65535); they are scaled to the    */
/*  SDL 0-255 range by dividing by 257.                                       */
/*                                                                            */
/******************************************************************************/

CCB *C_FontHandler::PlaceTextInCel(char *textContent, uint32 redBgColor,
                                   uint32 greenBgColor, uint32 blueBgColor,
                                   uint32 redFgColor, uint32 greenFgColor,
                                   uint32 blueFgColor, unsigned textCelID)
{
    /* Verify that a string larger than 80 characters has not been passed */
    if (std::strlen(textContent) > 80)
    {
        printf("ERROR - A string greater than 80 characters was passed!\n");
        return nullptr;
    }

    /* Scale 3DO 16-bit-per-component colours (0-65535) to SDL 8-bit (0-255) */
    SDL_Color fgColor;
    fgColor.r = static_cast<uint8>(redFgColor   / 257);
    fgColor.g = static_cast<uint8>(greenFgColor / 257);
    fgColor.b = static_cast<uint8>(blueFgColor  / 257);
    fgColor.a = 255;

    SDL_Color bgColor;
    bgColor.r = static_cast<uint8>(redBgColor   / 257);
    bgColor.g = static_cast<uint8>(greenBgColor / 257);
    bgColor.b = static_cast<uint8>(blueBgColor  / 257);
    bgColor.a = 255;

    return RenderTextToCel(textContent, fgColor, bgColor, textCelID);
}


/*******************  C_FontHandler Class - PlaceTextInCel()  *****************/
/*                                                                            */
/*  Overload without explicit colours — uses white text on a black background.*/
/*                                                                            */
/******************************************************************************/

CCB *C_FontHandler::PlaceTextInCel(char *textContent, unsigned textCelID)
{
    /* Verify that a string larger than 80 characters has not been passed */
    if (std::strlen(textContent) > 80)
    {
        printf("ERROR - A string greater than 80 characters was passed!\n");
        return nullptr;
    }

    SDL_Color fgColor = { 255, 255, 255, 255 }; /* white */
    SDL_Color bgColor = {   0,   0,   0, 255 }; /* black */

    return RenderTextToCel(textContent, fgColor, bgColor, textCelID);
}


/***********************************  EOF  ************************************/