#include <views.h>
#include <countries.h>
#include <duration.h>
#include <store.h>
#include <raygui.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// the list scrolls itself to the current speaker unless the chair scrolled it recently
static double manual_scroll_until = 0.0;

static Color fade_to(Color color, Color base, float amount) {
    return (Color){
        (unsigned char)(color.r + (base.r - color.r) * amount),
        (unsigned char)(color.g + (base.g - color.g) * amount),
        (unsigned char)(color.b + (base.b - color.b) * amount),
        255,
    };
}

static bool caucus_listed(const Session *session) {
    return session->caucus.active && caucus_has_speakers(session->caucus.motion.type);
}

static bool unmod_on(const Session *session) {
    return session->caucus.active && !caucus_has_speakers(session->caucus.motion.type);
}

// nobody more can be called, judged the way chair.c calls speakers: a slot needs a full slot left,
// otherwise (and always in a consultation) any time left will do
static bool caucus_used_up(const Session *session, double now) {
    const Caucus *caucus = &session->caucus;
    if (caucus->speakers.current >= 0) return false;
    double left = clock_left(&caucus->total, now);
    bool slots = session->settings.mod_clock == MOD_CLOCK_SLOTS && caucus->motion.type != MOTION_CONSULT;
    if (slots) return left < caucus->motion.speaker - 0.5;
    return left <= 0.0;
}

// the heading of a speakers list
static const char *list_title(const Session *session, const SpeakerList *list) {
    if (list == &session->gsl) return "General speakers' list";
    switch (session->caucus.motion.type) {
        case MOTION_CONSULT: return "Consultation speakers";
        case MOTION_ROUND_ROBIN: return "Round robin order";
        default: return "Moderated caucus speakers";
    }
}

static const char *mode_title(const Session *session) {
    if (session->phase == PHASE_VOTING) return "VOTING PROCEDURE";
    if (session->phase == PHASE_SUSPENDED) return "COMMITTEE SUSPENDED";
    if (session->phase == PHASE_ADJOURNED) return "COMMITTEE ADJOURNED";
    if (!session->caucus.active) return "GENERAL SPEAKERS' LIST";
    switch (session->caucus.motion.type) {
        case MOTION_UNMOD: return "UNMODERATED CAUCUS";
        case MOTION_CONSULT: return "CONSULTATION OF THE WHOLE";
        case MOTION_ROUND_ROBIN: return "ROUND ROBIN";
        default: return "MODERATED CAUCUS";
    }
}

// setup, roll call and votes change many small things: one undo step covers the whole visit,
// and a visit that changed nothing leaves no step behind
static void settle_overlay_undo(App *app) {
    if (!app->overlay_undo) return;
    app->overlay_undo = false;
    const History *history = &app->history;
    if (history->count > 0 && memcmp(&history->states[history->newest], &app->session, sizeof(Session)) == 0) {
        forget_last(&app->history);
    }
}

void open_overlay(App *app, Overlay overlay) {
    Session *session = &app->session;
    settle_overlay_undo(app);
    if (overlay == OVERLAY_SETUP || overlay == OVERLAY_ROLLCALL || overlay == OVERLAY_VOTE || overlay == OVERLAY_SETTINGS) {
        const char *label = overlay == OVERLAY_SETUP ? "setup changes" : overlay == OVERLAY_ROLLCALL ? "roll call changes" :
                            overlay == OVERLAY_VOTE ? "vote changes" : "settings changes";
        remember(&app->history, session, label);
        app->overlay_undo = true;
    }
    app->overlay = overlay;
    app->overlay_fresh = overlay != OVERLAY_NONE;
    app->focus = NULL;
    app->armed = ARM_NONE;
    if (overlay == OVERLAY_SETUP) {
        copy_text(app->committee_edit, sizeof(app->committee_edit), session->committee);
        copy_text(app->agenda_edit, sizeof(app->agenda_edit), session->agenda);
        format_clock(session->settings.gsl_time, app->gsl_time_edit, sizeof(app->gsl_time_edit));
        clear_picker(&app->roster_pick);
        app->roster_pick.countries = true;
    }
    else if (overlay == OVERLAY_SETTINGS) {
        format_clock(session->settings.gsl_time, app->gsl_time_edit, sizeof(app->gsl_time_edit));
        snprintf(app->warn_time_edit, sizeof(app->warn_time_edit), "%d", session->settings.warn_time);
    }
    else if (overlay == OVERLAY_YIELD) {
        clear_picker(&app->yield_pick);
        clear_picker(&app->reply_pick);
        app->yield_pick.present_only = true;
        app->reply_pick.present_only = true;
    }
    else if (overlay == OVERLAY_ROLLCALL) {
        if (app->roll_cursor >= session->roster.count) app->roll_cursor = 0;
    }
    else if (overlay == OVERLAY_VOTE) {
        if (app->vote_cursor >= session->roster.count) app->vote_cursor = 0;
    }
}

void focus_motion_line(App *app, const char *prefill) {
    if (prefill != NULL) copy_text(app->motion_quick, sizeof(app->motion_quick), prefill);
    focus_field(app, app->motion_quick);
}

void open_speakers_list(App *app) {
    app->show_speakers = true;
    focus_field(app, app->speaker_pick.text);
}

void act_toggle(App *app) {
    Session *session = &app->session;
    if (session->phase != PHASE_DEBATE) {
        toast(app, "The committee is in %s", phase_name(session->phase));
        return;
    }
    if (toggle_floor(session, app->now)) {
        touch(app);
        return;
    }
    if (caucus_listed(session) && caucus_used_up(session, app->now)) {
        toast(app, "The caucus time is used up");
    }
    else toast(app, "Add a speaker first");
}

void act_next(App *app) {
    Session *session = &app->session;
    if (unmod_on(session)) {
        toast(app, "An unmoderated caucus has no speakers");
        return;
    }
    before(app, "next speaker");
    if (!next_speaker(session, app->now)) {
        forget_last(&app->history);
        toast(app, "Nobody is waiting to speak");
        return;
    }
    manual_scroll_until = 0.0;
    SpeakerList *list = floor_list(session);
    if (list->current < 0) {
        if (list == &session->gsl) toast(app, "The speakers' list is empty");
        else if (pending_count(list) > 0) toast(app, "The caucus time is used up");
        else if (caucus_listed(session)) toast(app, "No more speakers");
    }
}

void act_restart(App *app) {
    if (floor_list(&app->session)->current < 0 && !unmod_on(&app->session)) return;
    before(app, "restart the clock");
    restart_speaker(&app->session, app->now);
}

void act_end_caucus(App *app) {
    if (!app->session.caucus.active) return;
    before(app, "end the caucus");
    end_caucus(&app->session, app->now);
    app->add_to_gsl = false;
}

void act_add_speaker(App *app, SpeakerList *list, const char *typed) {
    if (typed == NULL || typed[0] == '\0') return;
    Session *session = &app->session;
    char name[NAME_SIZE];
    copy_text(name, sizeof(name), roster_spelling(session, typed));
    before(app, "add a speaker");
    int result = add_to_list(session, list, name);
    if (result == SPEAKER_WAITING) {
        forget_last(&app->history);
        toast(app, "%s is already on the list", name);
        return;
    }
    if (result < 0) {
        forget_last(&app->history);
        toast(app, result == SPEAKER_FULL ? "The list is full" : "Type a delegation first");
        return;
    }
    int delegate = find_delegate(&session->roster, name);
    if (delegate < 0) toast(app, "%s is not on the roster, added anyway", name);
    else if (session->roster.roll_taken && session->roster.items[delegate].attendance == ABSENT) {
        toast(app, "%s is marked absent, added anyway", name);
    }
}

void act_insert_next(App *app, SpeakerList *list, const char *typed) {
    if (typed == NULL || typed[0] == '\0') return;
    Session *session = &app->session;
    char name[NAME_SIZE];
    copy_text(name, sizeof(name), roster_spelling(session, typed));
    int at = list->current >= 0 ? list->current + 1 : next_pending(list);
    if (at < 0) at = list->count;
    before(app, "add a speaker next");
    int result = insert_speaker(list, at, name);
    if (result < 0) {
        forget_last(&app->history);
        toast(app, result == SPEAKER_WAITING ? "%s is already on the list" : "Couldn't add %s", name);
        return;
    }
    toast(app, "%s speaks next", name);
}

void act_nudge(App *app, double seconds) {
    Session *session = &app->session;
    Clock *clock = unmod_on(session) ? &session->caucus.total : floor_clock(session);
    if (clock == NULL || clock->limit <= 0.0) return;
    if (!unmod_on(session) && floor_list(session)->current < 0) {
        toast(app, "Nobody has the floor");
        return;
    }
    before(app, "adjust the clock");
    extend_clock(clock, seconds);
    if (clock->limit < 1.0) clock->limit = 1.0;
}

void act_undo(App *app) {
    char label[UNDO_LABEL];
    const char *what = undo_label(&app->history);
    copy_text(label, sizeof(label), what);
    if (!undo(&app->history, &app->session)) {
        toast(app, "Nothing to undo");
        return;
    }
    touch(app);
    invalidate_pickers(app);
    apply_theme(app);
    // an open dialog's boxes still hold what was typed; they would copy it straight back
    Session *session = &app->session;
    if (app->overlay == OVERLAY_SETUP) {
        copy_text(app->committee_edit, sizeof(app->committee_edit), session->committee);
        copy_text(app->agenda_edit, sizeof(app->agenda_edit), session->agenda);
    }
    if (app->overlay == OVERLAY_SETUP || app->overlay == OVERLAY_SETTINGS) {
        format_clock(session->settings.gsl_time, app->gsl_time_edit, sizeof(app->gsl_time_edit));
        snprintf(app->warn_time_edit, sizeof(app->warn_time_edit), "%d", session->settings.warn_time);
    }
    // the snapshot the dialog took when it opened may be the one just restored
    app->overlay_undo = false;
    toast(app, "Undid: %s", label);
}

void act_export(App *app) {
    char log_name[64];
    char stats_name[64];
    stamped_name("mun_log", "txt", log_name, sizeof(log_name));
    stamped_name("mun_stats", "csv", stats_name, sizeof(stats_name));
    if (export_log(&app->session, log_name) && export_stats(&app->session, stats_name)) {
        toast(app, "Wrote %s and %s", log_name, stats_name);
    }
    else toast(app, "Couldn't write the export files here");
}

void act_import_text(App *app, const char *text, const char *source) {
    before(app, "import delegations");
    int added = import_delegates(&app->session.roster, text);
    if (added == 0) {
        forget_last(&app->history);
        toast(app, "No new delegations in %s", source);
        return;
    }
    invalidate_pickers(app);
    toast(app, "Added %d delegation%s from %s", added, added == 1 ? "" : "s", source);
}

void draw_top_bar(App *app, Rectangle bounds) {
    const Theme *t = &app->theme;
    Session *session = &app->session;
    float u = app->u;
    DrawRectangleRec(bounds, t->panel);
    DrawLineEx((Vector2){ bounds.x, bounds.y + bounds.height }, (Vector2){ bounds.x + bounds.width, bounds.y + bounds.height }, 1.0f, t->line);

    float pad = 14.0f * u;
    float size = chair_text(app);
    gui_size((int)(size * 0.95f));

    struct { const char *label; Overlay overlay; } items[] = {
        { "Setup", OVERLAY_SETUP }, { "Roll call", OVERLAY_ROLLCALL }, { "Vote", OVERLAY_VOTE },
        { "Log", OVERLAY_LOG }, { "Settings", OVERLAY_SETTINGS },
    };
    int count = (int)(sizeof(items) / sizeof(items[0]));
    float buttonH = bounds.height - 16.0f * u;
    float x = bounds.x + bounds.width - pad;

    float presentW = (float)GuiGetTextWidth("Present") + 34.0f * u;
    x -= presentW;
    if (button(app, (Rectangle){ x, bounds.y + 8.0f * u, presentW, buttonH }, "Present", BUTTON_PRIMARY)) app->present = true;
    for (int i = count - 1; i >= 0; i--) {
        float w = (float)GuiGetTextWidth(items[i].label) + 26.0f * u;
        x -= w + 6.0f * u;
        if (button(app, (Rectangle){ x, bounds.y + 8.0f * u, w, buttonH }, items[i].label, BUTTON_QUIET)) {
            open_overlay(app, items[i].overlay);
        }
    }

    // ending a caucus (also X twice) and resuming debate after a suspension or the vote
    if (session->phase != PHASE_DEBATE) {
        float w = (float)GuiGetTextWidth("Resume debate") + 26.0f * u;
        x -= w + 16.0f * u;
        if (button(app, (Rectangle){ x, bounds.y + 8.0f * u, w, buttonH }, "Resume debate", BUTTON_PRIMARY)) {
            before(app, "resume debate");
            set_phase(session, PHASE_DEBATE, app->now);
        }
    }
    else if (session->caucus.active) {
        float w = (float)GuiGetTextWidth("End caucus") + 26.0f * u;
        x -= w + 16.0f * u;
        const char *label = is_armed(app, ARM_END_CAUCUS) ? "Sure?" : "End caucus";
        if (button(app, (Rectangle){ x, bounds.y + 8.0f * u, w, buttonH }, label, BUTTON_DANGER) && confirm_armed(app, ARM_END_CAUCUS)) {
            act_end_caucus(app);
        }
    }

    int present = count_present(&session->roster);
    int members = session->roster.count;
    int quorum = session->settings.quarter_quorum ? (members + 3) / 4 : one_third(members);
    if (session->roster.roll_taken && present < quorum) {
        float warnSize = size * 0.9f;
        float warnW = text_width(app->bold, "NO QUORUM", warnSize) + 24.0f * u;
        x -= warnW + 16.0f * u;
        Rectangle warnRect = { x, bounds.y + 11.0f * u, warnW, bounds.height - 22.0f * u };
        DrawRectangleRounded(warnRect, 0.5f, 8, t->expired);
        draw_text_in(app->bold, "NO QUORUM", warnRect, warnSize, t->on_accent, 1);
    }

    float textW = x - pad - 12.0f * u;
    const char *committee = session->committee[0] != '\0' ? session->committee : "Untitled committee";
    if (session->agenda[0] != '\0') {
        draw_text_in(app->bold, committee, (Rectangle){ pad, bounds.y + 6.0f * u, textW, size * 1.15f }, size * 1.05f, t->text, 0);
        draw_text_in(app->font, session->agenda, (Rectangle){ pad, bounds.y + 6.0f * u + size * 1.2f, textW, size }, size * 0.82f, t->muted, 0);
    }
    else draw_text_in(app->bold, committee, (Rectangle){ pad, bounds.y, textW, bounds.height }, size * 1.1f, t->text, 0);
}

// the word beside the heading when the digits alone don't tell: PAUSED, TIME or OVERTIME
static const char *clock_state(App *app, const Clock *clock) {
    double now = app->now;
    if (clock_expired(clock, now)) return clock_left(clock, now) <= -1.0 ? "OVERTIME" : "TIME";
    if (!clock->running && clock_elapsed(clock, now) > 0.0) return "PAUSED";
    return NULL;
}

// how the clock block looks in each state: a fill and digit colour, so the state never rests on colour alone
typedef struct {
    Color fill;
    Color digits;
    Color ink;      // names and headings on the fill
    Color soft;     // secondary text on the fill
} ClockLook;

static ClockLook clock_look(App *app, const Clock *clock) {
    const Theme *t = &app->theme;
    bool dark = app->session.settings.dark;
    double now = app->now;
    ClockLook look = { t->panel, t->text, t->text, t->muted };
    if (clock_expired(clock, now)) {
        bool pulse = app->flash > 0.0f && fmodf(app->flash, 1.0f) > 0.5f;
        if (app->flash > 0.0f) look.fill = pulse ? (dark ? (Color){ 0xC6, 0x28, 0x28, 255 } : RED)
                                                 : (dark ? (Color){ 0x7A, 0x1F, 0x1F, 255 } : MAROON);
        else look.fill = dark ? (Color){ 0x7A, 0x1F, 0x1F, 255 } : MAROON;
        look.digits = (Color){ 255, 255, 255, 255 };
    }
    else if (clock_warning(clock, now, app->session.settings.warn_time)) {
        look.fill = dark ? (Color){ 0x5A, 0x3E, 0x00, 255 } : (Color){ 0xFF, 0xEC, 0x99, 255 };
        look.digits = dark ? (Color){ 0xFF, 0xD1, 0x66, 255 } : BLACK;
    }
    else if (!clock->running && clock_elapsed(clock, now) > 0.0) {
        look.fill = dark ? (Color){ 0x0F, 0x2A, 0x4A, 255 } : t->highlight;
        look.digits = t->paused;
    }
    else if (!clock->running) look.digits = t->muted;

    // text on the fill follows how light the fill is
    float luma = (0.299f * look.fill.r + 0.587f * look.fill.g + 0.114f * look.fill.b) / 255.0f;
    bool strong = look.fill.r != t->panel.r || look.fill.g != t->panel.g || look.fill.b != t->panel.b;
    if (strong) {
        look.ink = luma < 0.5f ? (Color){ 255, 255, 255, 255 } : BLACK;
        look.soft = luma < 0.5f ? (Color){ 0xE6, 0xEA, 0xF0, 255 } : (Color){ 0x3A, 0x40, 0x48, 255 };
    }
    return look;
}

// voting procedure, suspension and adjournment: the phase in large type instead of a clock
static void draw_phase_card(App *app, Rectangle block, bool presenting) {
    const Theme *t = &app->theme;
    Session *session = &app->session;
    float h = block.height;
    float inset = (presenting ? 0.03f : 0.035f) * block.width;
    float innerW = block.width - 2 * inset;
    float roundness = 10.0f * app->u / (block.height < block.width ? block.height : block.width);
    DrawRectangleRounded(block, roundness, 6, t->panel);
    DrawRectangleRoundedLinesEx(block, roundness, 6, 1.0f, t->line);

    float titleSize = h * 0.07f;
    draw_text_in(app->bold, mode_title(session), (Rectangle){ block.x + inset, block.y + h * 0.04f, innerW, titleSize * 1.1f }, titleSize, t->muted, 0);

    // the time of day while delegates are away, the word VOTING while they vote
    Rectangle big = { block.x + inset, block.y + h * 0.2f, innerW, h * 0.65f };
    if (session->phase == PHASE_VOTING) {
        float size = fit_text(app->bold, "VOTING", h * 0.32f, innerW);
        draw_text_in(app->bold, "VOTING", big, size, t->text, 1);
    }
    else {
        char now[16] = "--:--";
        time_t wall = time(NULL);
        struct tm *local = localtime(&wall);
        if (local != NULL) strftime(now, sizeof(now), "%H:%M", local);
        draw_clock(app, now, big, t->text);
    }
}

// the speaker clock, or the total of an unmod
static void draw_clock_block(App *app, Rectangle block, bool presenting) {
    const Theme *t = &app->theme;
    Session *session = &app->session;
    double now = app->now;
    float h = block.height;
    float inset = (presenting ? 0.03f : 0.035f) * block.width;
    float innerW = block.width - 2 * inset;

    bool unmod = unmod_on(session);
    SpeakerList *list = floor_list(session);
    // once a mod can call nobody more its own clock is what matters
    bool over = caucus_listed(session) && caucus_used_up(session, now);
    const Clock *clock = unmod || over ? &session->caucus.total : floor_clock(session);
    if (session->phase != PHASE_DEBATE && !session->caucus.active) {
        draw_phase_card(app, block, presenting);
        return;
    }
    ClockLook look = clock_look(app, clock);
    float roundness = 10.0f * app->u / (block.height < block.width ? block.height : block.width);
    DrawRectangleRounded(block, roundness, 6, look.fill);
    DrawRectangleRoundedLinesEx(block, roundness, 6, 1.0f, t->line);

    // heading: what the committee is doing, and the state when the digits don't show it
    float titleSize = h * 0.07f;
    float y = block.y + h * 0.04f;
    const char *state = clock_state(app, clock);
    float stateW = state != NULL ? text_width(app->bold, state, titleSize) + 16.0f * app->u : 0.0f;
    draw_text_in(app->bold, mode_title(session), (Rectangle){ block.x + inset, y, innerW - stateW, titleSize * 1.1f }, titleSize, look.soft, 0);
    if (state != NULL) draw_text_in(app->bold, state, (Rectangle){ block.x + block.width - inset - stateW, y, stateW, titleSize * 1.1f }, titleSize, look.ink, 2);
    y += titleSize * 1.15f;
    const char *topic = session->caucus.active ? session->caucus.motion.topic : "";
    if (topic[0] != '\0') {
        draw_text_in(app->font, topic, (Rectangle){ block.x + inset, y, innerW, h * 0.06f }, h * 0.052f, look.soft, 0);
        y += h * 0.07f;
    }
    else y += h * 0.02f;

    // the speaker
    if (!unmod) {
        const char *name = NULL;
        char called[NAME_SIZE + 32];
        if (list->current >= 0) name = list->items[list->current].name;
        Color nameColor = look.ink;
        if (over) {
            name = "Caucus time elapsed";
            nameColor = look.soft;
        }
        else if (name == NULL) {
            int next = next_pending(list);
            if (next >= 0) {
                snprintf(called, sizeof(called), "Next: %s", list->items[next].name);
                name = called;
            }
            else name = list->count > 0 ? "List finished" : "No speakers yet";
            nameColor = look.soft;
        }
        float nameH = h * 0.12f;
        float nameSize = fit_text(app->bold, name, h * 0.115f, innerW);
        draw_text_in(app->bold, name, (Rectangle){ block.x + inset, y, innerW, nameH }, nameSize, nameColor, 0);
        if (!over && list->current >= 0 && list->items[list->current].note[0] != '\0') {
            float noteX = block.x + inset + text_width(app->bold, name, nameSize) + 16.0f * app->u;
            draw_text_in(app->font, list->items[list->current].note, (Rectangle){ noteX, y, block.x + block.width - inset - noteX, nameH }, h * 0.045f, look.soft, 0);
        }
        y += nameH + h * 0.02f;
    }

    // the clock, as large as the rest of the card allows
    char face[16];
    format_left(clock->limit > 0.0 ? clock_left(clock, now) : clock_elapsed(clock, now), face, sizeof(face));
    if (clock->limit <= 0.0) format_clock((int)clock_elapsed(clock, now), face, sizeof(face));
    draw_clock(app, face, (Rectangle){ block.x + inset, y, innerW, block.y + h * 0.95f - y }, look.digits);
}

// the rows of a speakers list with crossed out names; in operator mode hovering a row shows
// buttons to move, remove and restore it
static void draw_speaker_rows(App *app, Rectangle area, bool presenting, SpeakerList *list) {
    const Theme *t = &app->theme;
    Session *session = &app->session;
    double now = app->now;
    float u = app->u;
    float pad = 14.0f * u;
    float rowH = presenting ? area.height * 0.1f : 34.0f * u;
    if (presenting && rowH < 30.0f) rowH = 30.0f;
    int visible = (int)(area.height / rowH);
    if (visible < 1) visible = 1;

    if (list->count == 0) {
        draw_text_in(app->font, "No speakers yet", (Rectangle){ area.x + pad, area.y, area.width - 2 * pad, rowH }, rowH * 0.45f, t->muted, 0);
        return;
    }

    // keep the current speaker on the second row unless the chair is scrolling by hand
    int maxFirst = list->count - visible;
    if (maxFirst < 0) maxFirst = 0;
    if (!presenting && hovered(area) && GetMouseWheelMove() != 0.0f) {
        app->list_scroll -= GetMouseWheelMove() * 2.0f;
        manual_scroll_until = now + 6.0;
    }
    if (now > manual_scroll_until || presenting) {
        int anchor = list->current >= 0 ? list->current : next_pending(list);
        if (anchor < 0) anchor = list->count;
        float target = (float)(anchor - 1);
        app->list_scroll += (target - app->list_scroll) * 0.25f;
    }
    if (app->list_scroll > (float)maxFirst) app->list_scroll = (float)maxFirst;
    if (app->list_scroll < 0.0f) app->list_scroll = 0.0f;

    float textSize = rowH * 0.5f;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    bool mouseIn = !presenting && hovered(area);
    for (int i = 0; i < list->count; i++) {
        float y = area.y + ((float)i - app->list_scroll) * rowH;
        if (y + rowH < area.y || y > area.y + area.height) continue;
        Speaker *speaker = &list->items[i];
        Rectangle row = { area.x, y, area.width, rowH - 2.0f };
        bool current = i == list->current;

        if (current) {
            Color state = floor_clock(session) != NULL && list == floor_list(session) ? clock_color(app, floor_clock(session), now) : t->accent;
            DrawRectangleRounded(row, 0.25f, 6, t->highlight);
            DrawRectangle((int)row.x, (int)row.y + 2, (int)(5.0f * u), (int)row.height - 4, state);
        }
        else if (i % 2 == 1) DrawRectangleRec(row, fade_to(t->raised, t->panel, 0.4f));

        char number[12];
        snprintf(number, sizeof(number), "%d", i + 1);
        float numberW = text_width(app->font, "000", textSize * 0.8f);
        draw_text_in(app->font, number, (Rectangle){ row.x + 10.0f * u, row.y, numberW, row.height }, textSize * 0.8f, t->muted, 2);

        float nameX = row.x + 10.0f * u + numberW + 12.0f * u;
        float noteW = 0.0f;
        if (speaker->note[0] != '\0') noteW = fminf(text_width(app->font, speaker->note, textSize * 0.75f) + 12.0f * u, row.width * 0.35f);
        float nameW = row.x + row.width - nameX - noteW - 24.0f * u;
        Color nameColor = current ? t->text : (speaker->done ? t->done : t->text);
        Font nameFont = current ? app->bold : app->font;
        float shownSize = fit_text(nameFont, speaker->name, textSize, nameW);
        float nameY = row.y + (row.height - shownSize) / 2.0f;
        if (speaker->done) draw_strike(nameFont, speaker->name, nameX, nameY, shownSize, nameColor);
        else draw_text(nameFont, speaker->name, nameX, nameY, shownSize, nameColor);
        if (noteW > 0.0f) {
            draw_text_in(app->font, speaker->note, (Rectangle){ row.x + row.width - noteW - 12.0f * u, row.y, noteW, row.height }, textSize * 0.75f, t->muted, 2);
        }

        // chair tools on the hovered row
        if (mouseIn && CheckCollisionPointRec(GetMousePosition(), row)) {
            float b = row.height - 6.0f;
            float bx = row.x + row.width - 4.0f - b;
            DrawRectangle((int)(bx - 3 * (b + 4.0f) - 6.0f), (int)row.y, (int)(4 * (b + 4.0f) + 10.0f), (int)row.height, current ? t->highlight : t->panel);
            gui_size((int)(textSize * 0.8f));
            if (button(app, (Rectangle){ bx, row.y + 3.0f, b, b }, GuiIconText(ICON_CROSS_SMALL, NULL), BUTTON_PLAIN)) {
                before(app, "remove a speaker");
                remove_from_list(session, list, i, now);
                break;
            }
            bx -= b + 4.0f;
            if (speaker->done) {
                if (button(app, (Rectangle){ bx, row.y + 3.0f, b, b }, GuiIconText(ICON_UNDO, NULL), BUTTON_PLAIN)) {
                    before(app, "restore a speaker");
                    set_done(list, i, false, 0.0);
                    break;
                }
                bx -= b + 4.0f;
            }
            if (button(app, (Rectangle){ bx, row.y + 3.0f, b, b }, GuiIconText(ICON_ARROW_DOWN_FILL, NULL), BUTTON_PLAIN)) {
                before(app, "move a speaker");
                if (!move_speaker(list, i, 1)) forget_last(&app->history);
                manual_scroll_until = now + 6.0;
                break;
            }
            bx -= b + 4.0f;
            if (button(app, (Rectangle){ bx, row.y + 3.0f, b, b }, GuiIconText(ICON_ARROW_UP_FILL, NULL), BUTTON_PLAIN)) {
                before(app, "move a speaker");
                if (!move_speaker(list, i, -1)) forget_last(&app->history);
                manual_scroll_until = now + 6.0;
                break;
            }
        }
    }
    EndScissorMode();

    // a hint that more rows are hidden
    if (app->list_scroll > 0.05f) DrawRectangleGradientV((int)area.x, (int)area.y, (int)area.width, (int)(10 * u), Fade(t->line, 0.8f), Fade(t->panel, 0.0f));
    if (app->list_scroll < (float)maxFirst - 0.05f) DrawRectangleGradientV((int)area.x, (int)(area.y + area.height - 10 * u), (int)area.width, (int)(10 * u), Fade(t->panel, 0.0f), Fade(t->line, 0.8f));
}

// a speakers list with its heading, as the room sees it
static void draw_list_block(App *app, Rectangle block, bool presenting, SpeakerList *list) {
    const Theme *t = &app->theme;
    float u = app->u;
    draw_panel(app, block);
    float pad = 14.0f * u;
    float headSize = presenting ? block.height * 0.05f : chair_text(app);
    draw_text_in(app->bold, "Speakers", (Rectangle){ block.x + pad, block.y + pad * 0.6f, block.width - 2 * pad, headSize * 1.3f }, headSize, t->text, 0);
    Rectangle area = { block.x + 6.0f * u, block.y + pad * 0.6f + headSize * 1.5f, block.width - 12.0f * u, 0 };
    area.height = block.y + block.height - area.y - 6.0f * u;
    draw_speaker_rows(app, area, presenting, list);
}

// the pending motions as the room sees them, in voting order
static void draw_motion_block(App *app, Rectangle block) {
    const Theme *t = &app->theme;
    const MotionList *motions = &app->session.motions;
    draw_panel(app, block);
    float pad = 14.0f * app->u;
    float headSize = block.height * 0.05f;
    draw_text_in(app->bold, "Motions on the floor", (Rectangle){ block.x + pad, block.y + pad * 0.6f, block.width - 2 * pad, headSize * 1.3f }, headSize, t->text, 0);
    float rowH = block.height * 0.15f;
    float y = block.y + pad * 0.6f + headSize * 1.6f;
    for (int i = 0; i < motions->count && y + rowH <= block.y + block.height; i++) {
        // describe_motion leads with the kind and length; the rest (topic, proposer) goes underneath
        char what[TOPIC_SIZE + NAME_SIZE + 48];
        describe_motion(&motions->items[i], what, sizeof(what));
        char *rest = strstr(what, " \xC2\xB7 ");
        const char *detail = "";
        if (rest != NULL) {
            *rest = '\0';
            detail = rest + 4;
        }
        char number[12];
        snprintf(number, sizeof(number), "%d", i + 1);
        Rectangle row = { block.x + 6.0f * app->u, y, block.width - 12.0f * app->u, rowH - 6.0f };
        if (i == 0) DrawRectangleRounded(row, 0.2f, 6, t->highlight);
        float numberW = rowH * 0.42f;
        draw_text_in(app->bold, number, (Rectangle){ row.x + 8.0f, row.y, numberW, row.height }, rowH * 0.34f, t->muted, 1);
        float textX = row.x + numberW + 16.0f;
        float textW = row.x + row.width - textX - 8.0f;
        draw_text_in(app->bold, what, (Rectangle){ textX, row.y + row.height * 0.06f, textW, row.height * 0.5f }, rowH * 0.3f, t->text, 0);
        draw_text_in(app->font, detail, (Rectangle){ textX, row.y + row.height * 0.5f, textW, row.height * 0.42f }, rowH * 0.24f, t->muted, 0);
        y += rowH;
    }
}

void draw_stage(App *app, Rectangle bounds) {
    Session *session = &app->session;
    float u = app->u;
    // the list beside the clock when the stage is wide enough, never during an unmod
    float pad = bounds.height * 0.03f;
    Rectangle inner = { bounds.x + pad, bounds.y + pad, bounds.width - 2 * pad, bounds.height - 2 * pad };
    bool wide = !unmod_on(session) && inner.width / inner.height > 1.35f;
    if (!wide) {
        draw_clock_block(app, inner, true);
        return;
    }
    float listW = inner.width * 0.32f;
    if (listW < 300.0f * u) listW = 300.0f * u;
    draw_clock_block(app, (Rectangle){ inner.x, inner.y, inner.width - listW - pad, inner.height }, true);
    Rectangle side = { inner.x + inner.width - listW, inner.y, listW, inner.height };
    // while motions wait and nobody speaks, the room sees them in voting order to check they were taken down right
    const Clock *clock = floor_clock(session);
    if (session->motions.count > 0 && !session->caucus.active && (clock == NULL || !clock->running)) draw_motion_block(app, side);
    else draw_list_block(app, side, true, floor_list(session));
}

// the tiles of the roster, narrowed by the speaker box. while a motion is typed a click names its proposer
void draw_roster_grid(App *app, Rectangle area, bool for_motion) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    float size = chair_text(app);
    draw_panel(app, area);
    Rectangle grid = { area.x + 10.0f * u, area.y + 10.0f * u, area.width - 20.0f * u, area.height - 20.0f * u };

    Picker *pick = &app->speaker_pick;
    static int everyone[MAX_DELEGATES];
    const int *items = pick->matches;
    int count = pick->match_count;
    if (for_motion) {
        count = match_delegates(&session->roster, "", false, everyone, MAX_DELEGATES);
        items = everyone;
    }
    SpeakerList *target = add_target(app);
    if (session->roster.count == 0) {
        draw_text_in(app->font, "No delegations yet", (Rectangle){ grid.x, grid.y, grid.width, size * 1.6f }, size * 0.9f, t->muted, 0);
        return;
    }
    if (count == 0) {
        draw_text_in(app->font, "No match", (Rectangle){ grid.x, grid.y, grid.width, size * 1.6f }, size * 0.85f, t->muted, 0);
        return;
    }

    float gap = 6.0f * u;
    // a column for every 140 px or so, from two to six
    int columns = (int)((grid.width + gap) / (140.0f * u + gap));
    if (columns < 2) columns = 2;
    if (columns > 6) columns = 6;
    float tileW = (grid.width - (columns - 1) * gap) / columns;
    float tileH = size * 1.75f;
    int rows = (count + columns - 1) / columns;
    float contentH = rows * (tileH + gap);
    if (hovered(grid) && GetMouseWheelMove() != 0.0f) app->grid_scroll -= GetMouseWheelMove() * tileH;
    float maxScroll = contentH - grid.height;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    bool typing = is_focused(app, pick->text) && pick->text[0] != '\0';
    if (typing) {
        float selY = (pick->selected / columns) * (tileH + gap);
        if (selY < app->grid_scroll) app->grid_scroll = selY;
        if (selY + tileH > app->grid_scroll + grid.height) app->grid_scroll = selY + tileH - grid.height;
    }
    if (app->grid_scroll > maxScroll) app->grid_scroll = maxScroll;
    if (app->grid_scroll < 0.0f) app->grid_scroll = 0.0f;

    BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
    bool inside = hovered(grid);
    for (int i = 0; i < count; i++) {
        Rectangle tile = { grid.x + (i % columns) * (tileW + gap), grid.y + (i / columns) * (tileH + gap) - app->grid_scroll, tileW, tileH };
        if (tile.y + tile.height < grid.y || tile.y > grid.y + grid.height) continue;
        const Delegate *delegate = &session->roster.items[items[i]];
        bool waiting = !for_motion && is_waiting(target, delegate->name);
        bool absent = session->roster.roll_taken && delegate->attendance == ABSENT;
        bool selected = typing && !for_motion && i == pick->selected;
        bool over = inside && CheckCollisionPointRec(GetMousePosition(), tile);

        DrawRectangleRounded(tile, 0.3f, 6, waiting ? t->panel : (over ? t->highlight : t->raised));
        DrawRectangleRoundedLinesEx(tile, 0.3f, 6, selected ? 2.0f * u : 1.0f, selected ? t->accent : t->line);
        Color nameColor = waiting || absent ? t->done : t->text;
        draw_text_in(app->font, delegate->name, (Rectangle){ tile.x + 8.0f * u, tile.y, tile.width - 16.0f * u, tile.height }, size * 0.86f, nameColor, 0);

        if (over && clicked(tile)) {
            if (for_motion) set_motion_proposer(app, delegate->name);
            else if (waiting) toast(app, "%s is already on the list", delegate->name);
            else act_add_speaker(app, target, delegate->name);
        }
    }
    EndScissorMode();
}

// several names at once ("fra, ger, bra"), each resolved to the roster; Shift+Enter puts them next
static void add_typed(App *app, SpeakerList *target, const char *typed, bool next) {
    char copy[NAME_SIZE];
    copy_text(copy, sizeof(copy), typed);
    char *part = copy;
    while (part != NULL) {
        char *comma = strchr(part, ',');
        if (comma != NULL) *comma = '\0';
        char name[NAME_SIZE];
        copy_text(name, sizeof(name), part);
        trim_text(name);
        if (name[0] != '\0') {
            // the same best match the tiles would highlight for that text, present delegations first
            int best[1];
            const Roster *roster = &app->session.roster;
            int found = match_delegates(roster, name, roster->roll_taken, best, 1);
            if (found == 0 && roster->roll_taken) found = match_delegates(roster, name, false, best, 1);
            if (found > 0) copy_text(name, sizeof(name), roster->items[best[0]].name);
            if (next) act_insert_next(app, target, name);
            else act_add_speaker(app, target, name);
        }
        part = comma != NULL ? comma + 1 : NULL;
    }
}

void draw_speakers_panel(App *app, Rectangle area) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    float size = chair_text(app);
    SpeakerList *target = add_target(app);
    draw_panel(app, area);
    float pad = 10.0f * u;
    Rectangle inner = { area.x + pad, area.y + pad * 0.7f, area.width - 2 * pad, area.height - pad * 1.4f };

    // heading: the list the box feeds (G turns a caucus's box to the speakers' list), and Hide (also Esc)
    float headH = size * 1.6f;
    gui_size((int)(size * 0.85f));
    float hideW = text_width(app->font, "Hide", size * 0.85f) + 26.0f * u;
    if (button(app, (Rectangle){ inner.x + inner.width - hideW, inner.y + headH * 0.1f, hideW, headH * 0.8f }, "Hide", BUTTON_QUIET)) {
        app->show_speakers = false;
    }
    draw_text_in(app->bold, list_title(session, target), (Rectangle){ inner.x, inner.y, inner.width - hideW - 8.0f * u, headH }, size, t->text, 0);

    // the box: Enter adds the highlighted match and stays open for the next name
    Rectangle box = { inner.x, inner.y + headH + 4.0f * u, inner.width, size * 2.0f };
    Picker *pick = &app->speaker_pick;
    pick->present_only = session->roster.roll_taken;
    gui_size((int)size);
    if (picker_field(app, pick, box, "Add speaker")) {
        bool next = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (strchr(pick->text, ',') != NULL) add_typed(app, target, pick->text, next);
        else {
            const char *name = picker_choice(app, pick);
            if (name != NULL) {
                char chosen[NAME_SIZE];
                copy_text(chosen, sizeof(chosen), name);
                if (next) act_insert_next(app, target, chosen);
                else act_add_speaker(app, target, chosen);
            }
        }
        clear_picker(pick);
        focus_field(app, pick->text);
    }
    update_picker(app, pick);

    Rectangle rows = { area.x + 4.0f * u, box.y + box.height + 6.0f * u, area.width - 8.0f * u, 0 };
    rows.height = area.y + area.height - rows.y - 4.0f * u;
    draw_speaker_rows(app, rows, false, target);
}
