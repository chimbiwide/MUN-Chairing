#include <raylib.h>
#include <rlgl.h>
#include <raygui.h>
#include <stdio.h>
#include <string.h>

#include <assets.h>
#include <store.h>
#include <demo.h>
#include <views.h>

// the session and its undo snapshots are several megabytes, too big for the stack
static App app;

static bool clocks_running(const Session *session) {
    return session->gsl_clock.running || session->caucus.total.running || session->caucus.speaker.running;
}

static void save_now(App *app, bool force) {
    if (app->demo) return;
    bool due = app->dirty && app->now - app->saved_at > 0.75;
    bool stale = clocks_running(&app->session) && app->now - app->saved_at > 5.0;
    if (!force && !due && !stale) return;
    if (save_session(&app->session, app->session_path, app->now)) {
        app->dirty = false;
        app->save_failed = false;
    }
    else if (!app->save_failed) {
        toast(app, "Couldn't save %s", app->session_path);
        app->save_failed = true;
    }
    app->saved_at = app->now;
}

// a letter that moved the focus into a text box must not also be typed into it
static void drop_typed_keys(void) {
    while (GetCharPressed() != 0) {}
}

static bool quit_requested = false;

static void request_quit(App *app) {
    if (!clocks_running(&app->session) || confirm_armed(app, ARM_QUIT)) quit_requested = true;
    else toast(app, "A clock is running. Quit again to close (the session is saved)");
}

static void handle_keys(App *app) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
    if (ctrl && IsKeyPressed(KEY_Q)) request_quit(app);
    if (ctrl && IsKeyPressed(KEY_S)) {
        app->dirty = true;
        save_now(app, true);
        if (app->demo) toast(app, "Demo mode: nothing is saved");
        else if (!app->save_failed) toast(app, "Saved to %s", app->session_path);
    }
    if (app->typing) {
        if (IsKeyPressed(KEY_ESCAPE)) app->focus = NULL;
        // Space in an empty speaker box or motion line leaves it and runs the clock
        bool quick = app->focus == app->speaker_pick.text || app->focus == app->motion_quick;
        if (quick && app->focus[0] == '\0' && IsKeyPressed(KEY_SPACE) && app->overlay == OVERLAY_NONE) {
            app->focus = NULL;
            drop_typed_keys();
            act_toggle(app);
        }
        return;
    }

    if (ctrl && IsKeyPressed(KEY_Z)) act_undo(app);
    if (ctrl && IsKeyPressed(KEY_E)) act_export(app);
    if (ctrl) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (app->overlay != OVERLAY_NONE) open_overlay(app, OVERLAY_NONE);
        else if (app->present) {
            app->present = false;
            app->blank = false;
        }
        else app->show_speakers = false;
        return;
    }
    if (IsKeyPressed(KEY_F1) || (shift && IsKeyPressed(KEY_SLASH))) {
        open_overlay(app, app->overlay == OVERLAY_HELP ? OVERLAY_NONE : OVERLAY_HELP);
        return;
    }
    // the roll call and the vote read their own letters
    if (app->overlay != OVERLAY_NONE) return;

    // presenter clickers send Page Down / Page Up
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_PAGE_UP)) act_toggle(app);
    if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_PAGE_DOWN)) act_next(app);
    if (IsKeyPressed(KEY_R)) act_restart(app);
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_F5)) app->present = !app->present;
    if (IsKeyPressed(KEY_Y)) {
        Session *session = &app->session;
        if (!session->caucus.active && session->gsl.current >= 0) open_overlay(app, OVERLAY_YIELD);
        else toast(app, "Yields are only for a speaker on the general speakers' list");
    }
    if (IsKeyPressed(KEY_LEFT)) act_nudge(app, shift ? -60.0 : -10.0);
    if (IsKeyPressed(KEY_RIGHT)) act_nudge(app, shift ? 60.0 : 10.0);
    if (IsKeyPressed(KEY_X) && app->session.caucus.active) {
        if (confirm_armed(app, ARM_END_CAUCUS)) act_end_caucus(app);
        else toast(app, "Press X again to end the caucus");
    }
    if (app->present && IsKeyPressed(KEY_B)) app->blank = !app->blank;

    // typing needs the chair's panels, so these leave the presentation view
    if (IsKeyPressed(KEY_A)) {
        app->present = false;
        open_speakers_list(app);
        drop_typed_keys();
    }
    if (IsKeyPressed(KEY_M)) {
        app->present = false;
        focus_motion_line(app, NULL);
        drop_typed_keys();
    }
    if (IsKeyPressed(KEY_E)) {
        app->present = false;
        focus_motion_line(app, "ext ");
        drop_typed_keys();
    }
    // a motion starts with its length, so a digit starts typing one
    for (int d = 0; d <= 9; d++) {
        if (!IsKeyPressed(KEY_ZERO + d) && !IsKeyPressed(KEY_KP_0 + d)) continue;
        char digit[2] = { (char)('0' + d), '\0' };
        app->present = false;
        focus_motion_line(app, digit);
        drop_typed_keys();
        break;
    }
    if (IsKeyPressed(KEY_G)) app->add_to_gsl = !app->add_to_gsl;
    if (IsKeyPressed(KEY_C)) open_overlay(app, OVERLAY_ROLLCALL);
    if (IsKeyPressed(KEY_V)) open_overlay(app, OVERLAY_VOTE);
    if (IsKeyPressed(KEY_L)) open_overlay(app, OVERLAY_LOG);
}

static void handle_drops(App *app) {
    if (!IsFileDropped()) return;
    FilePathList files = LoadDroppedFiles();
    for (unsigned int i = 0; i < files.count; i++) {
        char *text = LoadFileText(files.paths[i]);
        if (text == NULL) continue;
        char *clean = utf8_text(text);
        UnloadFileText(text);
        if (clean == NULL) continue;
        act_import_text(app, clean, GetFileName(files.paths[i]));
        MemFree(clean);
    }
    UnloadDroppedFiles(files);
}

static void draw_overlay(App *app) {
    switch (app->overlay) {
        case OVERLAY_SETUP: draw_setup(app); break;
        case OVERLAY_ROLLCALL: draw_rollcall(app); break;
        case OVERLAY_VOTE: draw_vote(app); break;
        case OVERLAY_SETTINGS: draw_settings(app); break;
        case OVERLAY_HELP: draw_help(app); break;
        case OVERLAY_YIELD: draw_yield(app); break;
        case OVERLAY_LOG: draw_log(app); break;
        default: break;
    }
}

static void draw_frame(App *app) {
    float width = (float)GetScreenWidth();
    float height = (float)GetScreenHeight();
    float u = width / 1280.0f < height / 720.0f ? width / 1280.0f : height / 720.0f;
    app->u = u < 0.8f ? 0.8f : (u > 2.0f ? 2.0f : u);

    ClearBackground(app->theme.background);
    bool modal = app->overlay != OVERLAY_NONE;
    if (modal) GuiLock();

    if (app->present) draw_present(app);
    else {
        float u = app->u;
        float top = 46.0f * u;
        float pad = 12.0f * u;
        // the panels change with the focus only while the mouse button is up, so a click lands where it was aimed
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) app->layout_focus = app->focus;
        bool adding = app->layout_focus == app->speaker_pick.text;
        bool proposing = app->layout_focus == app->motion_quick;
        bool listed = app->show_speakers || adding;

        // the motions fill the window (the clock is on the projector); the speakers' list, once asked for,
        // takes the right of it
        Rectangle body = { pad, top + pad, width - 2 * pad, height - top - 2 * pad };
        Rectangle side = { 0 };
        if (listed) {
            float sideW = width * 0.40f;
            if (sideW < 380.0f) sideW = 380.0f;
            side = (Rectangle){ width - sideW, body.y, sideW - pad, body.height };
            body.width = side.x - pad - body.x;
        }

        draw_top_bar(app, (Rectangle){ 0, 0, width, top });
        // while a box is typed in the roster comes up under it: a click on a tile adds that speaker
        // or names the motion's proposer
        Rectangle motions = body;
        Rectangle speakers = side;
        float tilesH = body.height * 0.42f;
        if (proposing) motions.height -= tilesH + pad;
        if (adding) speakers.height -= tilesH + pad;
        // the boxes go before the tiles, a click on a tile may give the focus back. the speakers' box goes
        // before the motion line, so the Enter that brings the list out is not taken by its box as well
        if (listed) draw_speakers_panel(app, speakers);
        draw_motions_panel(app, motions);
        if (adding) draw_roster_grid(app, (Rectangle){ side.x, speakers.y + speakers.height + pad, side.width, tilesH }, false);
        if (proposing) draw_roster_grid(app, (Rectangle){ body.x, motions.y + motions.height + pad, body.width, tilesH }, true);
    }

    if (modal) {
        GuiUnlock();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.45f));
        bool fresh = app->overlay_fresh;
        if (fresh) GuiLock();
        draw_overlay(app);
        if (fresh) GuiUnlock();
        app->overlay_fresh = false;
    }
    draw_toast(app);
}

static void frame(App *app) {
    app->typing = app->focus != NULL;
    app->focus_before = app->focus;
    Vector2 mouse = GetMousePosition();
    if (mouse.x != app->last_mouse.x || mouse.y != app->last_mouse.y) {
        app->mouse_moved_at = app->now;
        app->last_mouse = mouse;
    }
    handle_drops(app);
    handle_keys(app);

    int alerts = update_floor(&app->session, app->now);
    if (alerts & (ALERT_SPEAKER | ALERT_CAUCUS)) app->flash = 3.0f;

    BeginDrawing();
    draw_frame(app);
    EndDrawing();
    if (app->flash > 0.0f) app->flash -= GetFrameTime();
    save_now(app, false);
}

// renders each screen of the demo committee into dir, for checking the layout without a display
static void take_screenshots(App *app, const char *dir) {
    const struct { int width, height; } sizes[] = { { 1280, 720 }, { 1920, 1080 }, { 1024, 640 } };
    char path[512];
    for (int s = 0; s < (int)(sizeof(sizes) / sizeof(sizes[0])); s++) {
        SetWindowSize(sizes[s].width, sizes[s].height);
        for (int scene = 0; scene < DEMO_SCENES; scene++) {
            if (s > 0 && !demo_scene_resized(scene)) continue;
            app->focus = NULL;
            app->now = 1000.0;
            app->mouse_moved_at = -100.0;
            const char *name = build_demo_scene(app, scene);
            apply_theme(app);
            snprintf(path, sizeof(path), "%s/%02d_%s_%dx%d.png", dir, scene + 1, name, sizes[s].width, sizes[s].height);
            // the pointer parked on the top bar hovers nothing; enough frames for the list to scroll into place
            SetMousePosition(0, 0);
            for (int f = 0; f < 24; f++) {
                app->typing = app->focus != NULL;
                BeginDrawing();
                draw_frame(app);
                // the back buffer is only defined before the swap
                if (f == 23) {
                    rlDrawRenderBatchActive();
                    Image shot = LoadImageFromScreen();
                    ExportImage(shot, path);
                    UnloadImage(shot);
                }
                EndDrawing();
            }
        }
    }
}

int main(int argc, char *argv[])
{
    const char *shots = NULL;
    app.session_path = SESSION_FILE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shots = argv[++i];
        else if (strcmp(argv[i], "--session") == 0 && i + 1 < argc) app.session_path = argv[++i];
        else if (strcmp(argv[i], "--demo") == 0) app.demo = true;
        else {
            printf("usage: %s [--session file] [--demo] [--screenshot dir]\n", argv[0]);
            return 1;
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "MUN Timer");
    SetWindowMinSize(1024, 640);
    // Esc leaves text boxes and dialogs, it must never close the app mid-session
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    Image icon = LoadImageFromMemory(".png", app_icon, app_icon_size);
    ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    SetWindowIcon(icon);
    UnloadImage(icon);

    init_session(&app.session);
    load_fonts(&app);
    clear_picker(&app.speaker_pick);
    clear_picker(&app.yield_pick);
    clear_picker(&app.reply_pick);
    clear_picker(&app.roster_pick);
    app.roster_pick.countries = true;
    app.speaker_pick.present_only = true;

    if (shots != NULL) {
        app.demo = true;
        take_screenshots(&app, shots);
        unload_fonts(&app);
        CloseWindow();
        return 0;
    }

    app.now = GetTime();
    if (app.demo) build_demo_scene(&app, 0);
    else if (!load_session(&app.session, app.session_path)) {
        init_session(&app.session);
        open_overlay(&app, OVERLAY_SETUP);
    }
    else toast(&app, "Restored the last session");
    apply_theme(&app);

    // closing while a clock runs takes a second close within 3 s; everything is saved either way
    while (!quit_requested) {
        app.now = GetTime();
        if (WindowShouldClose()) request_quit(&app);
        frame(&app);
    }

    save_now(&app, true);
    unload_fonts(&app);
    CloseWindow();
    return 0;
}
