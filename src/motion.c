#include <motion.h>
#include <countries.h>
#include <duration.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))
// a typed line has far fewer; words past this are ignored
#define MAX_WORDS 96
// the most words a proposer's name may take: "Bosnia and Herzegovina", "United States of America"
#define MAX_RUN 5

// one word of a typed line, pointing into it
typedef struct {
    const char *start;
    size_t length;
} Word;

typedef struct {
    Word items[MAX_WORDS];
    int count;
} WordList;

static const struct {
    const char *word;
    MotionType type;
} KIND_WORDS[] = {
    {"mod", MOTION_MOD}, {"moderated", MOTION_MOD}, {"mc", MOTION_MOD},
    {"unmod", MOTION_UNMOD}, {"unmoderated", MOTION_UNMOD}, {"umc", MOTION_UNMOD}, {"uc", MOTION_UNMOD},
    {"consult", MOTION_CONSULT}, {"consultation", MOTION_CONSULT}, {"cotw", MOTION_CONSULT},
    {"rr", MOTION_ROUND_ROBIN}, {"robin", MOTION_ROUND_ROBIN}, {"round", MOTION_ROUND_ROBIN},
    {"ext", MOTION_EXTEND}, {"extend", MOTION_EXTEND}, {"extension", MOTION_EXTEND},
    {"st", MOTION_SPEAKING_TIME}, {"speaking", MOTION_SPEAKING_TIME},
    {"close", MOTION_CLOSE_DEBATE}, {"cloture", MOTION_CLOSE_DEBATE},
    {"table", MOTION_TABLE}, {"postpone", MOTION_TABLE},
    {"suspend", MOTION_SUSPEND}, {"recess", MOTION_SUSPEND},
    {"adjourn", MOTION_ADJOURN},
    {"intro", MOTION_INTRODUCE}, {"introduce", MOTION_INTRODUCE}, {"dr", MOTION_INTRODUCE}, {"wp", MOTION_INTRODUCE},
    {"other", MOTION_OTHER},
};

// units typed apart from their number ("10 min"). "second" is left out: after a caucus
// length it is more likely "second reading" than a unit, so only a speaking time takes it
static const char *const UNIT_WORDS[] = {"m", "min", "mins", "minute", "minutes", "s", "sec", "secs", "seconds"};

// writes message for the chair and gives false, so callers can return it
static bool fail(char *error, int error_size, const char *message) {
    copy_text(error, error_size, message);
    return false;
}

// type as a position in the voting order, MOTION_OTHER for a value outside the enum
static MotionType known_type(MotionType type) {
    int value = (int)type;
    return value >= 0 && value < MOTION_TYPE_COUNT ? type : MOTION_OTHER;
}

// length of text held in a buffer of size bytes, even when the terminator is missing
static size_t held_length(const char *text, size_t size) {
    const char *end = memchr(text, '\0', size);
    return end != NULL ? (size_t)(end - text) : size;
}

const char *motion_name(MotionType type) {
    switch (type) {
        case MOTION_ADJOURN: return "Adjourn the meeting";
        case MOTION_SUSPEND: return "Suspend the meeting";
        case MOTION_EXTEND: return "Extend caucus";
        case MOTION_UNMOD: return "Unmoderated caucus";
        case MOTION_CONSULT: return "Consultation of the whole";
        case MOTION_ROUND_ROBIN: return "Round robin";
        case MOTION_MOD: return "Moderated caucus";
        case MOTION_SPEAKING_TIME: return "Set speaking time";
        case MOTION_INTRODUCE: return "Introduce document";
        case MOTION_TABLE: return "Table debate";
        case MOTION_CLOSE_DEBATE: return "Close debate";
        default: return "Other motion";
    }
}

const char *motion_label(MotionType type) {
    switch (type) {
        case MOTION_ADJOURN: return "Adjourn";
        case MOTION_SUSPEND: return "Suspend";
        case MOTION_EXTEND: return "Extend";
        case MOTION_UNMOD: return "Unmod";
        case MOTION_CONSULT: return "Consult";
        case MOTION_ROUND_ROBIN: return "Round robin";
        case MOTION_MOD: return "Mod";
        case MOTION_SPEAKING_TIME: return "Speaking time";
        case MOTION_INTRODUCE: return "Introduce";
        case MOTION_TABLE: return "Table";
        case MOTION_CLOSE_DEBATE: return "Close debate";
        default: return "Other";
    }
}

bool motion_uses_total(MotionType type) {
    return type == MOTION_UNMOD || type == MOTION_CONSULT || type == MOTION_MOD || type == MOTION_EXTEND;
}

bool motion_uses_speaker(MotionType type) {
    return type == MOTION_MOD || type == MOTION_ROUND_ROBIN || type == MOTION_SPEAKING_TIME;
}

bool motion_uses_topic(MotionType type) {
    return type == MOTION_MOD || type == MOTION_UNMOD || type == MOTION_CONSULT || type == MOTION_ROUND_ROBIN ||
           type == MOTION_INTRODUCE || type == MOTION_OTHER;
}

bool motion_starts_caucus(MotionType type) {
    return type == MOTION_UNMOD || type == MOTION_CONSULT || type == MOTION_MOD || type == MOTION_ROUND_ROBIN;
}

VoteRule motion_rule(MotionType type) {
    return type == MOTION_CLOSE_DEBATE || type == MOTION_TABLE ? VOTE_TWO_THIRDS : VOTE_SIMPLE;
}

int votes_needed(VoteRule rule, int present) {
    return rule == VOTE_TWO_THIRDS ? two_thirds(present) : simple_majority(present);
}

bool motion_carries(VoteRule rule, int yes, int no) {
    if (yes < 1) return false;
    // a negative count is no votes at all
    long long against = no > 0 ? no : 0;
    if (rule == VOTE_TWO_THIRDS) return yes >= 2 * against;
    return yes > against;
}

// something besides spaces in a buffer of size bytes
static bool has_text(const char *text, size_t size) {
    size_t length = held_length(text, size);
    for (size_t i = 0; i < length; i++) {
        if (!isspace((unsigned char)text[i])) return true;
    }
    return false;
}

bool check_motion(const Motion *motion, char *error, int error_size) {
    if (motion == NULL || known_type(motion->type) != motion->type) return fail(error, error_size, "Pick a kind of motion");
    switch (motion->type) {
        case MOTION_UNMOD:
        case MOTION_CONSULT:
        case MOTION_EXTEND:
            if (motion->total <= 0) return fail(error, error_size, "Add a length like 10");
            break;
        case MOTION_MOD:
            if (motion->total <= 0 || motion->speaker <= 0) return fail(error, error_size, "Add a length like 9:45");
            if (motion->speaker > motion->total) return fail(error, error_size, "Speaking time is longer than the caucus");
            break;
        case MOTION_ROUND_ROBIN:
        case MOTION_SPEAKING_TIME:
            if (motion->speaker <= 0) return fail(error, error_size, "Add a speaking time like 1:30");
            break;
        case MOTION_INTRODUCE:
        case MOTION_OTHER:
            if (!has_text(motion->topic, sizeof(motion->topic))) return fail(error, error_size, "Say what the motion is about");
            break;
        default:
            break;
    }
    copy_text(error, error_size, "");
    return true;
}

void describe_motion(const Motion *motion, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    buffer[0] = '\0';
    if (motion == NULL) return;

    MotionType type = known_type(motion->type);
    Duration duration = {motion->total, motion->speaker};
    char length[48];
    char kind[96] = "";
    switch (type) {
        case MOTION_MOD: {
            int slots = speaker_slots(duration);
            format_duration(duration, length, sizeof(length));
            snprintf(kind, sizeof(kind), "Mod %s (%d %s)", length, slots, slots == 1 ? "speaker" : "speakers");
            break;
        }
        case MOTION_UNMOD:
        case MOTION_CONSULT:
        case MOTION_EXTEND:
            format_span(motion->total, length, sizeof(length));
            snprintf(kind, sizeof(kind), "%s %s", motion_label(type), length);
            break;
        case MOTION_ROUND_ROBIN:
            format_clock(motion->speaker, length, sizeof(length));
            snprintf(kind, sizeof(kind), "Round robin %s each", length);
            break;
        case MOTION_SPEAKING_TIME:
            format_clock(motion->speaker, length, sizeof(length));
            snprintf(kind, sizeof(kind), "Speaking time %s", length);
            break;
        case MOTION_OTHER:
            break;
        default:
            copy_text(kind, sizeof(kind), motion_label(type));
            break;
    }

    static const char separator[] = " · ";
    const char *parts[3] = {kind, motion->topic, motion->proposer};
    size_t limits[3] = {sizeof(kind), sizeof(motion->topic), sizeof(motion->proposer)};
    char line[sizeof(kind) + TOPIC_SIZE + NAME_SIZE + 16];
    size_t used = 0;
    for (int i = 0; i < 3; i++) {
        size_t part = held_length(parts[i], limits[i]);
        size_t gap = used > 0 ? sizeof(separator) - 1 : 0;
        if (part == 0 || used + gap + part >= sizeof(line)) continue;
        memcpy(line + used, separator, gap);
        memcpy(line + used + gap, parts[i], part);
        used += gap + part;
    }
    line[used] = '\0';
    // an OTHER with nothing to show still says what it is
    if (used == 0) copy_text(line, sizeof(line), motion_label(type));
    // copy_text never leaves half of a '·' or a name at a cut
    copy_text(buffer, size, line);
}

// splits text on whitespace
static void split_words(const char *text, WordList *words) {
    words->count = 0;
    if (text == NULL) return;
    const char *p = text;
    while (words->count < MAX_WORDS) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') return;
        const char *start = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        words->items[words->count++] = (Word){start, (size_t)(p - start)};
    }
}

// equal to known, a lowercase ASCII word, ignoring case
static bool is_word(const Word *word, const char *known) {
    size_t length = strlen(known);
    if (word->length != length) return false;
    for (size_t i = 0; i < length; i++) {
        if (tolower((unsigned char)word->start[i]) != (unsigned char)known[i]) return false;
    }
    return true;
}

static bool word_in(const Word *word, const char *const *list, int count) {
    for (int i = 0; i < count; i++) {
        if (is_word(word, list[i])) return true;
    }
    return false;
}

// the kind a word names, -1 when it names none
static int find_kind(const Word *word) {
    for (int i = 0; i < COUNT_OF(KIND_WORDS); i++) {
        if (is_word(word, KIND_WORDS[i].word)) return (int)KIND_WORDS[i].type;
    }
    return -1;
}

// the word that may follow a kind word as part of the kind: "moderated caucus", "round robin", "close debate"
static const char *second_word(MotionType type) {
    switch (type) {
        case MOTION_MOD:
        case MOTION_UNMOD:
        case MOTION_EXTEND: return "caucus";
        case MOTION_ROUND_ROBIN: return "robin";
        case MOTION_SPEAKING_TIME: return "time";
        case MOTION_CLOSE_DEBATE:
        case MOTION_TABLE: return "debate";
        default: return NULL;
    }
}

// a length starts with a digit: "10", "9:45", "10m45s", ".5"
static bool starts_number(const Word *word) {
    if (isdigit((unsigned char)word->start[0])) return true;
    return word->length > 1 && word->start[0] == '.' && isdigit((unsigned char)word->start[1]);
}

// digits with at most one point and nothing else: "10", "7.5"
static bool is_plain_number(const Word *word) {
    int digits = 0;
    int points = 0;
    for (size_t i = 0; i < word->length; i++) {
        char c = word->start[i];
        if (c == '.') points++;
        else if (isdigit((unsigned char)c)) digits++;
        else return false;
    }
    return digits > 0 && points <= 1;
}

// words the length at words[at] takes: one, or two when a plain number has its unit typed apart
static int length_words(const WordList *words, int at, bool speaking) {
    if (at + 1 >= words->count || !is_plain_number(&words->items[at])) return 1;
    const Word *unit = &words->items[at + 1];
    if (word_in(unit, UNIT_WORDS, COUNT_OF(UNIT_WORDS))) return 2;
    if (speaking && is_word(unit, "second")) return 2;
    return 1;
}

// words[from] to words[from + count - 1] joined by single spaces, false when they don't fit
static bool join_words(const WordList *words, int from, int count, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (int i = from; i < from + count && i < words->count; i++) {
        const Word *word = &words->items[i];
        size_t gap = used > 0 ? 1 : 0;
        if (word->length >= size - used - gap) return false;
        if (gap > 0) out[used++] = ' ';
        memcpy(out + used, word->start, word->length);
        used += word->length;
        out[used] = '\0';
    }
    return true;
}

// adds word after a space, cutting it short when text is full
static void append_word(char *text, size_t size, const Word *word) {
    size_t used = strlen(text);
    if (used > 0 && used + 1 < size) text[used++] = ' ';
    size_t room = size - 1 - used;
    size_t length = word->length < room ? word->length : room;
    memcpy(text + used, word->start, length);
    text[used + length] = '\0';
}

// count kept inside the array in case a caller wrote a bad one
static int delegate_count(const Roster *roster) {
    if (roster->count < 0) return 0;
    return roster->count < MAX_DELEGATES ? roster->count : MAX_DELEGATES;
}

// roster index of a delegate called by the name or any alias of COUNTRIES[country], -1 when none
static int country_delegate(const Roster *roster, int country) {
    int index = find_delegate(roster, COUNTRIES[country].name);
    const char *alias = COUNTRIES[country].aliases;
    while (index < 0 && alias != NULL && *alias != '\0') {
        size_t length = strcspn(alias, "|");
        char name[NAME_SIZE * 2];
        if (length < sizeof(name)) {
            memcpy(name, alias, length);
            name[length] = '\0';
            index = find_delegate(roster, name);
        }
        alias += length;
        if (*alias == '|') alias++;
    }
    return index;
}

// the one delegate whose folded name starts with word folded (3 letters or more, so "fra" is France),
// -1 when none or several
static int prefix_delegate(const Roster *roster, const char *word) {
    char typed[NAME_SIZE * 2];
    fold_name(word, typed, sizeof(typed));
    size_t length = strlen(typed);
    if (length < 3) return -1;

    int found = -1;
    int total = delegate_count(roster);
    for (int i = 0; i < total; i++) {
        char name[NAME_SIZE * 2];
        fold_name(roster->items[i].name, name, sizeof(name));
        if (strncmp(name, typed, length) != 0) continue;
        if (found >= 0) return -1;
        found = i;
    }
    return found;
}

// roster index of the delegate named by the longest run of up to MAX_RUN words from words[from],
// with the run's word count in *run. -1 when no run names one
static int find_proposer(const Roster *roster, const WordList *words, int from, int *run) {
    *run = 0;
    if (roster == NULL || from < 0 || from >= words->count) return -1;
    int most = words->count - from < MAX_RUN ? words->count - from : MAX_RUN;
    for (int count = most; count >= 1; count--) {
        char name[NAME_SIZE * 2];
        if (!join_words(words, from, count, name, sizeof(name))) continue;
        int index = find_delegate(roster, name);
        if (index < 0) {
            int country = find_country(name);
            if (country >= 0) index = country_delegate(roster, country);
        }
        // exact names of every length first, a prefix only for a single word
        if (index < 0 && count == 1) index = prefix_delegate(roster, name);
        if (index >= 0) {
            *run = count;
            return index;
        }
    }
    return -1;
}

// the length of an unmod, consultation or extension is one span, so "7:30" is 7 min 30 s here
// (parse_caucus would read a mod's 7 minutes at 30 s each). other forms go to parse_caucus
static bool parse_total(const char *text, int *seconds, char *error, int error_size) {
    const char *colon = strchr(text, ':');
    if (colon == NULL) {
        Duration duration = {0};
        if (!parse_caucus(text, &duration, error, error_size)) return false;
        *seconds = duration.total;
        return true;
    }
    long minutes = 0;
    const char *c = text;
    if (!isdigit((unsigned char)*c)) return fail(error, error_size, "Can't read that length");
    for (; c < colon; c++) {
        if (!isdigit((unsigned char)*c)) return fail(error, error_size, "Can't read that length");
        if (minutes < 100000) minutes = minutes * 10 + (*c - '0');
    }
    const char *rest = colon + 1;
    size_t digits = strlen(rest);
    if (digits < 1 || digits > 2 || !isdigit((unsigned char)rest[0]) || (digits == 2 && !isdigit((unsigned char)rest[1]))) {
        return fail(error, error_size, "Can't read that length");
    }
    int extra = atoi(rest);
    if (extra >= 60) return fail(error, error_size, "Can't read that length");
    long total = minutes * 60 + extra;
    if (total <= 0) return fail(error, error_size, "Length can't be zero");
    if (total > MAX_CAUCUS_SECONDS) return fail(error, error_size, "Longer than 4 hours");
    *seconds = (int)total;
    return true;
}

// "Unknown motion "xyz": try mod 9:45 or unmod 10", with a long word cut short
static bool fail_unknown(const Word *word, char *error, int error_size) {
    char typed[NAME_SIZE];
    size_t length = word->length < sizeof(typed) - 1 ? word->length : sizeof(typed) - 1;
    memcpy(typed, word->start, length);
    typed[length] = '\0';
    char shown[24];
    copy_text(shown, sizeof(shown), typed);
    char message[ERROR_SIZE];
    snprintf(message, sizeof(message), "Unknown motion \"%s\": try mod 9:45 or unmod 10", shown);
    return fail(error, error_size, message);
}

bool parse_motion_line(const char *text, const Roster *roster, Motion *out, char *error, int error_size) {
    WordList words;
    split_words(text, &words);
    if (words.count == 0) return fail(error, error_size, "Type a motion like mod 9:45");

    // the kind, one or two words. "unmod" or "mod" may come anywhere ("10 unmod usa"); without a kind it is a mod
    Motion motion = {.type = MOTION_OTHER};
    if (find_kind(&words.items[0]) < 0) {
        static const char *const anywhere[] = {"unmod", "unmoderated", "mod", "moderated"};
        for (int i = 1; i < words.count; i++) {
            if (!word_in(&words.items[i], anywhere, COUNT_OF(anywhere))) continue;
            // the kind moves to the front, with a "caucus" after it
            int taken = i + 1 < words.count && is_word(&words.items[i + 1], "caucus") ? 2 : 1;
            Word kind[2];
            memcpy(kind, &words.items[i], (size_t)taken * sizeof(Word));
            memmove(&words.items[taken], &words.items[0], (size_t)i * sizeof(Word));
            memcpy(&words.items[0], kind, (size_t)taken * sizeof(Word));
            break;
        }
    }
    const Word *first = &words.items[0];
    int kind = find_kind(first);
    bool implicit = kind < 0;
    int at = 0;
    if (!implicit) {
        motion.type = (MotionType)kind;
        at = 1;
        const char *second = second_word(motion.type);
        if (second != NULL && at < words.count && is_word(&words.items[at], second)) at++;
    }

    // the proposer may come before the length ("france 9:45 trade"): the length moves in front of it
    bool caucus = implicit || motion_uses_total(motion.type);
    bool speaking = !caucus && motion_uses_speaker(motion.type);
    if ((caucus || speaking) && at < words.count && !starts_number(&words.items[at])) {
        int run = 0;
        if (find_proposer(roster, &words, at, &run) >= 0 && at + run < words.count && starts_number(&words.items[at + run])) {
            int taken = length_words(&words, at + run, speaking);
            Word length[2];
            memcpy(length, &words.items[at + run], (size_t)taken * sizeof(Word));
            memmove(&words.items[at + taken], &words.items[at], (size_t)run * sizeof(Word));
            memcpy(&words.items[at], length, (size_t)taken * sizeof(Word));
        }
        else if (implicit) return fail_unknown(first, error, error_size);
    }

    // the length. when the word after the kind is no number it is missing and check_motion says so
    if ((caucus || speaking) && at < words.count && starts_number(&words.items[at])) {
        int taken = length_words(&words, at, speaking);
        char length[64];
        if (!join_words(&words, at, taken, length, sizeof(length))) {
            return fail(error, error_size, caucus ? "Can't read that length" : "Can't read that time");
        }
        if (caucus && !implicit && motion.type != MOTION_MOD) {
            if (!parse_total(length, &motion.total, error, error_size)) return false;
        }
        else if (caucus) {
            Duration duration = {0};
            if (!parse_caucus(length, &duration, error, error_size)) return false;
            if (implicit) motion.type = MOTION_MOD;
            if (motion.type == MOTION_MOD && duration.speaker <= 0) {
                return fail(error, error_size, "Add a speaking time, e.g. 9:45");
            }
            motion.total = duration.total;
            // an unmod, consultation or extension has no speaking time, whatever was typed
            if (motion.type == MOTION_MOD) motion.speaker = duration.speaker;
        }
        else if (!parse_speaking(length, &motion.speaker, error, error_size)) return false;
        at += taken;
    }

    // the proposer: after a "by" first, then right after the length
    int proposer = -1;
    int skip = 0;           // words[skip] up to words[skip_end] are the proposer, "by" included
    int skip_end = 0;
    for (int i = at; i + 1 < words.count && proposer < 0; i++) {
        if (!is_word(&words.items[i], "by")) continue;
        int run = 0;
        proposer = find_proposer(roster, &words, i + 1, &run);
        if (proposer >= 0) {
            skip = i;
            skip_end = i + 1 + run;
        }
    }
    if (proposer < 0) {
        int run = 0;
        proposer = find_proposer(roster, &words, at, &run);
        if (proposer >= 0) {
            skip = at;
            skip_end = at + run;
        }
    }
    if (proposer >= 0) copy_text(motion.proposer, NAME_SIZE, roster->items[proposer].name);

    // the topic: every other word. "dr" and "wp" also say what kind of document it names
    char topic[TOPIC_SIZE * 2] = "";
    bool document = motion.type == MOTION_INTRODUCE && (is_word(first, "dr") || is_word(first, "wp"));
    if (document) append_word(topic, sizeof(topic), first);
    size_t kind_length = strlen(topic);
    bool leading = true;
    for (int i = at; i < words.count; i++) {
        if (i >= skip && i < skip_end) continue;
        if (leading) {
            leading = false;
            if (is_word(&words.items[i], "on")) continue;
        }
        append_word(topic, sizeof(topic), &words.items[i]);
    }
    // a bare "dr" names no document
    if (strlen(topic) == kind_length) topic[0] = '\0';
    copy_text(motion.topic, TOPIC_SIZE, topic);
    // a cut at the joining space (or at a character it couldn't split) leaves that space at the end
    trim_text(motion.topic);

    if (!check_motion(&motion, error, error_size)) return false;
    if (out != NULL) *out = motion;
    return true;
}

// count kept inside the array in case a caller wrote a bad one
static int motion_count(const MotionList *list) {
    if (list->count < 0) return 0;
    return list->count < MAX_MOTIONS ? list->count : MAX_MOTIONS;
}

void clear_motions(MotionList *list) {
    memset(list->items, 0, sizeof(list->items));
    list->count = 0;
    // serials keep counting up, so every motion of a session has its own
    if (list->serial < 0) list->serial = 0;
}

int add_motion(MotionList *list, const Motion *motion) {
    int count = motion_count(list);
    if (motion == NULL || count >= MAX_MOTIONS) return -1;

    // above every serial in the list, also when the counter fell behind (a list restored without it)
    long long serial = list->serial > 0 ? list->serial : 0;
    for (int i = 0; i < count; i++) {
        if (list->items[i].serial >= serial) serial = (long long)list->items[i].serial + 1;
    }
    if (serial >= INT_MAX) serial = 0;

    list->items[count] = *motion;
    list->items[count].serial = (int)serial;
    list->serial = (int)serial + 1;
    list->count = count + 1;
    return count;
}

bool remove_motion(MotionList *list, int index) {
    int count = motion_count(list);
    if (index < 0 || index >= count) return false;
    memmove(&list->items[index], &list->items[index + 1], (size_t)(count - index - 1) * sizeof(Motion));
    list->count = count - 1;
    list->items[list->count] = (Motion){0};
    return true;
}

bool move_motion(MotionList *list, int index, int delta) {
    int count = motion_count(list);
    long long target = (long long)index + delta;
    if (delta == 0 || index < 0 || index >= count || target < 0 || target >= count) return false;
    Motion moved = list->items[index];
    list->items[index] = list->items[target];
    list->items[target] = moved;
    return true;
}

// -1, 0 or 1 like a comparison of a and b
static int order(long long a, long long b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

int compare_motions(const Motion *a, const Motion *b) {
    MotionType type = known_type(a->type);
    MotionType other = known_type(b->type);
    if (type != other) return order(type, other);
    // the longer caucus first, and between mods of one length the one with more speakers
    if (motion_uses_total(type) && a->total != b->total) return order(b->total, a->total);
    if (type == MOTION_MOD && a->speaker != b->speaker) return order(a->speaker, b->speaker);
    return order(a->serial, b->serial);
}

void sort_motions(MotionList *list) {
    int count = motion_count(list);
    // insertion sort: stable, and the list is short
    for (int i = 1; i < count; i++) {
        Motion moving = list->items[i];
        int at = i;
        while (at > 0 && compare_motions(&moving, &list->items[at - 1]) < 0) {
            list->items[at] = list->items[at - 1];
            at--;
        }
        list->items[at] = moving;
    }
}
