/*******************************************************************************
 *  platform/input.h — SDL2 input layer (replaces 3DO ControlPad)
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include "types.h"
#include <SDL.h>

/* ── 3DO Controller button flags (preserved for source compatibility) ────── */
#define ControlUp           0x00000001
#define ControlDown         0x00000002
#define ControlLeft         0x00000004
#define ControlRight        0x00000008
#define ControlA            0x00000010
#define ControlB            0x00000020
#define ControlC            0x00000040
#define ControlX            0x00000080
#define ControlStart        0x00000100
#define ControlLeftShift    0x00000200
#define ControlRightShift   0x00000400

/* ── ControlPadEventData (replaces 3DO event.h struct) ───────────────────── */
struct ControlPadEventData {
    uint32 cped_ButtonBits;
};

/* ── Input API ───────────────────────────────────────────────────────────── */

/* Initialize input system (keyboard + gamepad) */
bool InitInput(int num_pads);

/* Shutdown input */
void ShutdownInput(void);

/* Poll current input state — matches 3DO GetControlPad signature */
int32 GetControlPad(int32 pad_number, bool wait_for_input, ControlPadEventData *data);

/* Process SDL events (call once per frame before GetControlPad) */
void PumpInputEvents(void);

/* Check if quit was requested (window close, Alt+F4, etc.) */
bool QuitRequested(void);

/* Compatibility alias */
#define InitEventUtility(a, b, c) InitInput(b)
#define KillEventUtility()        ShutdownInput()

#endif /* PLATFORM_INPUT_H */
