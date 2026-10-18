#include <views.h>
#include <raygui.h>
#include <stdio.h>
#include <time.h>

// the projector view: the stage fills the window inside a safe margin, the chair keeps every hotkey
void draw_present(App *app) {
    if (app->blank) {
        ClearBackground(BLACK);
        return;
    }
    const Theme *t = &app->theme;
    Session *session = &app->session;
    float width = (float)GetScreenWidth();
    float height = (float)GetScreenHeight();
    float margin = height * 0.035f;

    // header: committee and topic on the left, the time of day on the right
    float headH = height * 0.075f;
    char clockText[16] = "";
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local != NULL) strftime(clockText, sizeof(clockText), "%H:%M", local);
    float headSize = headH * 0.42f;
    float clockW = text_width(app->bold, clockText, headSize * 1.2f) + 10.0f;
    draw_text_in(app->bold, clockText, (Rectangle){ width - margin - clockW, margin, clockW, headH }, headSize * 1.2f, t->muted, 2);
    const char *committee = session->committee[0] != '\0' ? session->committee : "Model United Nations";
    char head[TITLE_SIZE + TOPIC_SIZE + 8];
    if (session->agenda[0] != '\0') snprintf(head, sizeof(head), "%s  ·  %s", committee, session->agenda);
    else snprintf(head, sizeof(head), "%s", committee);
    draw_text_in(app->bold, head, (Rectangle){ margin, margin, width - 2 * margin - clockW - 20.0f, headH }, headSize, t->muted, 0);

    Rectangle stage = { margin * 0.5f, margin + headH, width - margin, height - 1.5f * margin - headH };
    draw_stage(app, stage, true);

    // moving the mouse shows a strip with the chair's main buttons for a few seconds
    if (app->now - app->mouse_moved_at < 3.0 && app->overlay == OVERLAY_NONE) {
        float u = app->u;
        float stripH = 64.0f * u;
        Rectangle strip = { width * 0.2f, height - stripH - 18.0f * u, width * 0.6f, stripH };
        DrawRectangleRounded(strip, 0.3f, 8, Fade(t->panel, 0.94f));
        DrawRectangleRoundedLinesEx(strip, 0.3f, 8, 1.0f, t->line);
        float gap = 10.0f * u;
        float w = (strip.width - 4 * gap) / 3.0f;
        Rectangle b = { strip.x + gap, strip.y + gap, w, strip.height - 2 * gap };
        gui_size((int)chair_text(app));
        const Clock *clock = session->caucus.active && !caucus_has_speakers(session->caucus.motion.type) ? &session->caucus.total : floor_clock(session);
        bool running = clock != NULL && clock->running;
        if (button(app, b, running ? "Pause" : "Start", running ? BUTTON_PLAIN : BUTTON_GOOD)) act_toggle(app);
        b.x += w + gap;
        if (button(app, b, "Next", BUTTON_PLAIN)) act_next(app);
        b.x += w + gap;
        if (button(app, b, "Leave", BUTTON_QUIET)) app->present = false;
    }
}
