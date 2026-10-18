#include <ui.h>
#include <assets.h>
#include <countries.h>
#include <raygui.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UI_FONT_SIZE 64
#define BOLD_FONT_SIZE 96
#define DIGIT_FONT_SIZE 360

// ASCII, Latin-1 to Latin Extended-B without the soft hyphen, and ten punctuation marks
#define UI_CODEPOINTS ((0x7E - 0x20 + 1) + (0x24F - 0xA1 + 1 - 1) + 10)

static int ui_codepoints(int *codepoints) {
    int count = 0;
    for (int c = 0x20; c <= 0x7E; c++) codepoints[count++] = c;
    // 0xA0 and 0xAD have no visible glyph, raylib warns about them
    for (int c = 0xA1; c <= 0x24F; c++) {
        if (c != 0xAD) codepoints[count++] = c;
    }
    const int punctuation[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2039, 0x203A };
    for (int i = 0; i < (int)(sizeof(punctuation) / sizeof(punctuation[0])); i++) codepoints[count++] = punctuation[i];
    _Static_assert(sizeof(punctuation) / sizeof(punctuation[0]) == 10, "UI_CODEPOINTS counts ten punctuation marks");
    return count;
}

static Font load_font(const unsigned char *data, int size, int pixels, const int *codepoints, int count, bool mipmaps) {
    Font font = LoadFontFromMemory(".ttf", data, size, pixels, codepoints, count);
    if (mipmaps) {
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    }
    else SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

void load_fonts(App *app) {
    static int codepoints[UI_CODEPOINTS];
    int count = ui_codepoints(codepoints);
    app->font = load_font(noto_regular, noto_regular_size, UI_FONT_SIZE, codepoints, count, true);
    app->bold = load_font(noto_bold, noto_bold_size, BOLD_FONT_SIZE, codepoints, count, true);

    int digits[16];
    int digit_count = 0;
    for (const char *c = "0123456789:+-. "; *c != '\0'; c++) digits[digit_count++] = *c;
    app->digits = load_font(noto_bold, noto_bold_size, DIGIT_FONT_SIZE, digits, digit_count, false);
    GuiSetFont(app->font);
}

void unload_fonts(App *app) {
    UnloadFont(app->font);
    UnloadFont(app->bold);
    UnloadFont(app->digits);
}

// raygui's own default colours on raylib's RAYWHITE, as in CrossCountryTimer
static const Theme light_theme = {
    .background = { 0xF5, 0xF5, 0xF5, 0xFF },
    .panel      = { 0xFF, 0xFF, 0xFF, 0xFF },
    .raised     = { 0xE6, 0xE9, 0xE9, 0xFF },
    .line       = { 0xC9, 0xC9, 0xC9, 0xFF },
    .text       = { 0x00, 0x00, 0x00, 0xFF },
    .muted      = { 0x68, 0x68, 0x68, 0xFF },
    .accent     = { 0x04, 0x92, 0xC7, 0xFF },
    .on_accent  = { 0xFF, 0xFF, 0xFF, 0xFF },
    .running    = { 0x00, 0x9E, 0x2F, 0xFF },
    .warning    = { 0xE0, 0x8B, 0x00, 0xFF },
    .expired    = { 0xE6, 0x29, 0x37, 0xFF },
    .highlight  = { 0xC9, 0xEF, 0xFE, 0xFF },
    .done       = { 0xAE, 0xB7, 0xB8, 0xFF },
    .paused     = { 0x36, 0x8B, 0xAF, 0xFF },
};

// for projectors: dark grey rather than black, so the room isn't lit by a white wall
static const Theme dark_theme = {
    .background = { 0x0B, 0x0E, 0x13, 0xFF },
    .panel      = { 0x14, 0x19, 0x22, 0xFF },
    .raised     = { 0x1A, 0x20, 0x29, 0xFF },
    .line       = { 0x2E, 0x37, 0x46, 0xFF },
    .text       = { 0xF3, 0xF5, 0xF8, 0xFF },
    .muted      = { 0xA3, 0xAD, 0xBB, 0xFF },
    .accent     = { 0x4D, 0xA3, 0xFF, 0xFF },
    .on_accent  = { 0x06, 0x10, 0x1C, 0xFF },
    .running    = { 0x2F, 0xD2, 0x7A, 0xFF },
    .warning    = { 0xFF, 0xB0, 0x20, 0xFF },
    .expired    = { 0xFF, 0x6B, 0x6B, 0xFF },
    .highlight  = { 0x1B, 0x33, 0x52, 0xFF },
    .done       = { 0x6B, 0x75, 0x85, 0xFF },
    .paused     = { 0x8F, 0xB8, 0xFF, 0xFF },
};

static Color mix(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255,
    };
}

void apply_theme(App *app) {
    bool dark = app->session.settings.dark;
    app->theme = dark ? dark_theme : light_theme;
    const Theme *t = &app->theme;

    // DEFAULT colours spread to every control, so this also undoes the dark scheme's per-control colours
    if (dark) {
        GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(t->line));
        GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(t->raised));
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(t->text));
        GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(t->accent));
        GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(t->highlight));
        GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(t->text));
        GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(t->accent));
        GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(mix(t->highlight, t->accent, 0.25f)));
        GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(t->text));
        GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToInt(mix(t->line, t->panel, 0.5f)));
        GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, ColorToInt(t->panel));
        GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, ColorToInt(t->done));
        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(t->panel));
        GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(t->line));
    }
    else {
        // what GuiLoadStyleDefault sets; it can't be called itself once our font is set, it would unload it
        GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x838383ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0xc9c9c9ff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0x686868ff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x5bb2d9ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0xc9effeff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0x6c9bbcff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x0492c7ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x97e8ffff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0x368bafff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, 0xb5c1c2ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, 0xe6e9e9ff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, 0xaeb7b8ff);
        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0xf5f5f5ff);
        GuiSetStyle(DEFAULT, LINE_COLOR, 0x90abb5ff);
    }
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_PADDING, 8);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL, TEXT_ALIGN_MIDDLE);

    if (dark) {
        GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, ColorToInt(t->panel));
        GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, ColorToInt(t->panel));
        GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED, ColorToInt(t->panel));
        GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, ColorToInt(t->text));
        GuiSetStyle(TEXTBOX, BORDER_COLOR_PRESSED, ColorToInt(t->accent));
        // the tick of a check box is drawn in its text colours, the label in LABEL's
        GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL, ColorToInt(t->accent));
        GuiSetStyle(CHECKBOX, TEXT_COLOR_FOCUSED, ColorToInt(t->accent));
        GuiSetStyle(CHECKBOX, TEXT_COLOR_PRESSED, ColorToInt(t->accent));
        GuiSetStyle(CHECKBOX, BASE_COLOR_NORMAL, ColorToInt(t->panel));
    }
    GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(TEXTBOX, TEXT_PADDING, 10);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(TOGGLE, GROUP_PADDING, 4);
    GuiSetStyle(CHECKBOX, TEXT_PADDING, 10);
    invalidate_pickers(app);
}

void draw_text(Font font, const char *text, float x, float y, float size, Color color) {
    DrawTextEx(font, text, (Vector2){ roundf(x), roundf(y) }, size, 0.0f, color);
}

float text_width(Font font, const char *text, float size) {
    return MeasureTextEx(font, text, size, 0.0f).x;
}

float fit_text(Font font, const char *text, float max_size, float width) {
    float measured = text_width(font, text, max_size);
    if (measured <= width || measured <= 0.0f) return max_size;
    float size = max_size * width / measured;
    while (size > 6.0f && text_width(font, text, size) > width) size -= 1.0f;
    return size;
}

void draw_text_in(Font font, const char *text, Rectangle bounds, float size, Color color, int align) {
    char shown[256];
    copy_text(shown, sizeof(shown), text);
    float width = text_width(font, shown, size);
    if (width > bounds.width) {
        // cut whole codepoints until the text and the ellipsis fit
        int len = (int)strlen(shown);
        while (len > 0) {
            len--;
            while (len > 0 && ((unsigned char)shown[len] & 0xC0) == 0x80) len--;
            char cut[260];
            snprintf(cut, sizeof(cut), "%.*s…", len, shown);
            width = text_width(font, cut, size);
            if (width <= bounds.width || len == 0) {
                copy_text(shown, sizeof(shown), cut);
                break;
            }
        }
    }
    float x = bounds.x;
    if (align == 1) x = bounds.x + (bounds.width - width) / 2.0f;
    else if (align == 2) x = bounds.x + bounds.width - width;
    float y = bounds.y + (bounds.height - size) / 2.0f;
    draw_text(font, shown, x, y, size, color);
}

void draw_strike(Font font, const char *text, float x, float y, float size, Color color) {
    draw_text(font, text, x, y, size, color);
    float width = text_width(font, text, size);
    float thick = size / 14.0f < 2.0f ? 2.0f : size / 14.0f;
    float middle = y + size * 0.56f;
    DrawLineEx((Vector2){ x - 2, middle }, (Vector2){ x + width + 2, middle }, thick, color);
}

// width of one equal cell for the digits at the font's own size
static float digit_cell(Font font) {
    float widest = 0.0f;
    for (int c = '0'; c <= '9'; c++) {
        GlyphInfo glyph = GetGlyphInfo(font, c);
        if (glyph.advanceX > widest) widest = (float)glyph.advanceX;
    }
    return widest;
}

static float glyph_advance(Font font, int codepoint) {
    int index = GetGlyphIndex(font, codepoint);
    if (font.glyphs[index].advanceX > 0) return (float)font.glyphs[index].advanceX;
    return font.recs[index].width;
}

void draw_clock(App *app, const char *face, Rectangle bounds, Color color) {
    Font font = app->digits;
    float base = (float)font.baseSize;
    float cell = digit_cell(font);

    float width = 0.0f;
    for (const char *c = face; *c != '\0'; c++) {
        width += (*c >= '0' && *c <= '9') ? cell : glyph_advance(font, *c);
    }
    if (width <= 0.0f) return;

    // digits stand from the top of a '0' to its bottom, which is what has to fit the height
    int zero = GetGlyphIndex(font, '0');
    float top = (float)font.glyphs[zero].offsetY;
    float ink = font.recs[zero].height;
    float scale = bounds.width / width;
    if (ink * scale > bounds.height) scale = bounds.height / ink;
    float size = base * scale;

    float x = bounds.x + (bounds.width - width * scale) / 2.0f;
    float y = bounds.y + (bounds.height - ink * scale) / 2.0f - top * scale;
    for (const char *c = face; *c != '\0'; c++) {
        bool digit = *c >= '0' && *c <= '9';
        float advance = glyph_advance(font, *c);
        float slot = digit ? cell : advance;
        float inset = (slot - advance) / 2.0f;
        DrawTextCodepoint(font, *c, (Vector2){ roundf(x + inset * scale), roundf(y) }, size, color);
        x += slot * scale;
    }
}

void draw_panel(App *app, Rectangle bounds) {
    DrawRectangleRounded(bounds, 8.0f / (bounds.height < bounds.width ? bounds.height : bounds.width), 6, app->theme.panel);
    DrawRectangleRoundedLinesEx(bounds, 8.0f / (bounds.height < bounds.width ? bounds.height : bounds.width), 6, 1.0f, app->theme.line);
}

void gui_size(int size) {
    GuiSetStyle(DEFAULT, TEXT_SIZE, size);
}

bool button(App *app, Rectangle bounds, const char *text, ButtonKind kind) {
    const Theme *t = &app->theme;
    // the white scheme keeps raygui's own buttons; only a destructive one gets red text
    if (!app->session.settings.dark) {
        if (kind != BUTTON_DANGER) return GuiButton(bounds, text) != 0;
        int states[3] = { TEXT_COLOR_NORMAL, TEXT_COLOR_FOCUSED, TEXT_COLOR_PRESSED };
        int before[3];
        for (int i = 0; i < 3; i++) {
            before[i] = GuiGetStyle(BUTTON, states[i]);
            GuiSetStyle(BUTTON, states[i], ColorToInt(t->expired));
        }
        bool pressed = GuiButton(bounds, text) != 0;
        for (int i = 0; i < 3; i++) GuiSetStyle(BUTTON, states[i], before[i]);
        return pressed;
    }
    int saved[12];
    for (int i = 0; i < 12; i++) saved[i] = GuiGetStyle(BUTTON, i);

    Color base = t->raised;
    Color label = t->text;
    Color border = t->line;
    if (kind == BUTTON_PRIMARY) { base = t->accent; label = t->on_accent; border = t->accent; }
    else if (kind == BUTTON_GOOD) { base = t->running; label = t->on_accent; border = t->running; }
    else if (kind == BUTTON_DANGER) { base = t->panel; label = t->expired; border = t->expired; }
    else if (kind == BUTTON_QUIET) { base = t->panel; label = t->muted; border = t->panel; }

    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(border));
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(base));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(label));
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(kind == BUTTON_QUIET ? t->line : mix(border, t->text, 0.25f)));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(mix(base, t->text, 0.08f)));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(kind == BUTTON_QUIET ? t->text : label));
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToInt(mix(border, t->text, 0.4f)));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(mix(base, t->text, 0.18f)));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToInt(label));

    bool pressed = GuiButton(bounds, text) != 0;
    for (int i = 0; i < 12; i++) GuiSetStyle(BUTTON, i, saved[i]);
    return pressed;
}

bool text_field(App *app, Rectangle bounds, char *text, int size, const char *placeholder) {
    bool editing = app->focus == text;
    bool submitted = false;
    if (GuiTextBox(bounds, text, size, editing)) {
        if (editing) {
            submitted = IsKeyPressed(KEY_ENTER);
            app->focus = NULL;
        }
        else app->focus = text;
    }
    if (editing && app->focus == text && IsKeyPressed(KEY_ESCAPE)) app->focus = NULL;

    if (text[0] == '\0' && placeholder != NULL && app->focus != text) {
        float textSize = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
        Rectangle inner = { bounds.x + GuiGetStyle(TEXTBOX, TEXT_PADDING), bounds.y, bounds.width - 2 * GuiGetStyle(TEXTBOX, TEXT_PADDING), bounds.height };
        draw_text_in(app->font, placeholder, inner, textSize, app->theme.muted, 0);
    }
    return submitted;
}

void focus_field(App *app, char *text) {
    app->focus = text;
    place_text_cursor(text);
}

bool is_focused(const App *app, const char *text) {
    return app->focus == text;
}

bool clicked(Rectangle bounds) {
    return !GuiIsLocked() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), bounds);
}

bool hovered(Rectangle bounds) {
    return !GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), bounds);
}

void clear_picker(Picker *pick) {
    pick->text[0] = '\0';
    pick->matched_for[0] = '\x01';
    pick->match_count = 0;
    pick->selected = 0;
}

void invalidate_pickers(App *app) {
    Picker *picks[] = { &app->speaker_pick, &app->yield_pick, &app->reply_pick, &app->roster_pick };
    for (int i = 0; i < 4; i++) {
        picks[i]->matched_for[0] = '\x01';
        picks[i]->matched_for[1] = '\0';
    }
}

void update_picker(App *app, Picker *pick) {
    if (strcmp(pick->text, pick->matched_for) == 0) return;
    copy_text(pick->matched_for, sizeof(pick->matched_for), pick->text);
    if (pick->countries) {
        pick->match_count = pick->text[0] != '\0' ? search_countries(pick->text, pick->matches, MAX_MATCHES) : 0;
    }
    else {
        pick->match_count = match_delegates(&app->session.roster, pick->text, pick->present_only, pick->matches, MAX_MATCHES);
    }
    pick->selected = 0;
}

const char *picker_name(App *app, const Picker *pick, int index) {
    if (index < 0 || index >= pick->match_count) return NULL;
    int item = pick->matches[index];
    if (pick->countries) return (item >= 0 && item < COUNTRY_COUNT) ? COUNTRIES[item].name : NULL;
    return (item >= 0 && item < app->session.roster.count) ? app->session.roster.items[item].name : NULL;
}

const char *picker_choice(App *app, Picker *pick) {
    static char typed[NAME_SIZE];
    update_picker(app, pick);
    if (pick->text[0] != '\0') {
        const char *best = picker_name(app, pick, pick->selected);
        if (best != NULL) return best;
    }
    copy_text(typed, sizeof(typed), pick->text);
    trim_text(typed);
    return typed[0] != '\0' ? typed : NULL;
}

bool picker_field(App *app, Picker *pick, Rectangle bounds, const char *placeholder) {
    update_picker(app, pick);
    bool editing = app->focus == pick->text;
    if (editing && pick->match_count > 0) {
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) pick->selected = (pick->selected + 1) % pick->match_count;
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) pick->selected = (pick->selected + pick->match_count - 1) % pick->match_count;
    }
    bool submitted = text_field(app, bounds, pick->text, sizeof(pick->text), placeholder);
    update_picker(app, pick);

    // grey completion after what was typed
    const char *best = pick->text[0] != '\0' ? picker_name(app, pick, pick->selected) : NULL;
    if (best != NULL && app->focus == pick->text) {
        float size = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
        float typedW = (float)GuiGetTextWidth(pick->text);
        float x = bounds.x + GuiGetStyle(TEXTBOX, TEXT_PADDING) + typedW + 4;
        Rectangle rest = { x, bounds.y, bounds.x + bounds.width - x - 6, bounds.height };
        char hint[NAME_SIZE + 8];
        size_t typedLen = strlen(pick->text);
        char folded_best[NAME_SIZE * 2];
        char folded_typed[NAME_SIZE * 2];
        fold_name(best, folded_best, sizeof(folded_best));
        fold_name(pick->text, folded_typed, sizeof(folded_typed));
        bool prefix = strlen(best) >= typedLen;
        for (size_t i = 0; prefix && i < typedLen; i++) {
            prefix = tolower((unsigned char)best[i]) == tolower((unsigned char)pick->text[i]);
        }
        if (prefix) snprintf(hint, sizeof(hint), "%s", best + typedLen);
        else snprintf(hint, sizeof(hint), "– %s", best);
        if (rest.width > 20 && strcmp(folded_best, folded_typed) != 0) {
            rest.x -= 4;
            draw_text_in(app->font, hint, rest, size, app->theme.muted, 0);
        }
    }
    return submitted;
}

void toast(App *app, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(app->toast, sizeof(app->toast), format, args);
    va_end(args);
    app->toast_until = app->now + 3.0;
}

void draw_toast(App *app) {
    if (app->toast[0] == '\0' || app->now > app->toast_until) return;
    float u = app->u > 0.0f ? app->u : 1.0f;
    float size = chair_text(app) * 1.05f;
    float width = text_width(app->font, app->toast, size) + 40.0f * u;
    float left = (float)(app->toast_until - app->now);
    float alpha = left < 0.4f ? left / 0.4f : 1.0f;
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float boxH = size * 2.2f;
    // over the right column, clear of the clock and the buttons under it; centred when presenting
    Rectangle box = { (screenW - width) / 2.0f, screenH - boxH - 40.0f * u, width, boxH };
    if (!app->present) {
        float columnX = screenW - (screenW * 0.40f < 380.0f ? 380.0f : screenW * 0.40f);
        float columnW = screenW - columnX - 12.0f * u;
        if (width > columnW - 16.0f * u) width = columnW - 16.0f * u;
        box = (Rectangle){ columnX + (columnW - width) / 2.0f, screenH - 30.0f * u - boxH - 18.0f * u, width, boxH };
    }
    Color back = app->session.settings.dark ? (Color){ 0xF2, 0xF6, 0xFC, 255 } : (Color){ 0x40, 0x40, 0x40, 255 };
    Color front = app->session.settings.dark ? (Color){ 0x0A, 0x10, 0x1C, 255 } : (Color){ 0xFF, 0xFF, 0xFF, 255 };
    DrawRectangleRounded(box, 0.5f, 8, Fade(back, 0.94f * alpha));
    draw_text_in(app->font, app->toast, (Rectangle){ box.x + 20, box.y, box.width - 40, box.height }, size, Fade(front, alpha), 1);
}

Color clock_color(App *app, const Clock *clock, double now) {
    const Theme *t = &app->theme;
    if (clock_expired(clock, now)) return t->expired;
    if (clock_warning(clock, now, app->session.settings.warn_time)) return t->warning;
    if (clock->running) return t->running;
    if (clock_elapsed(clock, now) > 0.0) return t->paused;
    return t->text;
}

void touch(App *app) {
    app->dirty = true;
}

void before(App *app, const char *label) {
    remember(&app->history, &app->session, label);
    app->dirty = true;
}

bool confirm_armed(App *app, Arm arm) {
    if (app->armed == arm && app->now < app->armed_until) {
        app->armed = ARM_NONE;
        return true;
    }
    app->armed = arm;
    app->armed_until = app->now + 3.0;
    return false;
}

bool is_armed(const App *app, Arm arm) {
    return app->armed == arm && app->now < app->armed_until;
}

float chair_text(const App *app) {
    float size = 18.0f * app->u;
    return size < 15.0f ? 15.0f : size;
}

SpeakerList *add_target(App *app) {
    Session *session = &app->session;
    bool caucus = session->caucus.active && caucus_has_speakers(session->caucus.motion.type);
    if (caucus && !app->add_to_gsl) return &session->caucus.speakers;
    return &session->gsl;
}

// the roster's spelling of what was typed, through country aliases too ("usa" -> "United States")
const char *roster_spelling(const Session *session, const char *typed) {
    int delegate = find_delegate(&session->roster, typed);
    if (delegate >= 0) return session->roster.items[delegate].name;
    int country = find_country(typed);
    if (country >= 0) {
        delegate = find_delegate(&session->roster, COUNTRIES[country].name);
        if (delegate >= 0) return session->roster.items[delegate].name;
    }
    return typed;
}

static bool valid_utf8(const unsigned char *s) {
    while (*s != '\0') {
        int extra = *s < 0x80 ? 0 : (*s >= 0xC2 && *s <= 0xDF) ? 1 : (*s >= 0xE0 && *s <= 0xEF) ? 2 : (*s >= 0xF0 && *s <= 0xF4) ? 3 : -1;
        if (extra < 0) return false;
        for (int i = 1; i <= extra; i++) {
            if ((s[i] & 0xC0) != 0x80) return false;
        }
        s += extra + 1;
    }
    return true;
}

char *utf8_text(const char *text) {
    const unsigned char *in = (const unsigned char *)text;
    size_t len = strlen(text);
    char *out = MemAlloc((unsigned int)(len * 3 + 1));
    if (out == NULL) return NULL;
    if (valid_utf8(in)) {
        memcpy(out, text, len + 1);
        return out;
    }
    // Windows-1252: 0x80-0x9F has its own letters and punctuation, the rest matches Unicode
    static const unsigned short high[32] = {
        0x20AC, 0x20, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x20, 0x017D, 0x20,
        0x20, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x20, 0x017E, 0x0178,
    };
    size_t n = 0;
    for (; *in != '\0'; in++) {
        unsigned int cp = *in < 0x80 ? *in : (*in < 0xA0 ? high[*in - 0x80] : *in);
        if (cp < 0x80) out[n++] = (char)cp;
        else if (cp < 0x800) {
            out[n++] = (char)(0xC0 | (cp >> 6));
            out[n++] = (char)(0x80 | (cp & 0x3F));
        }
        else {
            out[n++] = (char)(0xE0 | (cp >> 12));
            out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    out[n] = '\0';
    return out;
}
