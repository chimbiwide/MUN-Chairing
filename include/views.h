#ifndef VIEWS_H
#define VIEWS_H

#include <ui.h>

// view_floor.c: the main screen. the clock and the lists face the room, the buttons are the chair's
void draw_top_bar(App *app, Rectangle bounds);
// operator view: the clock block only; presenting: the clock with the speakers list beside it
void draw_stage(App *app, Rectangle bounds, bool presenting);
void draw_transport(App *app, Rectangle bounds);
void draw_speakers_panel(App *app, Rectangle bounds);
// tiles of the roster narrowed by the speaker box; for_motion makes a click name the motion's proposer
void draw_roster_grid(App *app, Rectangle bounds, bool for_motion);

// the actions behind the buttons and the keyboard
void act_toggle(App *app);
void act_next(App *app);
void act_restart(App *app);
void act_end_caucus(App *app);
void act_nudge(App *app, double seconds);
void act_add_speaker(App *app, SpeakerList *list, const char *name);
void act_insert_next(App *app, SpeakerList *list, const char *name);
void act_undo(App *app);
void act_export(App *app);
void act_import_text(App *app, const char *text, const char *source);
void open_overlay(App *app, Overlay overlay);
// focuses the motion line, replacing its text with prefill unless prefill is NULL
void focus_motion_line(App *app, const char *prefill);

// view_motions.c
void draw_motions_panel(App *app, Rectangle bounds);
// names the proposer of the motion being typed
void set_motion_proposer(App *app, const char *name);

// view_present.c
void draw_present(App *app);

// view_setup.c
void draw_setup(App *app);

// view_rollcall.c
void draw_rollcall(App *app);
void draw_vote(App *app);

// view_settings.c
void draw_settings(App *app);
void draw_help(App *app);
void draw_yield(App *app);
void draw_log(App *app);

// a centred dialog with a title bar and a close button (also Esc). returns the content area
Rectangle begin_dialog(App *app, const char *title, float width, float height, bool *closed);

#endif
