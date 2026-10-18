#include <views.h>
#include <countries.h>
#include <duration.h>
#include <raygui.h>
#include <stdio.h>
#include <string.h>

static void apply_speaking_time(App *app) {
    Session *session = &app->session;
    int seconds = 0;
    char error[ERROR_SIZE];
    if (!parse_speaking(app->gsl_time_edit, &seconds, error, sizeof(error))) {
        toast(app, "%s", error);
        format_clock(session->settings.gsl_time, app->gsl_time_edit, sizeof(app->gsl_time_edit));
        return;
    }
    session->settings.gsl_time = seconds;
    // a speaker who has not started yet gets the new time
    if (!session->caucus.active && clock_elapsed(&session->gsl_clock, app->now) <= 0.0) {
        int custom = session->gsl.current >= 0 ? session->gsl.items[session->gsl.current].limit : 0;
        if (custom == 0) set_clock(&session->gsl_clock, seconds);
    }
    format_clock(seconds, app->gsl_time_edit, sizeof(app->gsl_time_edit));
    touch(app);
}

// a labelled one-line field that applies its text to target as it is typed
static void live_field(App *app, Rectangle bounds, const char *label, char *edit, int size, char *target, int targetSize, const char *placeholder) {
    float textSize = chair_text(app);
    draw_text_in(app->font, label, (Rectangle){ bounds.x, bounds.y, bounds.width, textSize * 1.3f }, textSize * 0.85f, app->theme.muted, 0);
    Rectangle box = { bounds.x, bounds.y + textSize * 1.35f, bounds.width, textSize * 2.0f };
    gui_size((int)textSize);
    text_field(app, box, edit, size, placeholder);
    if (strcmp(edit, target) != 0) {
        copy_text(target, targetSize, edit);
        flatten_text(target);
        touch(app);
    }
}

static void add_country(App *app, const char *name) {
    Session *session = &app->session;
    int result = add_delegate(&session->roster, name);
    if (result == DELEGATE_DUPLICATE) toast(app, "%s is already on the roster", name);
    else if (result == DELEGATE_FULL) toast(app, "The roster is full");
    else if (result >= 0) {
        touch(app);
        invalidate_pickers(app);
    }
}

void draw_setup(App *app) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    bool closed = false;
    Rectangle body = begin_dialog(app, "Committee setup", 1080.0f * u, 620.0f * u, &closed);
    if (closed) {
        open_overlay(app, OVERLAY_NONE);
        return;
    }
    float size = chair_text(app);
    float gap = 20.0f * u;
    float leftW = body.width * 0.30f;
    float middleW = body.width * 0.36f;
    Rectangle left = { body.x, body.y, leftW, body.height };
    Rectangle middle = { left.x + leftW + gap, body.y, middleW - gap, body.height };
    Rectangle right = { middle.x + middle.width + gap, body.y, body.x + body.width - middle.x - middle.width - gap, body.height };

    // the committee
    float rowH = size * 3.6f;
    live_field(app, (Rectangle){ left.x, left.y, left.width, rowH }, "Committee", app->committee_edit, sizeof(app->committee_edit),
               session->committee, sizeof(session->committee), "e.g. DISEC");
    live_field(app, (Rectangle){ left.x, left.y + rowH, left.width, rowH }, "Topic", app->agenda_edit, sizeof(app->agenda_edit),
               session->agenda, sizeof(session->agenda), "e.g. Autonomous weapons");

    draw_text_in(app->font, "Speaking time on the speakers' list", (Rectangle){ left.x, left.y + 2 * rowH, left.width, size * 1.3f }, size * 0.85f, t->muted, 0);
    Rectangle timeBox = { left.x, left.y + 2 * rowH + size * 1.35f, left.width * 0.45f, size * 2.0f };
    gui_size((int)size);
    bool wasEditing = app->focus_before == app->gsl_time_edit;
    if (text_field(app, timeBox, app->gsl_time_edit, sizeof(app->gsl_time_edit), "1:00") || (wasEditing && !is_focused(app, app->gsl_time_edit))) {
        apply_speaking_time(app);
    }
    char span[32];
    format_span(session->settings.gsl_time, span, sizeof(span));
    draw_text_in(app->font, span, (Rectangle){ timeBox.x + timeBox.width + 10.0f * u, timeBox.y, left.width * 0.5f, timeBox.height }, size * 0.85f, t->muted, 0);

    float tipY = left.y + 3 * rowH + 10.0f * u;
    const char *tips[] = {
        "Paste a list of names (one per line) with Paste list,",
        "or drop a .txt or .csv file onto the window.",
        "Any name works: NGOs, observers, crisis characters.",
    };
    for (int i = 0; i < 3; i++) {
        draw_text_in(app->font, tips[i], (Rectangle){ left.x, tipY + i * size * 1.3f, left.width, size * 1.3f }, size * 0.78f, t->muted, 0);
    }
    gui_size((int)size);
    if (button(app, (Rectangle){ left.x, left.y + left.height - size * 2.2f, left.width, size * 2.2f }, "Take the roll call", BUTTON_PRIMARY)) {
        open_overlay(app, OVERLAY_ROLLCALL);
        return;
    }

    // add from the UN list, or any name typed
    draw_text_in(app->bold, "Add delegations", (Rectangle){ middle.x, middle.y, middle.width, size * 1.4f }, size, t->text, 0);
    Picker *pick = &app->roster_pick;
    pick->countries = true;
    Rectangle search = { middle.x, middle.y + size * 1.6f, middle.width, size * 2.0f };
    gui_size((int)size);
    if (picker_field(app, pick, search, "Search the UN members")) {
        const char *name = picker_choice(app, pick);
        if (name != NULL) {
            char chosen[NAME_SIZE];
            copy_text(chosen, sizeof(chosen), name);
            add_country(app, chosen);
        }
        clear_picker(pick);
        pick->countries = true;
        focus_field(app, pick->text);
    }
    update_picker(app, pick);

    float buttonsH = size * 1.9f;
    Rectangle results = { middle.x, search.y + search.height + 8.0f * u, middle.width, 0 };
    results.height = middle.y + middle.height - results.y - buttonsH - 8.0f * u;
    // with nothing typed the whole list shows, alphabetically
    static int all[256];
    int count = pick->match_count;
    const int *items = pick->matches;
    if (pick->text[0] == '\0') {
        for (int i = 0; i < COUNTRY_COUNT && i < 256; i++) all[i] = i;
        count = COUNTRY_COUNT;
        items = all;
    }
    float itemH = size * 1.75f;
    if (hovered(results) && GetMouseWheelMove() != 0.0f) app->search_scroll -= GetMouseWheelMove() * itemH * 2.0f;
    float maxScroll = count * itemH - results.height;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    if (app->search_scroll > maxScroll) app->search_scroll = maxScroll;
    if (app->search_scroll < 0.0f) app->search_scroll = 0.0f;
    if (pick->text[0] != '\0' && pick->match_count > 0) app->search_scroll = 0.0f;
    DrawRectangleRec(results, t->raised);
    BeginScissorMode((int)results.x, (int)results.y, (int)results.width, (int)results.height);
    bool inside = hovered(results);
    for (int i = 0; i < count; i++) {
        Rectangle row = { results.x, results.y + i * itemH - app->search_scroll, results.width, itemH };
        if (row.y + row.height < results.y || row.y > results.y + results.height) continue;
        const char *name = COUNTRIES[items[i]].name;
        bool added = find_delegate(&session->roster, name) >= 0;
        bool over = inside && CheckCollisionPointRec(GetMousePosition(), row);
        bool selected = pick->text[0] != '\0' && i == pick->selected;
        if (over || selected) DrawRectangleRec(row, t->highlight);
        draw_text_in(app->font, name, (Rectangle){ row.x + 10.0f * u, row.y, row.width - 90.0f * u, row.height }, size * 0.9f, added ? t->done : t->text, 0);
        draw_text_in(app->font, added ? "added" : (over ? "add" : ""), (Rectangle){ row.x + row.width - 80.0f * u, row.y, 70.0f * u, row.height }, size * 0.8f, added ? t->done : t->accent, 2);
        if (over && !added && clicked(row)) add_country(app, name);
    }
    EndScissorMode();

    float third = (middle.width - 2 * 8.0f * u) / 3.0f;
    Rectangle row = { middle.x, middle.y + middle.height - buttonsH, third, buttonsH };
    gui_size((int)(size * 0.9f));
    if (button(app, row, "Paste list", BUTTON_PLAIN)) {
        const char *clip = GetClipboardText();
        if (clip == NULL || clip[0] == '\0') toast(app, "The clipboard is empty");
        else {
            int added = import_delegates(&session->roster, clip);
            invalidate_pickers(app);
            touch(app);
            if (added > 0) toast(app, "Added %d delegation%s from the clipboard", added, added == 1 ? "" : "s");
            else toast(app, "No new names in the clipboard");
        }
    }
    row.x += third + 8.0f * u;
    if (button(app, row, "Add the P5", BUTTON_PLAIN)) {
        const char *p5[] = { "China", "France", "Russia", "United Kingdom", "United States" };
        for (int i = 0; i < 5; i++) {
            if (find_delegate(&session->roster, p5[i]) < 0) add_delegate(&session->roster, p5[i]);
        }
        invalidate_pickers(app);
        touch(app);
    }
    row.x += third + 8.0f * u;
    if (button(app, row, "Add as typed", BUTTON_PLAIN)) {
        char typed[NAME_SIZE];
        copy_text(typed, sizeof(typed), pick->text);
        trim_text(typed);
        if (typed[0] == '\0') toast(app, "Type a name in the search box first");
        else {
            add_country(app, typed);
            clear_picker(pick);
            pick->countries = true;
        }
    }

    // the roster
    char title[48];
    snprintf(title, sizeof(title), "Roster (%d)", session->roster.count);
    float sortW = text_width(app->font, "Sort A-Z", size * 0.85f) + 24.0f * u;
    float clearW = text_width(app->font, "Click again", size * 0.85f) + 24.0f * u;
    draw_text_in(app->bold, title, (Rectangle){ right.x, right.y, right.width - sortW - clearW - 16.0f * u, size * 1.4f }, size, t->text, 0);
    gui_size((int)(size * 0.85f));
    if (button(app, (Rectangle){ right.x + right.width - sortW - clearW - 8.0f * u, right.y, sortW, size * 1.4f }, "Sort A-Z", BUTTON_QUIET)) {
        sort_delegates(&session->roster);
        invalidate_pickers(app);
        touch(app);
    }
    if (button(app, (Rectangle){ right.x + right.width - clearW, right.y, clearW, size * 1.4f }, is_armed(app, ARM_CLEAR_ROSTER) ? "Click again" : "Clear", BUTTON_DANGER)) {
        if (confirm_armed(app, ARM_CLEAR_ROSTER)) {
            clear_roster(&session->roster);
            invalidate_pickers(app);
            touch(app);
            toast(app, "Roster cleared");
        }
    }

    Rectangle list = { right.x, right.y + size * 1.6f, right.width, right.y + right.height - right.y - size * 1.6f };
    DrawRectangleRec(list, t->raised);
    if (session->roster.count == 0) {
        draw_text_in(app->font, "No delegations yet", (Rectangle){ list.x + 10.0f * u, list.y, list.width, size * 2.0f }, size * 0.9f, t->muted, 0);
        return;
    }
    float lineH = size * 1.7f;
    if (hovered(list) && GetMouseWheelMove() != 0.0f) app->roster_scroll -= GetMouseWheelMove() * lineH * 2.0f;
    float rosterMax = session->roster.count * lineH - list.height;
    if (rosterMax < 0.0f) rosterMax = 0.0f;
    if (app->roster_scroll > rosterMax) app->roster_scroll = rosterMax;
    if (app->roster_scroll < 0.0f) app->roster_scroll = 0.0f;
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    bool inList = hovered(list);
    for (int i = 0; i < session->roster.count; i++) {
        Rectangle line = { list.x, list.y + i * lineH - app->roster_scroll, list.width, lineH };
        if (line.y + line.height < list.y || line.y > list.y + list.height) continue;
        const Delegate *delegate = &session->roster.items[i];
        bool over = inList && CheckCollisionPointRec(GetMousePosition(), line);
        if (over) DrawRectangleRec(line, t->highlight);
        char label[NAME_SIZE + 16];
        snprintf(label, sizeof(label), "%s%s", delegate->name, delegate->veto ? "  (P5)" : "");
        draw_text_in(app->font, label, (Rectangle){ line.x + 10.0f * u, line.y, line.width - lineH - 20.0f * u, line.height }, size * 0.9f, t->text, 0);
        if (over) {
            gui_size((int)(size * 0.8f));
            if (button(app, (Rectangle){ line.x + line.width - lineH, line.y + 3.0f, lineH - 6.0f, lineH - 6.0f }, GuiIconText(ICON_CROSS_SMALL, NULL), BUTTON_PLAIN)) {
                remove_delegate(&session->roster, i);
                invalidate_pickers(app);
                touch(app);
                break;
            }
        }
    }
    EndScissorMode();
}
