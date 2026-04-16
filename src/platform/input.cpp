/*******************************************************************************
 *  platform/input.cpp — SDL2 input implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "input.h"
#include <cstdlib>

/* ── Module state ────────────────────────────────────────────────────────── */
static bool             g_quit_requested = false;
static uint32           g_current_buttons = 0;
static SDL_GameController *g_gamepad = nullptr;

/* ── Initialization / Shutdown ───────────────────────────────────────────── */

bool InitInput(int num_pads)
{
    (void)num_pads;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_InitSubSystem(GAMECONTROLLER) failed: %s", SDL_GetError());
        /* Non-fatal — keyboard still works */
    }

    /* Open the first available gamepad */
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_gamepad = SDL_GameControllerOpen(i);
            if (g_gamepad) {
                SDL_Log("Gamepad connected: %s", SDL_GameControllerName(g_gamepad));
                break;
            }
        }
    }

    g_quit_requested = false;
    g_current_buttons = 0;
    return true;
}

void ShutdownInput(void)
{
    if (g_gamepad) {
        SDL_GameControllerClose(g_gamepad);
        g_gamepad = nullptr;
    }
}

/* ── Event Pump ──────────────────────────────────────────────────────────── */

void PumpInputEvents(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            g_quit_requested = true;
            exit(0);

        case SDL_CONTROLLERDEVICEADDED:
            if (!g_gamepad) {
                g_gamepad = SDL_GameControllerOpen(ev.cdevice.which);
                if (g_gamepad)
                    SDL_Log("Gamepad connected: %s", SDL_GameControllerName(g_gamepad));
            }
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (g_gamepad && ev.cdevice.which == SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(g_gamepad))) {
                SDL_GameControllerClose(g_gamepad);
                g_gamepad = nullptr;
                SDL_Log("Gamepad disconnected");
            }
            break;

        default:
            break;
        }
    }

    /* ── Build button state from keyboard ────────────────────────────────── */
    g_current_buttons = 0;
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_W]      || keys[SDL_SCANCODE_UP])     g_current_buttons |= ControlUp;
    if (keys[SDL_SCANCODE_S]      || keys[SDL_SCANCODE_DOWN])   g_current_buttons |= ControlDown;
    if (keys[SDL_SCANCODE_A]      || keys[SDL_SCANCODE_LEFT])   g_current_buttons |= ControlLeft;
    if (keys[SDL_SCANCODE_D]      || keys[SDL_SCANCODE_RIGHT])  g_current_buttons |= ControlRight;
    if (keys[SDL_SCANCODE_SPACE]  || keys[SDL_SCANCODE_Z])      g_current_buttons |= ControlA;
    if (keys[SDL_SCANCODE_X])                                    g_current_buttons |= ControlB;
    if (keys[SDL_SCANCODE_C])                                    g_current_buttons |= ControlC;
    if (keys[SDL_SCANCODE_V])                                    g_current_buttons |= ControlX;
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_ESCAPE]) g_current_buttons |= ControlStart;
    if (keys[SDL_SCANCODE_LSHIFT])                               g_current_buttons |= ControlLeftShift;
    if (keys[SDL_SCANCODE_RSHIFT])                               g_current_buttons |= ControlRightShift;

    /* ── Overlay gamepad state ───────────────────────────────────────────── */
    if (g_gamepad) {
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP))    g_current_buttons |= ControlUp;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  g_current_buttons |= ControlDown;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  g_current_buttons |= ControlLeft;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) g_current_buttons |= ControlRight;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_A))          g_current_buttons |= ControlA;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_B))          g_current_buttons |= ControlB;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_X))          g_current_buttons |= ControlX;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_START))      g_current_buttons |= ControlStart;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  g_current_buttons |= ControlLeftShift;
        if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) g_current_buttons |= ControlRightShift;

        /* Left stick as D-pad (with deadzone) */
        const int DEADZONE = 8000;
        int lx = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTX);
        int ly = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTY);
        if (lx < -DEADZONE) g_current_buttons |= ControlLeft;
        if (lx >  DEADZONE) g_current_buttons |= ControlRight;
        if (ly < -DEADZONE) g_current_buttons |= ControlUp;
        if (ly >  DEADZONE) g_current_buttons |= ControlDown;
    }
}

/* ── GetControlPad (3DO-compatible signature) ────────────────────────────── */

int32 GetControlPad(int32 pad_number, bool wait_for_input, ControlPadEventData *data)
{
    (void)pad_number;
    (void)wait_for_input;

    PumpInputEvents();

    if (data) {
        data->cped_ButtonBits = g_current_buttons;
    }
    return 1; /* success */
}

/* ── Quit state ──────────────────────────────────────────────────────────── */

bool QuitRequested(void)
{
    return g_quit_requested;
}
