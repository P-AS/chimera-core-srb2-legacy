/* srb2-input.c - the controller: SRB2's own keyboard, and axes for what a key
 * cannot say exactly.
 *
 * The buttons are the game's controls, each on a key of the game's default
 * keyboard (G_Controldefault: WASD, the arrows, Space, Shift...) or, where
 * that has none or shares it, one of the core's choosing (Fire Normal, Center
 * View, the weapon wheel, Custom 1-3), and pressing one is pressing its key: a key event goes to the engine
 * (D_PostEvent) when a button changes, so the menus, the title screen and the
 * game all read it as they read a keyboard, and the game builds its tic
 * command from the held keys with all its own logic (accelerative turning, the
 * Simple control style's camera). Enter and Escape are the menus' own keys,
 * and a prompt's too (Enter or Space is yes, Escape no).
 *
 * The names here are waterbox.config's "buttons", in the same order; each
 * button's letter in a movie's text and TAStudio's column is that file's
 * "mnemonics", and each axis's column header its "header". The ring-slinger
 * controls are in, but for the number row's weapon slots (Weapon 1-7, commented
 * below): Weapon Next and Prev reach every weapon, and seven more buttons would
 * crowd the UI.
 * The bindings are forced to that scheme at start (srb2_input_bind), so no
 * configuration can change what a button means.
 *
 * The axes are added into the tic command through upstream's seam for an
 * external driver (I_BaseTiccmd, which G_BuildTiccmd starts from):
 *   Forward Move, Side Move  -50..50 (MAXPLMOVE), as a key's full press is
 *                            50. The game adds its keys' amount without
 *                            clamping the sum, so an axis counts only while its
 *                            keys are not held (Forward/Backward, Strafe
 *                            Left/Right): the command stays in the game's range.
 *   Turn                     an angle delta, -32768..32767 (1/65536 turn),
 *                            added to what the turn keys give; positive
 *                            turns right (the command's angleturn negated)
 *   Aim                      the look pitch, as the command's aiming (angle >>
 *                            16) negated: positive looks down, as a stick's
 *                            Y; clipped as the game clips it; 0 leaves the
 *                            game's own look (patches/0002)
 */
#include <string.h>

#include "doomdef.h"
#include "g_game.h"
#include "d_event.h"
#include "d_main.h"
#include "d_ticcmd.h"
#include "g_input.h"
#include "keys.h"

#define SRB2_INPUT_TICCMD
#include "srb2-input.h"

static const struct srb2_button g_buttons[] = {
	{ "Forward", gc_forward, 'w' },
	{ "Backward", gc_backward, 's' },
	{ "Strafe Left", gc_strafeleft, 'a' },
	{ "Strafe Right", gc_straferight, 'd' },
	{ "Turn Left", gc_turnleft, KEY_LEFTARROW },
	{ "Turn Right", gc_turnright, KEY_RIGHTARROW },
	{ "Look Up", gc_lookup, KEY_UPARROW },
	{ "Look Down", gc_lookdown, KEY_DOWNARROW },
	{ "Jump", gc_jump, KEY_SPACE },
	{ "Spin", gc_use, KEY_LSHIFT }, /* 2.1's "use" */
	/* the ring-slinger controls (match, CTF; user-asked again 2026-10-11) */
	{ "Fire", gc_fire, KEY_RCTRL },
	{ "Fire Normal", gc_firenormal, KEY_RALT },
	{ "Toss Flag", gc_tossflag, '\'' },
	{ "Center View", gc_centerview, KEY_LCTRL },
	{ "Reset Camera", gc_camreset, 'r' },
	{ "Camera Toggle", gc_camtoggle, 'v' },
	{ "Weapon Next", gc_weaponnext, KEY_MOUSEWHEELUP },
	{ "Weapon Prev", gc_weaponprev, KEY_MOUSEWHEELDOWN },
	/* the weapon slots: left out, to keep the UI uncrowded (user-decided
	 * 2026-10-11); Weapon Next and Prev reach them all
	{ "Weapon 1", gc_wepslot1, '1' },
	{ "Weapon 2", gc_wepslot2, '2' },
	{ "Weapon 3", gc_wepslot3, '3' },
	{ "Weapon 4", gc_wepslot4, '4' },
	{ "Weapon 5", gc_wepslot5, '5' },
	{ "Weapon 6", gc_wepslot6, '6' },
	{ "Weapon 7", gc_wepslot7, '7' },
	*/
	{ "Custom 1", gc_custom1, 'z' },
	{ "Custom 2", gc_custom2, 'x' },
	{ "Custom 3", gc_custom3, 'c' },
	{ "Pause", gc_pause, 'p' },
	/* the menus' own keys, a prompt's too (M_Responder: Enter or Space is yes,
	 * Escape no) */
	{ "Enter", gc_null, KEY_ENTER },
	{ "Escape", gc_null, KEY_ESCAPE },
};
#define NBUTTONS ((int)(sizeof g_buttons / sizeof g_buttons[0]))

static const struct srb2_axis g_axes[] = {
	{ "Forward Move", -MAXPLMOVE, MAXPLMOVE },
	{ "Side Move", -MAXPLMOVE, MAXPLMOVE },
	{ "Turn", -32768, 32767 },
	{ "Aim", -32768, 32767 },
};
#define NAXES ((int)(sizeof g_axes / sizeof g_axes[0]))

/* the controller's state: what the frontend set for the next step, and what
 * the engine was last told (machine memory: a state restores both) */
static uint8_t g_held[NBUTTONS], g_down[NBUTTONS];
static int32_t g_axis[NAXES];

int srb2_input_button_count(void) { return NBUTTONS; }
const struct srb2_button *srb2_input_button(int i) { return i >= 0 && i < NBUTTONS ? &g_buttons[i] : NULL; }
int srb2_input_axis_count(void) { return NAXES; }
const struct srb2_axis *srb2_input_axis(int i) { return i >= 0 && i < NAXES ? &g_axes[i] : NULL; }

void srb2_input_set_button(int i, int held)
{
	if (i >= 0 && i < NBUTTONS)
		g_held[i] = held != 0;
}

void srb2_input_set_axis(int i, int32_t value)
{
	if (i < 0 || i >= NAXES)
		return;
	if (value < g_axes[i].min)
		value = g_axes[i].min;
	if (value > g_axes[i].max)
		value = g_axes[i].max;
	g_axis[i] = value;
}

/* the default keyboard, whatever the configuration says: every binding
 * cleared, the game's defaults, then each button's key its control's alone */
void srb2_input_bind(void)
{
	G_ClearAllControlKeys();
	G_Controldefault();
	for (int i = 0; i < NBUTTONS; i++)
		if (g_buttons[i].control != gc_null)
		{
			gamecontrol[g_buttons[i].control][0] = g_buttons[i].key;
			gamecontrol[g_buttons[i].control][1] = KEY_NULL;
		}
}

/* a step's buttons as key events: releases first, then presses, each in the
 * controller's order */
void srb2_input_post(void)
{
	event_t ev;
	memset(&ev, 0, sizeof ev);
	for (int pass = 0; pass < 2; pass++)
		for (int i = 0; i < NBUTTONS; i++)
		{
			const int release = pass == 0;
			if (release ? (g_down[i] && !g_held[i]) : (!g_down[i] && g_held[i]))
			{
				ev.type = release ? ev_keyup : ev_keydown;
				ev.data1 = g_buttons[i].key;
				D_PostEvent(&ev);
				g_down[i] = g_held[i];
			}
		}
}

static int held(int control)
{
	for (int i = 0; i < NBUTTONS; i++)
		if (g_buttons[i].control == control)
			return g_held[i];
	return 0;
}

/* the axes, as the command G_BuildTiccmd starts from */
void srb2_input_base(ticcmd_t *cmd)
{
	memset(cmd, 0, sizeof *cmd);
	if (!held(gc_forward) && !held(gc_backward))
		cmd->forwardmove = (SINT8)g_axis[0];
	if (!held(gc_strafeleft) && !held(gc_straferight))
		cmd->sidemove = (SINT8)g_axis[1];
	/* the command's angle is counter-clockwise, its pitch up: negated, with
	 * -32768 kept in range */
	cmd->angleturn = (INT16)(g_axis[2] == -32768 ? 32767 : -g_axis[2]);
	cmd->aiming = (INT16)(g_axis[3] == -32768 ? 32767 : -g_axis[3]);
}
