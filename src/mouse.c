/* Apple IIc mouse integration using cc65's statically linked AppleMouse driver.
 * The firmware/IRQ implementation and text cursor remain upstream cc65 code.
 */
#include "a2mouse.h"

#ifdef __CC65__
#include <mouse.h>

static unsigned char available;
static unsigned char visible;
static unsigned char previous_down;

unsigned char a2mouse_init(void)
{
    if (available) return 1;
    if (mouse_install(&mouse_def_callbacks,
                      mouse_static_stddrv) != MOUSE_ERR_OK) return 0;
    available = 1;
    previous_down = 0;
    /* Upstream defaults: x 0..279, y 0..191. Its text cursor uses these
     * coordinates, so clamping directly to 79x23 would be incorrect. */
    mouse_move(140, 96);
    mouse_show();
    visible = 1;
    return 1;
}

void a2mouse_poll(A2Mouse *state)
{
    struct mouse_info info;
    unsigned int x;
    unsigned int y;
    state->clicked = 0;
    state->down = 0;
    state->x = 0;
    state->y = 0;
    if (!available) return;
    mouse_info(&info);
    x = (unsigned int)info.pos.x;
    y = (unsigned int)info.pos.y;
    /* This is the same partition into text cells as cc65 mcbdefault.s:
     * seven firmware units span a pair of 80-column characters. */
    state->x = (unsigned char)(x * 2 / 7);
    state->y = (unsigned char)(y / 8);
    if (state->x > 79) state->x = 79;
    if (state->y > 23) state->y = 23;
    state->down = (info.buttons & MOUSE_BTN_LEFT) != 0;
    state->clicked = state->down && !previous_down;
    previous_down = state->down;
}

void a2mouse_hide(void)
{
    if (available && visible) {
        mouse_hide();
        visible = 0;
    }
}

void a2mouse_show(void)
{
    if (available && !visible) {
        mouse_show();
        visible = 1;
    }
}

void a2mouse_shutdown(void)
{
    if (available) {
        a2mouse_hide();
        mouse_uninstall();
        available = 0;
        previous_down = 0;
    }
}

#else
/* Native tests of the calculation/file code do not emulate Apple hardware. */
unsigned char a2mouse_init(void) { return 0; }
void a2mouse_poll(A2Mouse *state)
{
    state->x = state->y = state->down = state->clicked = 0;
}
void a2mouse_hide(void) {}
void a2mouse_show(void) {}
void a2mouse_shutdown(void) {}
#endif
