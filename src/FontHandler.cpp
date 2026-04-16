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

/* Default font size in points — matches the original 3DO bitmap font height */
static const int DEFAULT_FONT_SIZE = 16;

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

    /* Render the text to an SDL_Surface (shaded = fg on solid bg) */
    SDL_Surface *surface = TTF_RenderText_Shaded(gFont, textContent, fgColor, bgColor);
    if (!surface)
    {
        printf("ERROR - TTF_RenderText_Shaded() failed: %s\n", TTF_GetError());
        return nullptr;
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