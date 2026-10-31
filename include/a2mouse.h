#ifndef A2CALC_MOUSE_H
#define A2CALC_MOUSE_H

/* Character coordinates on the 80-column by 24-row text screen. */
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char down;
    unsigned char clicked;
} A2Mouse;

/* Returns 1 when AppleMouse-compatible firmware was found, otherwise 0. */
unsigned char a2mouse_init(void);
void a2mouse_poll(A2Mouse *state);
/* Hide while redrawing text; show afterwards. Calls are idempotent. */
void a2mouse_hide(void);
void a2mouse_show(void);
void a2mouse_shutdown(void);

#endif
