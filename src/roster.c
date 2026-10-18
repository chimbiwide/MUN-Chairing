#include <roster.h>
#include <countries.h>
#include <ctype.h>
#include <string.h>

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

// the Unicode space separators (no-break, ogham, en quad to hair, narrow no-break, mathematical, ideographic),
// the line and paragraph separators, and the zero width space, word joiner and byte order mark,
// which pasted text and files bring along
static const char *const WIDE_SPACES[] = {
    "\u00A0", "\u1680", "\u2000", "\u2001", "\u2002", "\u2003", "\u2004", "\u2005", "\u2006", "\u2007", "\u2008",
    "\u2009", "\u200A", "\u200B", "\u2028", "\u2029", "\u202F", "\u205F", "\u2060", "\u3000", "\uFEFF",
};
// " ' and the curly, low and angle quotes “ ” „ ‘ ’ « »
static const char *const QUOTES[] = {"\"", "'", "\u201C", "\u201D", "\u201E", "\u2018", "\u2019", "\u00AB", "\u00BB"};
// - * + and • ◦ ‣ ⁃ – — · ▪ ● ○ ■ □ ► ➢ ✓, then the bullet Word pastes from its symbol font
static const char *const BULLETS[] = {
    "-", "*", "+", "\u2022", "\u25E6", "\u2023", "\u2043", "\u2013", "\u2014", "\u00B7",
    "\u25AA", "\u25CF", "\u25CB", "\u25A0", "\u25A1", "\u25BA", "\u27A2", "\u2713", "\uF0B7",
};
// CR, LF, the vertical tab and form feed that Word and PowerPoint paste for soft returns, U+2028 and U+2029
static const char *const BREAKS[] = {"\r", "\n", "\v", "\f", "\u2028", "\u2029"};

// byte length of the entry of list that [start, end) begins with, 0 when none
static int prefix_in(const char *start, const char *end, const char *const *list, int count) {
    for (int i = 0; i < count; i++) {
        size_t len = strlen(list[i]);
        if ((size_t)(end - start) >= len && memcmp(start, list[i], len) == 0) return (int)len;
    }
    return 0;
}

// byte length of the entry of list that [start, end) ends with, 0 when none
static int suffix_in(const char *start, const char *end, const char *const *list, int count) {
    for (int i = 0; i < count; i++) {
        size_t len = strlen(list[i]);
        if ((size_t)(end - start) >= len && memcmp(end - len, list[i], len) == 0) return (int)len;
    }
    return 0;
}

// ASCII whitespace and the other control bytes, whatever the locale, so a UTF-8 sequence is never cut
static bool ascii_space(char c) {
    unsigned char byte = (unsigned char)c;
    return (byte > 0 && byte <= ' ') || byte == 0x7F;
}

// tabs, line breaks and the other control bytes inside a name become spaces
static void blank_controls(char *text) {
    for (char *c = text; *c != '\0'; c++) {
        if (ascii_space(*c)) *c = ' ';
    }
}

static int space_prefix(const char *start, const char *end) {
    if (start < end && ascii_space(*start)) return 1;
    return prefix_in(start, end, WIDE_SPACES, COUNT_OF(WIDE_SPACES));
}

static int space_suffix(const char *start, const char *end) {
    if (start < end && ascii_space(end[-1])) return 1;
    return suffix_in(start, end, WIDE_SPACES, COUNT_OF(WIDE_SPACES));
}

static const char *skip_spaces(const char *start, const char *end) {
    for (int len = space_prefix(start, end); len > 0; len = space_prefix(start, end)) start += len;
    return start;
}

static const char *drop_spaces(const char *start, const char *end) {
    for (int len = space_suffix(start, end); len > 0; len = space_suffix(start, end)) end -= len;
    return end;
}

// where the line starting at line ends: at a line break or at end
static const char *line_end(const char *line, const char *end) {
    for (; line < end; line++) {
        // only a control byte or the lead byte of U+2028 / U+2029 can start a break
        unsigned char byte = (unsigned char)*line;
        if ((byte <= '\r' || byte == 0xE2) && prefix_in(line, end, BREAKS, COUNT_OF(BREAKS)) > 0) return line;
    }
    return end;
}

// past the line break at stop, if there is one
static const char *next_line(const char *stop, const char *end) {
    return stop + prefix_in(stop, end, BREAKS, COUNT_OF(BREAKS));
}

// same_name(text, x) for an x that is already folded and not empty.
// a text starting with an ASCII letter or digit folds to something starting with it in lowercase
static bool folds_to(const char *text, const char *folded) {
    unsigned char first = (unsigned char)text[0];
    if (first < 0x80 && isalnum(first) && tolower(first) != (unsigned char)folded[0]) return false;
    char fold[NAME_SIZE * 2];
    fold_name(text, fold, sizeof(fold));
    return strcmp(fold, folded) == 0;
}

// count kept inside the array in case a caller wrote a bad one
static int roster_size(const Roster *roster) {
    if (roster->count < 0) return 0;
    return roster->count < MAX_DELEGATES ? roster->count : MAX_DELEGATES;
}

static int find_folded(const Roster *roster, const char *folded) {
    if (folded[0] == '\0') return -1;
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) {
        if (folds_to(roster->items[i].name, folded)) return i;
    }
    return -1;
}

// whether count_present counts the delegate
static bool is_counted(const Roster *roster, const Delegate *delegate) {
    return !roster->roll_taken || delegate->attendance == PRESENT || delegate->attendance == PRESENT_VOTING;
}

void clear_roster(Roster *roster) {
    memset(roster, 0, sizeof(*roster));
}

int add_delegate(Roster *roster, const char *name) {
    if (name == NULL) name = "";
    // trimmed before the copy so a long run of spaces can't push the name out of the buffer
    char clean[NAME_SIZE];
    copy_text(clean, sizeof(clean), skip_spaces(name, name + strlen(name)));
    blank_controls(clean);
    clean[drop_spaces(clean, clean + strlen(clean)) - clean] = '\0';

    // nothing but punctuation ("-", "...") names nobody
    char folded[NAME_SIZE * 2];
    fold_name(clean, folded, sizeof(folded));
    if (folded[0] == '\0') return DELEGATE_EMPTY;
    if (find_folded(roster, folded) >= 0) return DELEGATE_DUPLICATE;
    int count = roster_size(roster);
    if (count >= MAX_DELEGATES) return DELEGATE_FULL;

    Delegate *delegate = &roster->items[count];
    *delegate = (Delegate){.attendance = ABSENT, .ballot = BALLOT_NONE, .veto = is_p5(clean)};
    copy_text(delegate->name, NAME_SIZE, clean);
    roster->count = count + 1;
    return count;
}

bool remove_delegate(Roster *roster, int index) {
    int count = roster_size(roster);
    if (index < 0 || index >= count) return false;
    memmove(&roster->items[index], &roster->items[index + 1], (size_t)(count - index - 1) * sizeof(Delegate));
    roster->count = count - 1;
    roster->items[roster->count] = (Delegate){0};
    return true;
}

int find_delegate(const Roster *roster, const char *name) {
    char folded[NAME_SIZE * 2];
    fold_name(name, folded, sizeof(folded));
    return find_folded(roster, folded);
}

// past any spaces and quotes
static const char *skip_space_quotes(const char *start, const char *end) {
    for (;;) {
        int len = space_prefix(start, end);
        if (len == 0) len = prefix_in(start, end, QUOTES, COUNT_OF(QUOTES));
        if (len == 0) return start;
        start += len;
    }
}

// before any trailing spaces, quotes, commas and semicolons
static const char *drop_tail(const char *start, const char *end) {
    for (;;) {
        int len = space_suffix(start, end);
        if (len == 0) len = suffix_in(start, end, QUOTES, COUNT_OF(QUOTES));
        if (len == 0 && end > start && (end[-1] == ',' || end[-1] == ';')) len = 1;
        if (len == 0) return end;
        end -= len;
    }
}

// past a list marker and the whitespace after it: "1.", "2)", a row number up to 999, "-", "•".
// a marker needs whitespace or the end after it, so "3M Delegation" and "-Brazil" are left alone
static const char *skip_marker(const char *start, const char *end) {
    const char *after = start;
    while (after < end && after - start < 3 && isdigit((unsigned char)*after)) after++;
    if (after > start) {
        if (after < end && (*after == '.' || *after == ')')) after++;
    }
    else after += prefix_in(start, end, BULLETS, COUNT_OF(BULLETS));
    if (after == start || (after < end && space_prefix(after, end) == 0)) return start;
    return skip_spaces(after, end);
}

// a cell holding nothing but a marker or a row number: "1.", "12", "•"
static bool only_marker(const char *start, const char *end) {
    start = skip_space_quotes(start, end);
    end = drop_tail(start, end);
    return start < end && skip_marker(start, end) == end;
}

// past the double quotes that open at c and close before end ("" inside is a quote mark), c when they don't close
static const char *past_quotes(const char *c, const char *end) {
    if (c >= end || *c != '"') return c;
    for (const char *q = c + 1; q < end; q++) {
        if (*q != '"') continue;
        if (q + 1 < end && q[1] == '"') q++;
        else return q + 1;
    }
    return c;
}

// where the cell starting at start ends: at a tab or, with commas (only used on text without tabs), at a ','
// or ';'. double quotes that open the cell (after spaces and a list marker) and close on the line keep their
// commas: "Korea, Republic of". any other double quote is a plain character, so a stray one can't swallow
// the rest of the line
static const char *cell_end(const char *start, const char *end, bool commas) {
    if (!commas) {
        const char *tab = memchr(start, '\t', (size_t)(end - start));
        return tab != NULL ? tab : end;
    }
    const char *c = past_quotes(skip_marker(skip_spaces(start, end), end), end);
    while (c < end && *c != ',' && *c != ';') c++;
    return c;
}

// a ',' or ';' ending a cell with no space after it, the way spreadsheets write them ("Brazil,BR,12")
static bool has_csv_separator(const char *start, const char *end) {
    for (const char *stop = cell_end(start, end, true); stop < end; stop = cell_end(stop + 1, end, true)) {
        if (stop + 1 == end || stop[1] != ' ') return true;
    }
    return false;
}

// a CSV file as spreadsheets save one: two or more lines, each that isn't blank with a ',' or ';'
// outside quotes and no space after it ("Brazil,BR,12"). a column with "Korea, Republic of" is no CSV
static bool is_csv(const char *text, const char *text_end) {
    int lines = 0;
    const char *line = text;
    while (line < text_end) {
        const char *end = line_end(line, text_end);
        if (skip_spaces(line, end) < end) {
            if (!has_csv_separator(line, end)) return false;
            lines++;
        }
        line = next_line(end, text_end);
    }
    return lines >= 2;
}

// the cell [start, end) without spaces and quotes around it, a list marker or trailing separators,
// copied into out. add_delegate cuts it down to NAME_SIZE without splitting a character
static void cell_text(const char *start, const char *end, char *out, int size) {
    start = skip_space_quotes(start, end);
    start = skip_space_quotes(skip_marker(start, end), end);
    end = drop_tail(start, end);
    size_t len = (size_t)(end - start);
    if (len >= (size_t)size) len = (size_t)size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

// every piece between ',' and ';' on every line is a country or blank: a list that spans lines
// ("Brazil, France\nChad"). "Korea, Republic of" is no such list, "Republic of" is no country
static bool all_countries(const char *text, const char *text_end) {
    const char *line = text;
    while (line < text_end) {
        const char *end = line_end(line, text_end);
        const char *start = line;
        for (;;) {
            const char *stop = cell_end(start, end, true);
            char name[NAME_SIZE * 2];
            cell_text(start, stop, name, sizeof(name));
            if (name[0] != '\0' && find_country(name) < 0) return false;
            if (stop >= end) break;
            start = stop + 1;
        }
        line = next_line(end, text_end);
    }
    return true;
}

// adds the cells of a line: every one, or just the first that is more than a row number
// ("1.\tBrazil", "12,Brazil,BR"). false once the roster is full
static bool add_line(Roster *roster, const char *start, const char *end, bool commas, bool every, int *added) {
    const char *stop = cell_end(start, end, commas);
    while (!every && stop < end && only_marker(start, stop)) {
        start = stop + 1;
        stop = cell_end(start, end, commas);
    }
    for (;;) {
        char name[NAME_SIZE * 2];
        cell_text(start, stop, name, sizeof(name));
        int result = add_delegate(roster, name);
        if (result == DELEGATE_FULL) return false;
        if (result >= 0) (*added)++;
        if (!every || stop >= end) return true;
        start = stop + 1;
        stop = cell_end(start, end, commas);
    }
}

int import_delegates(Roster *roster, const char *text) {
    if (text == NULL) return 0;
    const char *text_end = text + strlen(text);
    // one line is a list. lines are a column, spreadsheet rows (tabs) or a CSV file,
    // unless every piece on them is a country
    bool tabs = strchr(text, '\t') != NULL;
    bool rows = tabs || line_end(text, text_end) < text_end;
    bool list = !rows || (!tabs && all_countries(text, text_end));
    bool csv = !list && !tabs && is_csv(text, text_end);

    int added = 0;
    const char *line = text;
    while (line < text_end) {
        const char *end = rows ? line_end(line, text_end) : text_end;
        if (!add_line(roster, line, end, list || csv, list, &added)) break;
        line = next_line(end, text_end);
    }
    return added;
}

void sort_delegates(Roster *roster) {
    int count = roster_size(roster);
    // a folded name is never longer than the name
    char keys[MAX_DELEGATES][NAME_SIZE];
    for (int i = 0; i < count; i++) fold_name(roster->items[i].name, keys[i], sizeof(keys[i]));

    // insertion sort: stable, and rosters are small
    for (int i = 1; i < count; i++) {
        Delegate moving = roster->items[i];
        char key[NAME_SIZE];
        memcpy(key, keys[i], sizeof(key));
        int at = i;
        while (at > 0 && strcmp(keys[at - 1], key) > 0) {
            roster->items[at] = roster->items[at - 1];
            memcpy(keys[at], keys[at - 1], sizeof(key));
            at--;
        }
        roster->items[at] = moving;
        memcpy(keys[at], key, sizeof(key));
    }
}

int match_delegates(const Roster *roster, const char *query, bool present_only, int *out, int max) {
    if (out == NULL || max <= 0) return 0;
    char typed[NAME_SIZE * 2];
    fold_name(query, typed, sizeof(typed));

    int found[MAX_DELEGATES];
    int scores[MAX_DELEGATES];
    int count = 0;
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) {
        const Delegate *delegate = &roster->items[i];
        if (present_only && !is_counted(roster, delegate)) continue;
        // a blank query matches everyone equally
        int score = typed[0] == '\0' ? 1 : match_score(delegate->name, query);
        if (score <= 0) continue;
        int at = count++;
        while (at > 0 && scores[at - 1] < score) {
            found[at] = found[at - 1];
            scores[at] = scores[at - 1];
            at--;
        }
        found[at] = i;
        scores[at] = score;
    }

    int written = count < max ? count : max;
    memcpy(out, found, (size_t)written * sizeof(int));
    return written;
}

int count_present(const Roster *roster) {
    int count = 0;
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) {
        if (is_counted(roster, &roster->items[i])) count++;
    }
    return count;
}

int count_attendance(const Roster *roster, Attendance attendance) {
    int count = 0;
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) {
        if (roster->items[i].attendance == attendance) count++;
    }
    return count;
}

void clear_ballots(Roster *roster) {
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) roster->items[i].ballot = BALLOT_NONE;
}

Tally count_ballots(const Roster *roster) {
    Tally tally = {0};
    int total = roster_size(roster);
    for (int i = 0; i < total; i++) {
        const Delegate *delegate = &roster->items[i];
        if (!is_counted(roster, delegate)) continue;
        switch (delegate->ballot) {
            case BALLOT_YES: tally.yes++; break;
            case BALLOT_NO:
                tally.no++;
                if (delegate->veto) tally.vetoed = true;
                break;
            case BALLOT_ABSTAIN: tally.abstain++; break;
            default: tally.missing++; break;
        }
    }
    return tally;
}

int simple_majority(int n) {
    return n > 0 ? n / 2 + 1 : 0;
}

int two_thirds(int n) {
    return n > 0 ? (int)((2LL * n + 2) / 3) : 0;
}

int one_third(int n) {
    return n > 0 ? n / 3 + (n % 3 != 0) : 0;
}
