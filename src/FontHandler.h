#ifndef __FONTHANDLER_HP
#define __FONTHANDLER_HP

/******************************************************************************/
/*                                                                            */
/*                          FontHandler.h                                     */
/*                                                                            */
/*    This header file contains the function prototypes for the Font library  */
/*    routines defined in the FontHandler.cpp file.                           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/*    (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.       */
/*                                                                            */
/******************************************************************************/

#include "platform/platform.h"

static const unsigned MAX_FONT_TEXT_CELS = 10;

/****************************  C_FontHandler Class  ***************************/

class C_FontHandler
{
    unsigned numTextCels;
    TTF_Font *gFont;                            /* SDL_ttf font handle        */
    CCB gTextCels[MAX_FONT_TEXT_CELS];          /* Sprite for each text slot  */

    /* Cache of alternate-size fonts (lazily opened by PlaceTextInCelSized). */
    static const unsigned MAX_ALT_FONTS = 4;
    TTF_Font *gAltFonts[MAX_ALT_FONTS];
    int       gAltSizes[MAX_ALT_FONTS];

    TTF_Font *GetFontAtSize(int ptSize);

    /* Private methods below */
    CCB *RenderTextToCel(char *textContent, SDL_Color fgColor,
                         SDL_Color bgColor, unsigned textCelID,
                         TTF_Font *font);

public:

    C_FontHandler();                /* The class constructor */
    ~C_FontHandler();               /* The class destructor  */

    int CreateCelFromFont(char *fontFileName);

    int CreateCelFromFont(void);

    CCB* PlaceTextInCel(char *textContent, uint32 redBgColor,
                        uint32 greenBgColor, uint32 blueBgColor,
                        uint32 redFgColor, uint32 greenFgColor,
                        uint32 blueFgColor, unsigned textCelID);

    CCB* PlaceTextInCel(char *textContent, unsigned textCelID);

    /* Render with a specific point size. ptSize is in TTF points; pass the
     * default value (10) to get the same result as PlaceTextInCel. */
    CCB* PlaceTextInCelSized(char *textContent, uint32 redBgColor,
                             uint32 greenBgColor, uint32 blueBgColor,
                             uint32 redFgColor, uint32 greenFgColor,
                             uint32 blueFgColor, unsigned textCelID,
                             int ptSize);

};  /* End of Class C_FontHandler */

#endif  /* __FONTHANDLER_HP */

/***********************************  EOF  ************************************/