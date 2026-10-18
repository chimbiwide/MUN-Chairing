#include <views.h>
#include <raygui.h>
#include <stdio.h>
#include <string.h>

static int quorum_of(const Session *session) {
    int members = session->roster.count;
    return session->settings.quarter_quorum ? (members + 3) / 4 : one_third(members);
}

// the delegates a vote counts, in roster order
static int voters(const Session *session, int *out) {
    int count = 0;
    bool roll = session->roster.roll_taken;
    for (int i = 0; i < session->roster.count; i++) {
        Attendance attendance = session->roster.items[i].attendance;
        if (!roll || attendance != ABSENT) out[count++] = i;
    }
    return count;
}

// rows of names with a toggle group each; returns the row clicked, -1 for none
static int toggle_rows(App *app, Rectangle area, const int *items, int count, int cursor, float *scroll,
                       const char *options, int (*get)(const Delegate *), void (*set)(App *, int, int)) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    float size = chair_text(app);
    float rowH = size * 1.9f;
    DrawRectangleRec(area, t->raised);

    // keep the cursor in view
    int cursorRow = -1;
    for (int i = 0; i < count; i++) {
        if (items[i] == cursor) cursorRow = i;
    }
    if (hovered(area) && GetMouseWheelMove() != 0.0f) *scroll -= GetMouseWheelMove() * rowH * 2.0f;
    else if (cursorRow >= 0) {
        float top = cursorRow * rowH;
        if (top < *scroll) *scroll = top;
        if (top + rowH > *scroll + area.height) *scroll = top + rowH - area.height;
    }
    float maxScroll = count * rowH - area.height;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    if (*scroll > maxScroll) *scroll = maxScroll;
    if (*scroll < 0.0f) *scroll = 0.0f;

    int picked = -1;
    float toggleW = area.width * 0.17f;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    bool wasLocked = GuiIsLocked();
    for (int i = 0; i < count; i++) {
        Rectangle row = { area.x, area.y + i * rowH - *scroll, area.width, rowH };
        if (row.y + row.height < area.y || row.y > area.y + area.height) continue;
        const Delegate *delegate = &session->roster.items[items[i]];
        if (items[i] == cursor) DrawRectangleRec(row, t->highlight);
        draw_text_in(app->font, delegate->name, (Rectangle){ row.x + 12.0f * u, row.y, row.width - 3 * toggleW - 28.0f * u, row.height }, size * 0.92f, t->text, 0);

        // a toggle half hidden by the scroll must not take clicks meant for what is outside the list
        bool visible = row.y >= area.y && row.y + row.height <= area.y + area.height;
        if (!visible && !wasLocked) GuiLock();
        int value = get(delegate);
        int before = value;
        gui_size((int)(size * 0.8f));
        GuiToggleGroup((Rectangle){ row.x + row.width - 3 * toggleW - 8.0f * u, row.y + 4.0f, toggleW - 3.0f, row.height - 8.0f }, options, &value);
        if (!visible && !wasLocked) GuiUnlock();
        if (value != before) set(app, items[i], value);
        Rectangle nameArea = { row.x, row.y, row.width - 3 * toggleW - 12.0f * u, row.height };
        if (visible && clicked(nameArea)) picked = items[i];
    }
    EndScissorMode();
    return picked;
}

static int get_attendance(const Delegate *delegate) {
    return delegate->attendance == PRESENT_VOTING ? 2 : delegate->attendance == PRESENT ? 1 : 0;
}

static void set_attendance_value(App *app, int index, int value) {
    set_attendance(&app->session, index, value == 2 ? PRESENT_VOTING : value == 1 ? PRESENT : ABSENT);
    invalidate_pickers(app);
    touch(app);
}

static void mark(App *app, Attendance attendance) {
    Session *session = &app->session;
    if (app->roll_cursor < 0 || app->roll_cursor >= session->roster.count) return;
    set_attendance(session, app->roll_cursor, attendance);
    invalidate_pickers(app);
    touch(app);
    if (app->roll_cursor < session->roster.count - 1) app->roll_cursor++;
}

void draw_rollcall(App *app) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Roll call", 1040.0f * u, 620.0f * u, &closed);
    float size = chair_text(app);
    if (session->roster.count == 0) {
        draw_text_in(app->font, "Add delegations in Setup first.", (Rectangle){ body.x, body.y, body.width, size * 2.0f }, size, t->muted, 0);
        if (closed) open_overlay(app, OVERLAY_NONE);
        return;
    }
    if (app->roll_cursor >= session->roster.count) app->roll_cursor = session->roster.count - 1;
    if (app->roll_cursor < 0) app->roll_cursor = 0;

    bool done = false;
    if (!app->typing && app->overlay_fresh == false) {
        if (IsKeyPressed(KEY_P)) mark(app, PRESENT);
        if (IsKeyPressed(KEY_V)) mark(app, PRESENT_VOTING);
        if (IsKeyPressed(KEY_A)) mark(app, ABSENT);
        if (IsKeyPressed(KEY_DOWN) && app->roll_cursor < session->roster.count - 1) app->roll_cursor++;
        if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_BACKSPACE)) && app->roll_cursor > 0) app->roll_cursor--;
        if (IsKeyPressed(KEY_ENTER)) done = true;
    }

    float leftW = body.width * 0.40f;
    Rectangle left = { body.x, body.y, leftW - 20.0f * u, body.height };
    Rectangle right = { body.x + leftW, body.y, body.width - leftW, body.height };

    draw_text_in(app->font, "Now calling", (Rectangle){ left.x, left.y, left.width, size * 1.3f }, size * 0.85f, t->muted, 0);
    const char *name = session->roster.items[app->roll_cursor].name;
    draw_text_in(app->bold, name, (Rectangle){ left.x, left.y + size * 1.4f, left.width, size * 3.0f }, fit_text(app->bold, name, size * 2.4f, left.width), t->text, 0);

    float buttonH = size * 2.3f;
    float y = left.y + size * 4.8f;
    gui_size((int)size);
    if (button(app, (Rectangle){ left.x, y, left.width, buttonH }, "Present", BUTTON_PRIMARY)) mark(app, PRESENT);
    y += buttonH + 8.0f * u;
    if (button(app, (Rectangle){ left.x, y, left.width, buttonH }, "Present and voting", BUTTON_PRIMARY)) mark(app, PRESENT_VOTING);
    y += buttonH + 8.0f * u;
    if (button(app, (Rectangle){ left.x, y, left.width, buttonH }, "Absent", BUTTON_PLAIN)) mark(app, ABSENT);
    y += buttonH + 14.0f * u;

    int present = count_attendance(&session->roster, PRESENT);
    int voting = count_attendance(&session->roster, PRESENT_VOTING);
    int total = present + voting;
    int members = session->roster.count;
    int quorum = quorum_of(session);
    char line[128];
    snprintf(line, sizeof(line), "%d present (%d present and voting) · %d absent", total, voting, members - total);
    draw_text_in(app->font, line, (Rectangle){ left.x, y, left.width, size * 1.4f }, size * 0.85f, t->text, 0);
    y += size * 1.5f;
    snprintf(line, sizeof(line), "Quorum %d (%s) · %s", quorum, session->settings.quarter_quorum ? "a quarter" : "a third", total >= quorum ? "met" : "NOT MET");
    draw_text_in(app->font, line, (Rectangle){ left.x, y, left.width, size * 1.4f }, size * 0.85f, total >= quorum ? t->running : t->expired, 0);
    y += size * 1.5f;
    snprintf(line, sizeof(line), "Simple majority %d · two-thirds %d", simple_majority(total), two_thirds(total));
    draw_text_in(app->font, line, (Rectangle){ left.x, y, left.width, size * 1.4f }, size * 0.85f, t->text, 0);

    float half = (left.width - 8.0f * u) / 2.0f;
    Rectangle bottom = { left.x, left.y + left.height - size * 2.0f, half, size * 2.0f };
    gui_size((int)(size * 0.9f));
    if (button(app, bottom, is_armed(app, ARM_RESET_ROLL) ? "Click again" : "Everyone absent", BUTTON_DANGER)) {
        if (confirm_armed(app, ARM_RESET_ROLL)) {
            for (int i = 0; i < session->roster.count; i++) session->roster.items[i].attendance = ABSENT;
            app->roll_cursor = 0;
            invalidate_pickers(app);
            touch(app);
        }
    }
    bottom.x += half + 8.0f * u;
    if (button(app, bottom, "Done", BUTTON_GOOD)) done = true;

    static int items[MAX_DELEGATES];
    for (int i = 0; i < session->roster.count; i++) items[i] = i;
    int picked = toggle_rows(app, right, items, session->roster.count, app->roll_cursor, &app->roll_scroll,
                             "Absent;Present;Voting", get_attendance, set_attendance_value);
    if (picked >= 0) app->roll_cursor = picked;

    if (done) {
        session->roster.roll_taken = true;
        finish_roll_call(session);
        touch(app);
        invalidate_pickers(app);
        open_overlay(app, OVERLAY_NONE);
        toast(app, "Roll call recorded: %d present, majority %d", total, simple_majority(total));
    }
    else if (closed) open_overlay(app, OVERLAY_NONE);
}

static int get_ballot(const Delegate *delegate) {
    return delegate->ballot == BALLOT_YES ? 0 : delegate->ballot == BALLOT_NO ? 1 : delegate->ballot == BALLOT_ABSTAIN ? 2 : -1;
}

static void set_ballot_value(App *app, int index, int value) {
    Delegate *delegate = &app->session.roster.items[index];
    if (value == 2 && delegate->attendance == PRESENT_VOTING) {
        toast(app, "%s is present and voting and may not abstain", delegate->name);
        return;
    }
    delegate->ballot = value == 0 ? BALLOT_YES : value == 1 ? BALLOT_NO : BALLOT_ABSTAIN;
    touch(app);
}

static void cast(App *app, const int *items, int count, Ballot ballot) {
    int at = -1;
    for (int i = 0; i < count; i++) {
        if (items[i] == app->vote_cursor) at = i;
    }
    if (at < 0) return;
    Delegate *delegate = &app->session.roster.items[app->vote_cursor];
    if (ballot == BALLOT_ABSTAIN && delegate->attendance == PRESENT_VOTING) {
        toast(app, "%s is present and voting and may not abstain", delegate->name);
        return;
    }
    delegate->ballot = ballot;
    touch(app);
    if (at < count - 1) app->vote_cursor = items[at + 1];
}

void draw_vote(App *app) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Substantive vote", 1040.0f * u, 640.0f * u, &closed);
    float size = chair_text(app);
    static int items[MAX_DELEGATES];
    int count = voters(session, items);
    if (count == 0) {
        draw_text_in(app->font, "Nobody is present to vote. Take the roll call first.", (Rectangle){ body.x, body.y, body.width, size * 2.0f }, size, t->muted, 0);
        if (closed) open_overlay(app, OVERLAY_NONE);
        return;
    }
    bool listed = false;
    for (int i = 0; i < count; i++) {
        if (items[i] == app->vote_cursor) listed = true;
    }
    if (!listed) app->vote_cursor = items[0];

    if (!app->typing && !app->overlay_fresh) {
        if (IsKeyPressed(KEY_Y)) cast(app, items, count, BALLOT_YES);
        if (IsKeyPressed(KEY_N)) cast(app, items, count, BALLOT_NO);
        if (IsKeyPressed(KEY_A)) cast(app, items, count, BALLOT_ABSTAIN);
        for (int i = 0; i < count; i++) {
            if (items[i] != app->vote_cursor) continue;
            if (IsKeyPressed(KEY_DOWN) && i < count - 1) app->vote_cursor = items[i + 1];
            if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_BACKSPACE)) && i > 0) app->vote_cursor = items[i - 1];
            break;
        }
    }

    float leftW = body.width * 0.40f;
    Rectangle left = { body.x, body.y, leftW - 20.0f * u, body.height };
    Rectangle right = { body.x + leftW, body.y, body.width - leftW, body.height };

    draw_text_in(app->font, "Vote on", (Rectangle){ left.x, left.y, left.width, size * 1.3f }, size * 0.85f, t->muted, 0);
    gui_size((int)size);
    text_field(app, (Rectangle){ left.x, left.y + size * 1.35f, left.width, size * 2.0f }, app->vote_title, sizeof(app->vote_title), "Draft Resolution 1.1");

    float y = left.y + size * 4.0f;
    const char *name = session->roster.items[app->vote_cursor].name;
    draw_text_in(app->bold, name, (Rectangle){ left.x, y, left.width, size * 2.4f }, fit_text(app->bold, name, size * 1.8f, left.width), t->text, 0);
    y += size * 2.6f;
    float third = (left.width - 16.0f * u) / 3.0f;
    float buttonH = size * 2.2f;
    gui_size((int)size);
    if (button(app, (Rectangle){ left.x, y, third, buttonH }, "Yes", BUTTON_GOOD)) cast(app, items, count, BALLOT_YES);
    if (button(app, (Rectangle){ left.x + third + 8.0f * u, y, third, buttonH }, "No", BUTTON_DANGER)) cast(app, items, count, BALLOT_NO);
    if (button(app, (Rectangle){ left.x + 2 * (third + 8.0f * u), y, third, buttonH }, "Abstain", BUTTON_PLAIN)) cast(app, items, count, BALLOT_ABSTAIN);
    y += buttonH + 16.0f * u;

    Tally tally = count_ballots(&session->roster);
    bool passes = substantive_passes(session, tally);
    char line[160];
    snprintf(line, sizeof(line), "Yes %d · No %d · Abstain %d · not voted %d", tally.yes, tally.no, tally.abstain, tally.missing);
    draw_text_in(app->font, line, (Rectangle){ left.x, y, left.width, size * 1.4f }, size * 0.9f, t->text, 0);
    y += size * 1.5f;
    const char *rule = session->settings.security_council ? "Needs 9 yes and no veto from a permanent member" : "Needs more yes than no; abstentions don't count";
    draw_text_in(app->font, rule, (Rectangle){ left.x, y, left.width, size * 1.4f }, size * 0.8f, t->muted, 0);
    y += size * 1.6f;
    const char *verdict = tally.vetoed && session->settings.security_council ? "VETOED" : (passes ? "PASSES" : "FAILS");
    draw_text_in(app->bold, verdict, (Rectangle){ left.x, y, left.width, size * 2.2f }, size * 1.8f, passes ? t->running : t->expired, 0);

    float half = (left.width - 8.0f * u) / 2.0f;
    Rectangle bottom = { left.x, left.y + left.height - size * 2.0f, half, size * 2.0f };
    gui_size((int)(size * 0.9f));
    if (button(app, bottom, "Clear ballots", BUTTON_PLAIN)) {
        clear_ballots(&session->roster);
        app->vote_cursor = items[0];
        touch(app);
    }
    bottom.x += half + 8.0f * u;
    if (button(app, bottom, "Record the vote", passes ? BUTTON_GOOD : BUTTON_DANGER)) {
        const char *what = app->vote_title[0] != '\0' ? app->vote_title : "the draft resolution";
        record_vote(session, what, tally, passes);
        touch(app);
        toast(app, "Recorded: %s %s %d-%d-%d", what, passes ? "passed" : "failed", tally.yes, tally.no, tally.abstain);
        open_overlay(app, OVERLAY_NONE);
        return;
    }

    int picked = toggle_rows(app, right, items, count, app->vote_cursor, &app->roll_scroll, "Yes;No;Abstain", get_ballot, set_ballot_value);
    if (picked >= 0) app->vote_cursor = picked;
    if (closed) open_overlay(app, OVERLAY_NONE);
}
