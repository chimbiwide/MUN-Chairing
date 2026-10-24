#include <views.h>
#include <duration.h>
#include <store.h>
#include <raygui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Rectangle begin_dialog(App *app, const char *title, float width, float height, bool *closed) {
    const Theme *t = &app->theme;
    float u = app->u;
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    if (width > screenW - 40.0f) width = screenW - 40.0f;
    if (height > screenH - 40.0f) height = screenH - 40.0f;
    Rectangle box = { (screenW - width) / 2.0f, (screenH - height) / 2.0f, width, height };
    DrawRectangleRounded(box, 12.0f * u / height, 8, t->panel);
    DrawRectangleRoundedLinesEx(box, 12.0f * u / height, 8, 1.0f, t->line);

    float size = chair_text(app);
    float barH = size * 2.6f;
    draw_text_in(app->bold, title, (Rectangle){ box.x + 22.0f * u, box.y, box.width - 120.0f * u, barH }, size * 1.15f, t->text, 0);
    gui_size((int)(size * 0.85f));
    float closeW = text_width(app->font, "Close", size * 0.85f) + 26.0f * u;
    *closed = button(app, (Rectangle){ box.x + box.width - closeW - 14.0f * u, box.y + barH * 0.2f, closeW, barH * 0.6f }, "Close", BUTTON_QUIET);
    DrawLineEx((Vector2){ box.x, box.y + barH }, (Vector2){ box.x + box.width, box.y + barH }, 1.0f, t->line);
    return (Rectangle){ box.x + 22.0f * u, box.y + barH + 16.0f * u, box.width - 44.0f * u, box.height - barH - 32.0f * u };
}

static bool check(App *app, Rectangle bounds, const char *label, bool *value) {
    float size = chair_text(app);
    gui_size((int)(size * 0.9f));
    bool before = *value;
    Rectangle box = { bounds.x, bounds.y + (bounds.height - size * 1.2f) / 2.0f, size * 1.2f, size * 1.2f };
    GuiCheckBox(box, label, value);
    if (*value != before) touch(app);
    return *value != before;
}

void draw_settings(App *app) {
    Session *session = &app->session;
    Settings *settings = &session->settings;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Settings", 960.0f * u, 450.0f * u, &closed);
    if (closed) {
        open_overlay(app, OVERLAY_NONE);
        return;
    }
    float size = chair_text(app);
    float half = (body.width - 30.0f * u) / 2.0f;
    Rectangle left = { body.x, body.y, half, body.height };
    Rectangle right = { body.x + half + 30.0f * u, body.y, half, body.height };

    // times
    float y = left.y;
    draw_text_in(app->bold, "Clocks", (Rectangle){ left.x, y, left.width, size * 1.4f }, size, t->text, 0);
    y += size * 1.7f;
    draw_text_in(app->font, "Speaking time on the speakers' list", (Rectangle){ left.x, y, left.width * 0.62f, size * 2.0f }, size * 0.88f, t->muted, 0);
    Rectangle timeBox = { left.x + left.width * 0.64f, y, left.width * 0.36f, size * 2.0f };
    gui_size((int)size);
    bool wasTime = app->focus_before == app->gsl_time_edit;
    if (text_field(app, timeBox, app->gsl_time_edit, sizeof(app->gsl_time_edit), "1:00") || (wasTime && !is_focused(app, app->gsl_time_edit))) {
        int seconds = 0;
        char error[ERROR_SIZE];
        if (parse_speaking(app->gsl_time_edit, &seconds, error, sizeof(error))) {
            settings->gsl_time = seconds;
            if (!session->caucus.active && clock_elapsed(&session->gsl_clock, app->now) <= 0.0 &&
                (session->gsl.current < 0 || session->gsl.items[session->gsl.current].limit == 0)) {
                set_clock(&session->gsl_clock, seconds);
            }
            touch(app);
        }
        else toast(app, "%s", error);
        format_clock(settings->gsl_time, app->gsl_time_edit, sizeof(app->gsl_time_edit));
    }
    y += size * 2.4f;
    draw_text_in(app->font, "Turn amber with this many seconds left", (Rectangle){ left.x, y, left.width * 0.62f, size * 2.0f }, size * 0.88f, t->muted, 0);
    Rectangle warnBox = { left.x + left.width * 0.64f, y, left.width * 0.36f, size * 2.0f };
    bool wasWarn = app->focus_before == app->warn_time_edit;
    if (text_field(app, warnBox, app->warn_time_edit, sizeof(app->warn_time_edit), "10") || (wasWarn && !is_focused(app, app->warn_time_edit))) {
        int seconds = atoi(app->warn_time_edit);
        if (seconds >= 0 && seconds <= 600) {
            settings->warn_time = seconds;
            touch(app);
        }
        snprintf(app->warn_time_edit, sizeof(app->warn_time_edit), "%d", settings->warn_time);
    }
    y += size * 2.6f;

    draw_text_in(app->bold, "Moderated caucus total", (Rectangle){ left.x, y, left.width, size * 1.4f }, size, t->text, 0);
    y += size * 1.6f;
    int mode = settings->mod_clock;
    float optionH = size * 1.8f;
    gui_size((int)(size * 0.85f));
    GuiToggleGroup((Rectangle){ left.x, y, left.width, optionH }, "Every speech costs a full slot (HMUN)\nOnly time actually spoken counts\nRuns continuously, like a wall clock", &mode);
    if (mode != (int)settings->mod_clock) {
        settings->mod_clock = (ModClock)mode;
        touch(app);
    }
    y += 3 * optionH + 2 * GuiGetStyle(TOGGLE, GROUP_PADDING) + 14.0f * u;

    float checkH = size * 1.75f;
    check(app, (Rectangle){ left.x, y, left.width, checkH }, "Start the next speaker's clock on Next", &settings->auto_start);

    // procedure and display
    y = right.y;
    draw_text_in(app->bold, "Procedure", (Rectangle){ right.x, y, right.width, size * 1.4f }, size, t->text, 0);
    y += size * 1.7f;
    check(app, (Rectangle){ right.x, y, right.width, checkH }, "The proposer speaks first in their mod", &settings->proposer_first);
    y += checkH;
    check(app, (Rectangle){ right.x, y, right.width, checkH }, "Drop the other motions when one passes", &settings->drop_motions);
    y += checkH;
    if (check(app, (Rectangle){ right.x, y, right.width, checkH }, "Keep motions in voting order", &settings->auto_sort) && settings->auto_sort) {
        sort_motions(&session->motions);
    }
    y += checkH;
    check(app, (Rectangle){ right.x, y, right.width, checkH }, "Security Council voting (9 votes, P5 veto)", &settings->security_council);
    y += checkH;
    check(app, (Rectangle){ right.x, y, right.width, checkH }, "Quorum is a quarter of members (Harvard)", &settings->quarter_quorum);
    y += checkH;
    if (check(app, (Rectangle){ right.x, y, right.width, checkH }, "Dark theme for projectors", &settings->dark)) apply_theme(app);
    y += checkH + 12.0f * u;

    draw_text_in(app->bold, "Records", (Rectangle){ right.x, y, right.width, size * 1.4f }, size, t->text, 0);
    y += size * 1.7f;
    gui_size((int)(size * 0.9f));
    float buttonH = size * 1.9f;
    if (button(app, (Rectangle){ right.x, y, right.width, buttonH }, "Export the log and speech counts", BUTTON_PLAIN)) act_export(app);
    y += buttonH + 8.0f * u;
    float halfButton = (right.width - 8.0f * u) / 2.0f;
    if (button(app, (Rectangle){ right.x, y, halfButton, buttonH }, is_armed(app, ARM_CLEAR_GSL) ? "Click again" : "Clear the speakers' list", BUTTON_DANGER)) {
        if (confirm_armed(app, ARM_CLEAR_GSL)) {
            clear_speakers(&session->gsl);
            set_clock(&session->gsl_clock, settings->gsl_time);
            add_log(&session->log, "Speakers' list cleared");
            touch(app);
        }
    }
    if (button(app, (Rectangle){ right.x + halfButton + 8.0f * u, y, halfButton, buttonH }, is_armed(app, ARM_NEW_SESSION) ? "Click again" : "New committee", BUTTON_DANGER)) {
        if (confirm_armed(app, ARM_NEW_SESSION)) {
            Settings keep = *settings;
            init_session(session);
            session->settings = keep;
            set_clock(&session->gsl_clock, keep.gsl_time);
            touch(app);
            invalidate_pickers(app);
            open_overlay(app, OVERLAY_SETUP);
            return;
        }
    }
}

void draw_help(App *app) {
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Help", 1000.0f * u, 640.0f * u, &closed);
    if (closed) {
        open_overlay(app, OVERLAY_NONE);
        return;
    }
    float size = chair_text(app) * 0.88f;
    float lineH = size * 1.48f;
    float half = (body.width - 30.0f * u) / 2.0f;

    static const char *keys[][2] = {
        { "Space", "Start or pause the speaker (Page Up on a clicker)" },
        { "N", "Next speaker, crosses out the current one (Page Down)" },
        { "R", "Restart the current speaker's clock" },
        { "Left / Right", "Take off or give 10 s (Shift: a minute)" },
        { "Y", "Yield: to the chair, questions, a delegate; right of reply" },
        { "A", "Add speakers: type, Enter (fra, ger, bra adds three)" },
        { "G", "During a caucus: add to the speakers' list instead" },
        { "M or a digit", "Type a motion; E starts an extension" },
        { "X", "End the caucus (press twice)" },
        { "C / V / L", "Roll call / substantive vote / log" },
        { "P or F5, B", "Presentation view, black screen" },
        { "F11", "Fullscreen" },
        { "Ctrl+Z", "Undo the last action" },
        { "Ctrl+S / E / Q", "Save now / export the log / quit" },
        { "Esc", "Leave a box, a dialog, presentation, the speakers' list" },
    };
    float y = body.y;
    draw_text_in(app->bold, "Keyboard", (Rectangle){ body.x, y, half, lineH }, size * 1.1f, t->text, 0);
    y += lineH * 1.2f;
    float keyW = half * 0.30f;
    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
        draw_text_in(app->bold, keys[i][0], (Rectangle){ body.x, y, keyW, lineH }, size, t->accent, 0);
        draw_text_in(app->font, keys[i][1], (Rectangle){ body.x + keyW, y, half - keyW, lineH }, size, t->text, 0);
        y += lineH;
    }

    static const char *notes[][2] = {
        { "9:45", "9 minutes in total, 45 s per speaker: 12 speakers" },
        { "10:30  5:30", "20 speakers / 10 speakers of 30 s" },
        { "10:1  10:1:30", "a small right side means minutes: 1 min, 1 min 30 s" },
        { "10:05", "a leading zero means seconds: 5 s each" },
        { "unmod 7:30", "for an unmod, 7 min 30 s" },
        { "10:45", "13 speakers; the 15 s left over is dropped" },
        { "fra 9:45 trade", "a mod by France on trade (no word: a mod)" },
        { "unmod 15 usa", "a 15 minute unmod moved by the United States" },
        { "st 1:30", "set the speaking time to 1 min 30 s" },
        { "ext 5  close", "extend the caucus by 5 min / close debate" },
        { "speakers list  gsl", "show the speakers' list beside the motions" },
    };
    float x = body.x + half + 30.0f * u;
    y = body.y;
    draw_text_in(app->bold, "Typing times and motions", (Rectangle){ x, y, half, lineH }, size * 1.1f, t->text, 0);
    y += lineH * 1.2f;
    float noteW = half * 0.36f;
    for (int i = 0; i < (int)(sizeof(notes) / sizeof(notes[0])); i++) {
        draw_text_in(app->bold, notes[i][0], (Rectangle){ x, y, noteW, lineH }, size, t->accent, 0);
        draw_text_in(app->font, notes[i][1], (Rectangle){ x + noteW, y, half - noteW, lineH }, size, t->text, 0);
        y += lineH;
    }
    y += lineH * 0.6f;
    const char *more[] = {
        "Motions are sorted into voting order: adjourn, suspend, extend,",
        "unmod, consultation, round robin, mod, speaking time, introduce,",
        "table, close debate. Longer caucuses first; equal mods: more speakers.",
        "Everything is saved as you go and restored when the app opens.",
    };
    for (int i = 0; i < 4; i++) {
        draw_text_in(app->font, more[i], (Rectangle){ x, y, half, lineH }, size * 0.92f, t->muted, 0);
        y += lineH;
    }
}

void draw_yield(App *app) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Yield and right of reply", 760.0f * u, 520.0f * u, &closed);
    if (closed) {
        open_overlay(app, OVERLAY_NONE);
        return;
    }
    float size = chair_text(app);
    SpeakerList *gsl = &session->gsl;
    bool speaking = !session->caucus.active && gsl->current >= 0;
    float y = body.y;

    if (speaking) {
        char line[NAME_SIZE + 48];
        char left[16];
        format_left(clock_left(&session->gsl_clock, app->now), left, sizeof(left));
        snprintf(line, sizeof(line), "%s has %s left", gsl->items[gsl->current].name, left);
        draw_text_in(app->bold, line, (Rectangle){ body.x, y, body.width, size * 1.6f }, size * 1.1f, t->text, 0);
        y += size * 2.0f;

        float half = (body.width - 10.0f * u) / 2.0f;
        float buttonH = size * 2.2f;
        bool chair = false;
        bool questions = false;
        if (!app->typing && !app->overlay_fresh) {
            chair = IsKeyPressed(KEY_C);
            questions = IsKeyPressed(KEY_Q);
        }
        gui_size((int)size);
        if (button(app, (Rectangle){ body.x, y, half, buttonH }, "To the chair", BUTTON_PRIMARY)) chair = true;
        if (button(app, (Rectangle){ body.x + half + 10.0f * u, y, half, buttonH }, "To questions", BUTTON_PLAIN)) questions = true;
        if (chair) {
            before(app, "yield to the chair");
            yield_to_chair(session, app->now);
            open_overlay(app, OVERLAY_NONE);
            return;
        }
        if (questions) {
            before(app, "yield to questions");
            yield_to_questions(session, app->now);
            open_overlay(app, OVERLAY_NONE);
            return;
        }
        y += buttonH + 18.0f * u;

        draw_text_in(app->font, "To another delegate", (Rectangle){ body.x, y, body.width, size * 1.4f }, size * 0.88f, t->muted, 0);
        y += size * 1.5f;
        Rectangle box = { body.x, y, body.width * 0.72f, size * 2.0f };
        gui_size((int)size);
        bool go = picker_field(app, &app->yield_pick, box, "Delegation");
        if (button(app, (Rectangle){ box.x + box.width + 10.0f * u, y, body.width * 0.28f - 10.0f * u, box.height }, "Yield", BUTTON_PLAIN)) go = true;
        if (go) {
            const char *name = picker_choice(app, &app->yield_pick);
            if (name != NULL) {
                char chosen[NAME_SIZE];
                copy_text(chosen, sizeof(chosen), roster_spelling(session, name));
                before(app, "yield to a delegate");
                if (yield_to_delegate(session, chosen, app->now)) {
                    open_overlay(app, OVERLAY_NONE);
                    return;
                }
                forget_last(&app->history);
                toast(app, "%s can't take the yield: already waiting, no time left, or this time was itself yielded", chosen);
            }
        }
        y += box.height + 24.0f * u;
    }
    else {
        draw_text_in(app->font, "Nobody has the floor",
                     (Rectangle){ body.x, y, body.width, size * 1.6f }, size * 0.9f, t->muted, 0);
        y += size * 2.4f;
    }

    draw_text_in(app->bold, "Right of reply", (Rectangle){ body.x, y, body.width, size * 1.4f }, size, t->text, 0);
    y += size * 1.6f;
    Rectangle replyBox = { body.x, y, body.width * 0.72f, size * 2.0f };
    gui_size((int)size);
    bool grant = picker_field(app, &app->reply_pick, replyBox, "Delegation");
    if (button(app, (Rectangle){ replyBox.x + replyBox.width + 10.0f * u, y, body.width * 0.28f - 10.0f * u, replyBox.height }, "Grant", BUTTON_PLAIN)) grant = true;
    if (grant) {
        const char *name = picker_choice(app, &app->reply_pick);
        if (name != NULL) {
            char chosen[NAME_SIZE];
            copy_text(chosen, sizeof(chosen), roster_spelling(session, name));
            before(app, "grant a right of reply");
            if (grant_reply(session, chosen)) {
                open_overlay(app, OVERLAY_NONE);
                toast(app, "%s speaks next with 30 s", chosen);
                return;
            }
            forget_last(&app->history);
            toast(app, "%s is already waiting to speak", chosen);
        }
    }
}

void draw_log(App *app) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Log and speech counts", 1100.0f * u, 640.0f * u, &closed);
    if (closed) {
        open_overlay(app, OVERLAY_NONE);
        return;
    }
    float size = chair_text(app) * 0.85f;
    float logW = body.width * 0.58f;
    Rectangle logArea = { body.x, body.y, logW, body.height - size * 2.6f };
    Rectangle stats = { body.x + logW + 24.0f * u, body.y, body.width - logW - 24.0f * u, body.height - size * 2.6f };

    DrawRectangleRec(logArea, t->raised);
    float lineH = size * 1.55f;
    const Log *log = &session->log;
    float contentH = log->count * lineH;
    float maxScroll = contentH - logArea.height;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    static int seen = -1;
    if (seen != log->count) {
        app->log_scroll = maxScroll;
        seen = log->count;
    }
    if (hovered(logArea) && GetMouseWheelMove() != 0.0f) app->log_scroll -= GetMouseWheelMove() * lineH * 3.0f;
    if (app->log_scroll > maxScroll) app->log_scroll = maxScroll;
    if (app->log_scroll < 0.0f) app->log_scroll = 0.0f;
    BeginScissorMode((int)logArea.x, (int)logArea.y, (int)logArea.width, (int)logArea.height);
    for (int i = 0; i < log->count; i++) {
        float y = logArea.y + i * lineH - app->log_scroll;
        if (y + lineH < logArea.y || y > logArea.y + logArea.height) continue;
        draw_text_in(app->font, log->items[i].time, (Rectangle){ logArea.x + 8.0f * u, y, 60.0f * u, lineH }, size, t->muted, 0);
        draw_text_in(app->font, log->items[i].text, (Rectangle){ logArea.x + 70.0f * u, y, logArea.width - 78.0f * u, lineH }, size, t->text, 0);
    }
    EndScissorMode();
    if (log->count == 0) draw_text_in(app->font, "Nothing has happened yet", (Rectangle){ logArea.x + 8.0f * u, logArea.y, logArea.width, lineH }, size, t->muted, 0);

    // speeches per delegation, most first, for fairness and awards
    draw_text_in(app->bold, "Delegation", (Rectangle){ stats.x, stats.y, stats.width * 0.55f, lineH }, size, t->muted, 0);
    draw_text_in(app->bold, "Speeches", (Rectangle){ stats.x + stats.width * 0.55f, stats.y, stats.width * 0.2f, lineH }, size, t->muted, 2);
    draw_text_in(app->bold, "Time", (Rectangle){ stats.x + stats.width * 0.75f, stats.y, stats.width * 0.25f, lineH }, size, t->muted, 2);
    static int order[MAX_DELEGATES];
    int count = session->roster.count;
    for (int i = 0; i < count; i++) order[i] = i;
    for (int i = 1; i < count; i++) {
        int item = order[i];
        const Delegate *d = &session->roster.items[item];
        int speeches = d->gsl_speeches + d->caucus_speeches;
        int j = i - 1;
        while (j >= 0) {
            const Delegate *e = &session->roster.items[order[j]];
            if (e->gsl_speeches + e->caucus_speeches >= speeches) break;
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = item;
    }
    Rectangle table = { stats.x, stats.y + lineH * 1.2f, stats.width, stats.height - lineH * 1.2f };
    float rowH = size * 1.45f;
    BeginScissorMode((int)table.x, (int)table.y, (int)table.width, (int)table.height);
    for (int i = 0; i < count; i++) {
        float y = table.y + i * rowH;
        if (y > table.y + table.height) break;
        const Delegate *d = &session->roster.items[order[i]];
        char speeches[24];
        char spoken[16];
        snprintf(speeches, sizeof(speeches), "%d (%d+%d)", d->gsl_speeches + d->caucus_speeches, d->gsl_speeches, d->caucus_speeches);
        format_clock((int)(d->spoken + 0.5), spoken, sizeof(spoken));
        Color color = (d->gsl_speeches + d->caucus_speeches) == 0 ? t->muted : t->text;
        draw_text_in(app->font, d->name, (Rectangle){ table.x, y, table.width * 0.55f, rowH }, size, color, 0);
        draw_text_in(app->font, speeches, (Rectangle){ table.x + table.width * 0.55f, y, table.width * 0.2f, rowH }, size, color, 2);
        draw_text_in(app->font, spoken, (Rectangle){ table.x + table.width * 0.75f, y, table.width * 0.25f, rowH }, size, color, 2);
    }
    EndScissorMode();

    gui_size((int)(size * 1.05f));
    Rectangle export = { body.x, body.y + body.height - size * 2.2f, 360.0f * u, size * 2.2f };
    if (button(app, export, "Export the log and counts", BUTTON_PRIMARY)) act_export(app);
}
