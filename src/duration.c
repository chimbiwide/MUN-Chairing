#include <duration.h>
#include <text.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_NUMBERS 3

// one number as typed, with the unit written after it and the separator in front of it
typedef struct {
    double value;
    bool whole;         // written without a decimal point
    bool padded;        // written with a leading zero, like the "05" of "10:05"
    int unit;           // 60 minutes, 1 seconds, 0 when none was written
    char separator;     // ':' '/' ',' or ' ' for spaces only, 0 when it follows a unit directly
} Number;

typedef struct {
    Number items[MAX_NUMBERS];
    int count;
} NumberList;

static const struct {
    const char *word;
    int seconds;
} units[] = {
    {"m", 60}, {"min", 60}, {"mins", 60}, {"minute", 60}, {"minutes", 60},
    {"s", 1}, {"sec", 1}, {"secs", 1}, {"second", 1}, {"seconds", 1},
};

// writes message for the chair and gives false, so callers can return it
static bool fail(char *error, int error_size, const char *message) {
    copy_text(error, error_size, message);
    return false;
}

static bool is_separator(char c) {
    return c == ':' || c == '/' || c == ',' || isspace((unsigned char)c);
}

// 60 for a minutes word, 1 for a seconds word, 0 for anything else. case doesn't matter
static int unit_seconds(const char *word, size_t len) {
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        const char *name = units[i].word;
        if (strlen(name) != len) continue;
        size_t k = 0;
        while (k < len && tolower((unsigned char)word[k]) == name[k]) k++;
        if (k == len) return units[i].seconds;
    }
    return 0;
}

// reads digits with at most one '.' and moves *p past them. false on a second point or no digits.
// past 1e9 the value stops growing: that is over every limit anyway.
// fraction digits count up to the 15th, as far as a double holds them exactly
static bool read_number(const char **p, Number *number) {
    double value = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    int digits = 0;
    int points = 0;
    const char *c = *p;
    bool padded = c[0] == '0' && isdigit((unsigned char)c[1]);
    for (; isdigit((unsigned char)*c) || *c == '.'; c++) {
        if (*c == '.') {
            points++;
            continue;
        }
        int digit = *c - '0';
        digits++;
        if (points == 0) {
            if (value < 1e9) value = value * 10.0 + digit;
        }
        else if (divisor < 1e15) {
            fraction = fraction * 10.0 + digit;
            divisor *= 10.0;
        }
    }
    *p = c;
    if (points > 1 || digits == 0) return false;
    *number = (Number){.value = value + fraction / divisor, .whole = points == 0, .padded = padded && points == 0};
    return true;
}

// splits text into numbers, their units and the separators between them.
// false when anything else is in it. blank text gives no numbers
static bool split_numbers(const char *text, NumberList *list) {
    list->count = 0;
    if (text == NULL) return true;

    char separator = 0;     // what came since the last number or unit
    const char *p = text;
    while (*p != '\0') {
        if (is_separator(*p)) {
            int marks = 0;
            separator = ' ';
            for (; *p != '\0' && is_separator(*p); p++) {
                if (isspace((unsigned char)*p)) continue;
                separator = *p;
                marks++;
            }
            if (marks > 1) return false;
            if (list->count == 0 && separator != ' ') return false;
        }
        else if (isdigit((unsigned char)*p) || *p == '.') {
            if (list->count >= MAX_NUMBERS) return false;
            Number *number = &list->items[list->count];
            if (!read_number(&p, number)) return false;
            number->separator = list->count > 0 ? separator : 0;
            list->count++;
            separator = 0;
        }
        else if (isalpha((unsigned char)*p)) {
            const char *word = p;
            while (isalpha((unsigned char)*p)) p++;
            int unit = unit_seconds(word, (size_t)(p - word));
            if (unit == 0 || list->count == 0) return false;
            // a unit follows its own number, directly or after spaces
            Number *number = &list->items[list->count - 1];
            if (number->unit != 0 || (separator != 0 && separator != ' ')) return false;
            number->unit = unit;
            separator = 0;
        }
        else return false;
    }
    // a separator left at the end has nothing after it and is dropped
    return true;
}

// a speaking time from count numbers: one number with an optional unit (bare under 10 means minutes,
// unless written with a leading zero like "05"), m:ss ("1:30"), or minutes then seconds ("1m30s",
// "1 min 30"). false when it is none of these
static bool read_time(const Number *numbers, int count, double *seconds) {
    if (count == 1) {
        const Number *only = &numbers[0];
        int unit = only->unit != 0 ? only->unit : (only->value < 10.0 && !only->padded) ? 60 : 1;
        *seconds = only->value * unit;
        return true;
    }
    if (count != 2) return false;

    const Number *minutes = &numbers[0];
    const Number *rest = &numbers[1];
    bool clock_face = minutes->unit == 0 && rest->unit == 0 && rest->separator == ':';
    bool spelled = minutes->unit == 60 && rest->unit != 60 &&
                   (rest->separator == 0 || rest->separator == ' ' || rest->separator == ',');
    if (!clock_face && !spelled) return false;
    if (!minutes->whole || !rest->whole || rest->value >= 60.0) return false;
    *seconds = minutes->value * 60.0 + rest->value;
    return true;
}

// whole seconds, capped far above every limit so the conversion stays defined
static int round_seconds(double seconds) {
    if (seconds >= 1e9) return 1000000000;
    return (int)lround(seconds);
}

bool parse_caucus(const char *text, Duration *out, char *error, int error_size) {
    NumberList list;
    if (!split_numbers(text, &list)) return fail(error, error_size, "Can't read that length");
    if (list.count == 0) return fail(error, error_size, "Type a length like 9:45");

    const Number *total = &list.items[0];
    bool has_speaker = list.count > 1;
    double speaker = 0.0;
    if (has_speaker && !read_time(&list.items[1], list.count - 1, &speaker)) {
        return fail(error, error_size, "Can't read that length");
    }

    // a bare "945" is far more likely 9:45 than 945 minutes. asked only when the suggestion itself
    // would work: it fits in 4 hours and its speaking time isn't zero ("100" stays 100 minutes)
    if (!has_speaker && total->unit == 0 && total->whole && total->value >= 100.0 &&
        total->value < (MAX_CAUCUS_SECONDS / 60 + 1) * 100.0) {
        int digits = (int)total->value;
        if (digits % 100 >= 1 && digits % 100 < 60) {
            char message[ERROR_SIZE];
            snprintf(message, sizeof(message), "Did you mean %d:%02d?", digits / 100, digits % 100);
            return fail(error, error_size, message);
        }
    }

    int total_seconds = round_seconds(total->value * (total->unit != 0 ? total->unit : 60));
    int speaker_seconds = round_seconds(speaker);
    if (total_seconds == 0) return fail(error, error_size, "Length can't be zero");
    if (has_speaker && speaker_seconds == 0) return fail(error, error_size, "Speaking time can't be zero");
    if (total_seconds > MAX_CAUCUS_SECONDS) return fail(error, error_size, "Longer than 4 hours");
    if (speaker_seconds > total_seconds) return fail(error, error_size, "Speaking time is longer than the caucus");

    if (out != NULL) *out = (Duration){.total = total_seconds, .speaker = speaker_seconds};
    return true;
}

bool parse_speaking(const char *text, int *seconds, char *error, int error_size) {
    NumberList list;
    if (!split_numbers(text, &list)) return fail(error, error_size, "Can't read that time");
    if (list.count == 0) return fail(error, error_size, "Type a time like 1:30");

    double value = 0.0;
    if (!read_time(list.items, list.count, &value)) return fail(error, error_size, "Can't read that time");
    int result = round_seconds(value);
    if (result == 0) return fail(error, error_size, "Time can't be zero");
    if (result > MAX_SPEAKING_SECONDS) return fail(error, error_size, "Longer than 30 minutes");

    if (seconds != NULL) *seconds = result;
    return true;
}

int speaker_slots(Duration duration) {
    if (duration.speaker <= 0 || duration.total <= 0) return 0;
    return duration.total / duration.speaker;
}

int slot_leftover(Duration duration) {
    if (duration.speaker <= 0 || duration.total <= 0) return 0;
    return duration.total % duration.speaker;
}

void format_clock(int seconds, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    long long left = seconds;
    const char *sign = left < 0 ? "-" : "";
    if (left < 0) left = -left;
    if (left >= 3600) {
        snprintf(buffer, (size_t)size, "%s%lld:%02lld:%02lld", sign, left / 3600, left / 60 % 60, left % 60);
    }
    else snprintf(buffer, (size_t)size, "%s%lld:%02lld", sign, left / 60, left % 60);
}

// a whole number of seconds held in a double, capped so the conversion stays defined
static int whole_seconds(double seconds) {
    if (seconds >= (double)INT_MAX) return INT_MAX;
    return (int)seconds;
}

void format_left(double left, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    if (left > 0.0) format_clock(whole_seconds(ceil(left)), buffer, size);
    else if (left <= -1.0) {
        char over[32];
        format_clock(whole_seconds(floor(-left)), over, sizeof(over));
        snprintf(buffer, (size_t)size, "+%s", over);
    }
    else copy_text(buffer, size, "0:00");   // from zero to just under a second over, and NaN
}

// adds "5 min" to a span, after a space when something is already there
static void add_part(char *text, size_t size, long long amount, const char *unit) {
    if (amount == 0) return;
    size_t used = strlen(text);
    snprintf(text + used, size - used, "%s%lld %s", used > 0 ? " " : "", amount, unit);
}

void format_span(int seconds, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    long long left = seconds < 0 ? -(long long)seconds : seconds;
    char text[64] = "";
    add_part(text, sizeof(text), left / 3600, "h");
    add_part(text, sizeof(text), left / 60 % 60, "min");
    add_part(text, sizeof(text), left % 60, "s");
    snprintf(buffer, (size_t)size, "%s%s", seconds < 0 ? "-" : "", text[0] != '\0' ? text : "0 s");
}

void format_duration(Duration duration, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    int total = duration.total;
    int speaker = duration.speaker;
    char text[64];

    // from 100 minutes on the unit is written, or "240" would read back as a missing colon
    if (total % 60 == 0) snprintf(text, sizeof(text), "%d%s", total / 60, total >= 100 * 60 ? "m" : "");
    else if (total > 0 && total % 6 == 0) snprintf(text, sizeof(text), "%d.%d", total / 60, total % 60 / 6);
    else snprintf(text, sizeof(text), "%ds", total);

    if (speaker > 0) {
        size_t used = strlen(text);
        char *end = text + used;
        size_t room = sizeof(text) - used;
        if (speaker % 60 == 0) snprintf(end, room, ":%d%s", speaker / 60, speaker >= 10 * 60 ? "m" : "");
        else if (speaker < 10) snprintf(end, room, ":%ds", speaker);
        else if (speaker < 60) snprintf(end, room, ":%d", speaker);
        else snprintf(end, room, ":%d:%02d", speaker / 60, speaker % 60);
    }
    copy_text(buffer, size, text);
}

void describe_duration(Duration duration, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    char total[64];
    format_span(duration.total, total, sizeof(total));
    if (duration.speaker <= 0) {
        copy_text(buffer, size, total);
        return;
    }

    char speaker[64];
    format_span(duration.speaker, speaker, sizeof(speaker));
    int slots = speaker_slots(duration);
    char line[256];
    snprintf(line, sizeof(line), "%s total · %s each · %d %s",
             total, speaker, slots, slots == 1 ? "speaker" : "speakers");

    int leftover = slot_leftover(duration);
    if (leftover > 0) {
        char rest[64];
        format_span(leftover, rest, sizeof(rest));
        size_t used = strlen(line);
        snprintf(line + used, sizeof(line) - used, " · %s left over", rest);
    }
    // copy_text never leaves half of a '·' at a cut
    copy_text(buffer, size, line);
}
