#include <views.h>
#include <countries.h>
#include <duration.h>
#include <raygui.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static Color badge_color(App *app, MotionType type) {
    bool dark = app->session.settings.dark;
    if (!dark) return app->theme.raised;
    switch (type) {
        case MOTION_MOD: case MOTION_ROUND_ROBIN: return dark ? (Color){ 0x39, 0xC5, 0xBB, 255 } : (Color){ 0x0B, 0x7F, 0x77, 255 };
        case MOTION_UNMOD: case MOTION_CONSULT: return dark ? (Color){ 0xA3, 0x71, 0xF7, 255 } : (Color){ 0x6D, 0x3F, 0xC8, 255 };
        case MOTION_EXTEND: return dark ? (Color){ 0x22, 0xD3, 0xEE, 255 } : (Color){ 0x05, 0x7A, 0x9C, 255 };
        case MOTION_CLOSE_DEBATE: case MOTION_TABLE: return dark ? (Color){ 0xF8, 0x71, 0x71, 255 } : (Color){ 0xB4, 0x23, 0x18, 255 };
        default: return app->theme.muted;
    }
}

static const char *badge_text(MotionType type) {
    switch (type) {
        case MOTION_MOD: return "MOD";
        case MOTION_UNMOD: return "UNMOD";
        case MOTION_CONSULT: return "CONSULT";
        case MOTION_ROUND_ROBIN: return "RR";
        case MOTION_EXTEND: return "EXT";
        case MOTION_SPEAKING_TIME: return "TIME";
        case MOTION_INTRODUCE: return "INTRO";
        case MOTION_TABLE: return "TABLE";
        case MOTION_CLOSE_DEBATE: return "CLOSE";
        case MOTION_SUSPEND: return "SUSP";
        case MOTION_ADJOURN: return "ADJ";
        default: return "OTHER";
    }
}

// "9:45 (12) · France · Accountability", what a row shows after the badge
static void motion_details(const Motion *motion, char *buffer, int size) {
    char length[48];
    length[0] = '\0';
    Duration duration = { motion->total, motion->speaker };
    if (motion->type == MOTION_MOD) format_duration(duration, length, sizeof(length));
    else if (motion_uses_total(motion->type)) format_span(motion->total, length, sizeof(length));
    else if (motion_uses_speaker(motion->type)) {
        char face[16];
        format_clock(motion->speaker, face, sizeof(face));
        snprintf(length, sizeof(length), "%s%s", face, motion->type == MOTION_ROUND_ROBIN ? " each" : "");
    }
    const char *parts[3] = { length, motion->proposer, motion->topic };
    size_t used = 0;
    buffer[0] = '\0';
    for (int i = 0; i < 3; i++) {
        if (parts[i][0] == '\0') continue;
        int wrote = snprintf(buffer + used, (size_t)size - used, "%s%s", used > 0 ? " · " : "", parts[i]);
        if (wrote < 0 || used + (size_t)wrote >= (size_t)size) break;
        used += (size_t)wrote;
    }
}

// a caucus shows itself; anything else passed is easy to miss
static void after_pass(App *app, const Motion *motion) {
    if (motion_starts_caucus(motion->type)) {
        app->add_to_gsl = false;
        app->list_scroll = 0.0f;
        return;
    }
    char what[TOPIC_SIZE + NAME_SIZE + 48];
    describe_motion(motion, what, sizeof(what));
    toast(app, "Passed: %s", what);
}

static void vote_on(App *app, int index, int yes, int no, bool passed) {
    Session *session = &app->session;
    if (index < 0 || index >= session->motions.count) return;
    Motion motion = session->motions.items[index];
    if (passed) {
        before(app, "pass a motion");
        if (!pass_motion(session, index, yes, no, app->now)) {
            forget_last(&app->history);
            toast(app, motion.type == MOTION_EXTEND ? "There is no caucus to extend" : "That motion can't be applied now");
            return;
        }
        after_pass(app, &motion);
    }
    else {
        before(app, "fail a motion");
        fail_motion(session, index, yes, no);
    }
}

// how the line reads, so a mod and an unmod can be told apart before Enter; nothing until it reads
static void echo_line(App *app, const char *line, Rectangle bounds) {
    if (asks_for_speakers_list(line)) {
        draw_text_in(app->bold, "Open the speakers' list", bounds, chair_text(app) * 0.9f, app->theme.text, 0);
        return;
    }
    Motion parsed;
    char error[ERROR_SIZE];
    if (!parse_motion_line(line, &app->session.roster, &parsed, error, sizeof(error))) return;
    char details[TOPIC_SIZE + NAME_SIZE + 48];
    motion_details(&parsed, details, sizeof(details));
    char shown[TOPIC_SIZE + NAME_SIZE + 96];
    snprintf(shown, sizeof(shown), "%s%s%s", motion_name(parsed.type), details[0] != '\0' ? " · " : "", details);
    draw_text_in(app->bold, shown, bounds, chair_text(app) * 0.9f, app->theme.text, 0);
}

void set_motion_proposer(App *app, const char *name) {
    Motion parsed;
    char error[ERROR_SIZE];
    if (parse_motion_line(app->motion_quick, &app->session.roster, &parsed, error, sizeof(error)) && parsed.proposer[0] != '\0') {
        toast(app, "This motion already has a proposer: %s", parsed.proposer);
    }
    else {
        char line[ENTRY_SIZE + NAME_SIZE + 8];
        snprintf(line, sizeof(line), "%s%sby %s ", app->motion_quick,
                 app->motion_quick[0] != '\0' && app->motion_quick[strlen(app->motion_quick) - 1] != ' ' ? " " : "", name);
        copy_text(app->motion_quick, sizeof(app->motion_quick), line);
    }
    focus_field(app, app->motion_quick);
}

static void submit_motion(App *app) {
    Session *session = &app->session;
    // no motion: the speakers' list comes out beside the motions, ready for names
    if (asks_for_speakers_list(app->motion_quick)) {
        app->motion_quick[0] = '\0';
        open_speakers_list(app);
        return;
    }
    Motion parsed;
    char error[ERROR_SIZE];
    if (!parse_motion_line(app->motion_quick, &session->roster, &parsed, error, sizeof(error))) {
        if (app->motion_quick[0] != '\0') toast(app, "%s", error);
        focus_field(app, app->motion_quick);
        return;
    }
    before(app, "add a motion");
    int index = propose_motion(session, &parsed, error, sizeof(error));
    if (index < 0) {
        forget_last(&app->history);
        toast(app, "%s", error[0] != '\0' ? error : "The motion list is full");
    }
    else app->motion_quick[0] = '\0';
    focus_field(app, app->motion_quick);
}

void draw_motions_panel(App *app, Rectangle area) {
    Session *session = &app->session;
    const Theme *t = &app->theme;
    float u = app->u;
    float size = chair_text(app);
    MotionList *motions = &session->motions;
    draw_panel(app, area);
    float pad = 10.0f * u;
    Rectangle inner = { area.x + pad, area.y + pad * 0.7f, area.width - 2 * pad, area.height - pad * 1.4f };

    float headH = size * 1.6f;
    draw_text_in(app->bold, "Motions", (Rectangle){ inner.x, inner.y, inner.width, headH }, size, t->text, 0);
    float y = inner.y + headH + 2.0f * u;

    // the line decides the kind: "unmod" in it makes an unmod, anything else is a mod
    gui_size((int)size);
    Rectangle line = { inner.x, y, inner.width, size * 2.0f };
    if (text_field(app, line, app->motion_quick, sizeof(app->motion_quick), "Motion")) submit_motion(app);
    y += line.height + 4.0f * u;
    if (app->motion_quick[0] != '\0') {
        echo_line(app, app->motion_quick, (Rectangle){ inner.x, y, inner.width, size * 1.4f });
        y += size * 1.4f + 4.0f * u;
    }

    Rectangle list = { inner.x, y, inner.width, inner.y + inner.height - y };
    if (motions->count == 0) return;
    float rowH = size * 1.85f;
    float contentH = motions->count * rowH;
    if (hovered(list) && GetMouseWheelMove() != 0.0f) app->motion_scroll -= GetMouseWheelMove() * rowH;
    float maxScroll = contentH - list.height;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    if (app->motion_scroll > maxScroll) app->motion_scroll = maxScroll;
    if (app->motion_scroll < 0.0f) app->motion_scroll = 0.0f;

    // voting goes top down, so only the first motion has Pass and Fail
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    bool inside = hovered(list);
    int vote = 0;
    for (int i = 0; i < motions->count; i++) {
        Rectangle row = { list.x, list.y + i * rowH - app->motion_scroll, list.width, rowH - 3.0f * u };
        if (row.y + row.height < list.y || row.y > list.y + list.height) continue;
        const Motion *motion = &motions->items[i];
        bool top = i == 0;
        DrawRectangleRounded(row, 0.25f, 6, top ? t->highlight : t->raised);

        char number[12];
        snprintf(number, sizeof(number), "%d", i + 1);
        draw_text_in(app->font, number, (Rectangle){ row.x + 4.0f * u, row.y, 18.0f * u, row.height }, size * 0.8f, t->muted, 1);
        float badgeSize = size * 0.68f;
        float badgeW = text_width(app->bold, "CONSULT", badgeSize) + 10.0f * u;
        Rectangle badgeRect = { row.x + 24.0f * u, row.y + row.height * 0.2f, badgeW, row.height * 0.6f };
        DrawRectangleRounded(badgeRect, 0.4f, 6, badge_color(app, motion->type));
        draw_text_in(app->bold, badge_text(motion->type), badgeRect, badgeSize, session->settings.dark ? t->on_accent : t->text, 1);

        bool over = inside && CheckCollisionPointRec(GetMousePosition(), row);
        float buttonW = row.height * 1.55f;
        float buttonsX = row.x + row.width - (top ? 2 * (buttonW + 4.0f * u) : 0.0f);
        float toolsW = over ? 3 * (row.height - 6.0f) + 8.0f * u : 0.0f;
        char details[TOPIC_SIZE + NAME_SIZE + 48];
        motion_details(motion, details, sizeof(details));
        float textX = badgeRect.x + badgeW + 8.0f * u;
        draw_text_in(app->font, details, (Rectangle){ textX, row.y, buttonsX - toolsW - textX - 6.0f * u, row.height }, size * 0.85f, t->text, 0);

        gui_size((int)(size * 0.8f));
        if (top) {
            if (button(app, (Rectangle){ buttonsX, row.y + 3.0f, buttonW, row.height - 6.0f }, "Pass", BUTTON_GOOD)) vote = 1;
            if (button(app, (Rectangle){ buttonsX + buttonW + 4.0f * u, row.y + 3.0f, buttonW, row.height - 6.0f }, "Fail", BUTTON_DANGER)) vote = -1;
        }
        if (over) {
            float b = row.height - 8.0f;
            float bx = buttonsX - 4.0f * u - b;
            if (button(app, (Rectangle){ bx, row.y + 4.0f, b, b }, GuiIconText(ICON_BIN, NULL), BUTTON_PLAIN)) {
                before(app, "withdraw a motion");
                withdraw_motion(session, i);
                break;
            }
            bx -= b + 2.0f;
            if (button(app, (Rectangle){ bx, row.y + 4.0f, b, b }, GuiIconText(ICON_ARROW_DOWN_FILL, NULL), BUTTON_PLAIN)) {
                before(app, "reorder motions");
                if (!move_motion(motions, i, 1)) forget_last(&app->history);
                break;
            }
            bx -= b + 2.0f;
            if (button(app, (Rectangle){ bx, row.y + 4.0f, b, b }, GuiIconText(ICON_ARROW_UP_FILL, NULL), BUTTON_PLAIN)) {
                before(app, "reorder motions");
                if (!move_motion(motions, i, -1)) forget_last(&app->history);
                break;
            }
        }
    }
    EndScissorMode();
    if (vote != 0) vote_on(app, 0, -1, -1, vote > 0);
}
