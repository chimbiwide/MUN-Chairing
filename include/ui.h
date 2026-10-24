#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <stdbool.h>
#include <session.h>
#include <chair.h>

#define ENTRY_SIZE 160
#define MAX_MATCHES MAX_DELEGATES

typedef struct {
    Color background;   // window
    Color panel;        // cards
    Color raised;       // inputs, alternate rows
    Color line;         // borders and dividers
    Color text;
    Color muted;        // secondary text
    Color accent;       // primary buttons, current speaker
    Color on_accent;    // text on accent
    Color running;      // a clock that runs
    Color warning;      // last seconds
    Color expired;      // time is up
    Color highlight;    // current row
    Color done;         // crossed out names
    Color paused;       // a clock that is paused
} Theme;

typedef enum {
    OVERLAY_NONE,
    OVERLAY_SETUP,
    OVERLAY_ROLLCALL,
    OVERLAY_VOTE,
    OVERLAY_SETTINGS,
    OVERLAY_HELP,
    OVERLAY_YIELD,
    OVERLAY_LOG,
} Overlay;

// destructive actions take a second press instead of a dialog the whole room would see
typedef enum {
    ARM_NONE,
    ARM_END_CAUCUS,
    ARM_CLEAR_GSL,
    ARM_CLEAR_ROSTER,
    ARM_RESET_ROLL,
    ARM_NEW_SESSION,
    ARM_QUIT,
} Arm;

typedef enum {
    BUTTON_PLAIN,
    BUTTON_PRIMARY,
    BUTTON_GOOD,
    BUTTON_DANGER,
    BUTTON_QUIET,
} ButtonKind;

// a name typed into a box, matched against the roster (or the UN list) as it is typed
typedef struct {
    char text[NAME_SIZE];
    char matched_for[NAME_SIZE];    // text the matches were computed for
    int matches[MAX_MATCHES];
    int match_count;
    int selected;           // highlighted match
    bool countries;         // matches index COUNTRIES instead of the roster
    bool present_only;
} Picker;

typedef struct {
    Session session;
    History history;
    double now;
    float u;                // layout scale: 1 at 1280x720
    const char *session_path;
    bool demo;              // never writes the session file

    Font font;
    Font bold;
    Font digits;
    Theme theme;

    // text box being typed in, NULL when none
    char *focus;
    bool typing;            // focus was set when the frame began
    char *focus_before;     // the box that had focus when the frame began
    char *layout_focus;     // the focus the panels are laid out for; it waits while the mouse button is down,
                            // so a press that takes the focus away can't move a button before its release lands
    bool dirty;             // session changed since the last save
    double saved_at;
    bool save_failed;

    Overlay overlay;
    bool overlay_fresh;     // opened this frame: the click that opened it must not also press its buttons
    bool overlay_undo;      // the overlay took one undo snapshot when it opened
    Arm armed;
    double armed_until;
    bool present;           // projector view
    double mouse_moved_at;  // presentation shows the chair strip for a while after the mouse moves
    Vector2 last_mouse;
    bool blank;             // presentation shows a black screen, the clocks keep running
    bool add_to_gsl;        // during a caucus the add box feeds the GSL instead of the caucus list
    bool show_speakers;     // the speakers' list is out beside the motions ("speakers list" on the motion line, or A)
    float list_scroll;      // first visible row of the speakers list, eased
    float grid_scroll;
    float motion_scroll;
    float log_scroll;
    float roster_scroll;
    float roll_scroll;
    float search_scroll;
    int roll_cursor;
    int vote_cursor;

    Picker speaker_pick;
    Picker yield_pick;
    Picker reply_pick;
    Picker roster_pick;

    char motion_quick[ENTRY_SIZE];

    char committee_edit[TITLE_SIZE];
    char agenda_edit[TOPIC_SIZE];
    char gsl_time_edit[16];
    char warn_time_edit[16];
    char extend_edit[16];
    char vote_title[TOPIC_SIZE];

    char toast[160];
    double toast_until;
    float flash;            // seconds of red flash left after a clock runs out
} App;

// gui.c
void place_text_cursor(const char *text);

// ui.c
void load_fonts(App *app);
void unload_fonts(App *app);
void apply_theme(App *app);

void draw_text(Font font, const char *text, float x, float y, float size, Color color);
float text_width(Font font, const char *text, float size);
// draws text inside bounds, vertically centred, cut with an ellipsis when too wide.
// align: 0 left, 1 centre, 2 right
void draw_text_in(Font font, const char *text, Rectangle bounds, float size, Color color, int align);
// largest size up to max_size at which text fits width
float fit_text(Font font, const char *text, float max_size, float width);
// a clock face as big as fits bounds, digits in equal cells so it doesn't jitter
void draw_clock(App *app, const char *face, Rectangle bounds, Color color);
void draw_strike(Font font, const char *text, float x, float y, float size, Color color);
void draw_panel(App *app, Rectangle bounds);

// raygui text size for the next controls
void gui_size(int size);
bool button(App *app, Rectangle bounds, const char *text, ButtonKind kind);
// a one-line text box: click to type, Enter returns true, Esc or a click elsewhere stops typing
bool text_field(App *app, Rectangle bounds, char *text, int size, const char *placeholder);
void focus_field(App *app, char *text);
bool is_focused(const App *app, const char *text);
// true on a left click inside bounds while the gui is not locked
bool clicked(Rectangle bounds);
bool hovered(Rectangle bounds);

// recomputes matches when the text changed
void update_picker(App *app, Picker *pick);
// name of the match at index (roster or country), NULL when out of range
const char *picker_name(App *app, const Picker *pick, int index);
// what Enter should use: the selected match, else the typed text trimmed (NULL when empty)
const char *picker_choice(App *app, Picker *pick);
// the text box with the best match shown as grey completion. Up/Down move the selection while typing.
// returns true on Enter
bool picker_field(App *app, Picker *pick, Rectangle bounds, const char *placeholder);
void clear_picker(Picker *pick);
// forces every picker to match again (the roster or attendance changed)
void invalidate_pickers(App *app);

void toast(App *app, const char *format, ...) __attribute__((format(printf, 2, 3)));
void draw_toast(App *app);

// the clock colour for a countdown: running, warning, expired or paused
Color clock_color(App *app, const Clock *clock, double now);

// marks the session changed so it is saved soon
void touch(App *app);
// remember() then the label for undo, and touch()
void before(App *app, const char *label);
// true on the second press within 3 s; the first press arms and returns false
bool confirm_armed(App *app, Arm arm);
bool is_armed(const App *app, Arm arm);
// chair-sized text: 18 px at 1280x720, scaled with the window
float chair_text(const App *app);
// the floor list the add box feeds
SpeakerList *add_target(App *app);
// a MemAlloc'd UTF-8 copy of text; text that isn't valid UTF-8 is read as Windows-1252,
// which is what Excel saves CSV files in. MemFree it
char *utf8_text(const char *text);
// the roster's spelling of a typed name, also through country aliases ("usa" -> "United States");
// the typed text itself when nothing matches
const char *roster_spelling(const Session *session, const char *typed);

#endif
