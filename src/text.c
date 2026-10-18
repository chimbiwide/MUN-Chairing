#include <text.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void copy_text(char *target, int size, const char *source) {
    if (target == NULL || size <= 0) return;
    if (source == NULL) source = "";
    snprintf(target, (size_t)size, "%s", source);
    size_t len = strlen(target);
    if (len == strlen(source)) return;

    // cut short: drop a UTF-8 sequence that lost its tail
    size_t lead = len;
    while (lead > 0 && ((unsigned char)target[lead - 1] & 0xC0) == 0x80) lead--;
    if (lead == 0) return;
    unsigned char first = (unsigned char)target[lead - 1];
    size_t need = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : first >= 0xC0 ? 2 : 1;
    if (len - (lead - 1) < need) target[lead - 1] = '\0';
}

void trim_text(char *text) {
    if (text == NULL) return;
    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) start++;
    size_t len = strlen(text + start);
    memmove(text, text + start, len + 1);
    while (len > 0 && isspace((unsigned char)text[len - 1])) text[--len] = '\0';
}

void flatten_text(char *text) {
    if (text == NULL) return;
    for (char *c = text; *c != '\0'; c++) {
        if (*c == '\t' || *c == '\r' || *c == '\n') *c = ' ';
    }
}

// one UTF-8 sequence. bytes that don't form one come back as themselves
static int decode_utf8(const unsigned char *s, int *len) {
    int cp = s[0];
    int extra = 0;
    if (cp >= 0xF0 && cp <= 0xF4) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC2 && cp <= 0xDF) { cp &= 0x1F; extra = 1; }
    if (s[0] >= 0x80 && extra == 0) {
        *len = 1;
        return -1;
    }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *len = 1;
            return -1;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *len = extra + 1;
    return cp;
}

// U+00C0..U+00FF, '-' marks the two signs that are not letters
static const char *latin1[64] = {
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "-", "o", "u", "u", "u", "u", "y", "th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "-", "o", "u", "u", "u", "u", "y", "th", "y",
};

// U+0100..U+017F
static const char latin_ext_a[] =
    "aaaaaaccccccccdddd"
    "eeeeeeeeeegggggggghhhh"
    "iiiiiiiiiiiijjkkk"
    "lllllllllln"
    "nnnnnnnnoooooooorrrrrr"
    "sssssssstttttt"
    "uuuuuuuuuuuuwwyyy"
    "zzzzzzs";

// what a non-ASCII codepoint folds to: letters, "" to drop it, " " to split words, NULL to keep it
static const char *fold_codepoint(int cp) {
    static char letter[2];
    if (cp >= 0xC0 && cp <= 0xFF) {
        const char *folded = latin1[cp - 0xC0];
        return folded[0] == '-' ? " " : folded;
    }
    if (cp >= 0x100 && cp <= 0x17F && (size_t)(cp - 0x100) < sizeof(latin_ext_a) - 1) {
        letter[0] = latin_ext_a[cp - 0x100];
        letter[1] = '\0';
        return letter;
    }
    if (cp == 0x2018 || cp == 0x2019 || cp == 0xB4 || cp == 0x60) return "";
    if (cp == 0xA0 || (cp >= 0x2010 && cp <= 0x2015)) return " ";
    if (cp >= 0x80 && cp < 0xC0) return " ";
    return NULL;
}

void fold_name(const char *text, char *out, int size) {
    if (out == NULL || size <= 0) return;
    out[0] = '\0';
    if (text == NULL) return;

    int n = 0;
    bool gap = false;
    const unsigned char *p = (const unsigned char *)text;
    while (*p != '\0') {
        int len = 1;
        int cp = decode_utf8(p, &len);
        const char *folded = NULL;
        char ascii[2] = {0};

        if (cp >= 0 && cp < 0x80) {
            if (isalnum(cp)) {
                ascii[0] = (char)tolower(cp);
                folded = ascii;
            }
            else if (cp == '\'' || cp == '.') folded = "";
            else folded = " ";
        }
        else if (cp >= 0x80) folded = fold_codepoint(cp);

        if (folded != NULL && folded[0] == ' ') {
            gap = n > 0;
        }
        else if (folded == NULL || folded[0] != '\0') {
            const char *bytes = folded != NULL ? folded : (const char *)p;
            int count = folded != NULL ? (int)strlen(folded) : len;
            if (gap && n + 1 < size) out[n++] = ' ';
            gap = false;
            if (n + count >= size) break;
            memcpy(out + n, bytes, (size_t)count);
            n += count;
        }
        p += len;
    }
    out[n] = '\0';
}

bool same_name(const char *a, const char *b) {
    char fa[NAME_SIZE * 2];
    char fb[NAME_SIZE * 2];
    fold_name(a, fa, sizeof(fa));
    fold_name(b, fb, sizeof(fb));
    return strcmp(fa, fb) == 0;
}

static bool small_word(const char *word, size_t len) {
    return (len == 2 && strncmp(word, "of", 2) == 0) ||
           (len == 3 && strncmp(word, "the", 3) == 0) ||
           (len == 3 && strncmp(word, "and", 3) == 0);
}

int match_name(const char *name, const char *query) {
    char folded[NAME_SIZE * 2];
    char typed[NAME_SIZE * 2];
    fold_name(name, folded, sizeof(folded));
    fold_name(query, typed, sizeof(typed));
    if (folded[0] == '\0' || typed[0] == '\0') return 0;

    if (strcmp(folded, typed) == 0) return 100;
    size_t typed_len = strlen(typed);
    if (strncmp(folded, typed, typed_len) == 0) return 80;

    char initials[NAME_SIZE];
    int count = 0;
    const char *word = folded;
    while (*word != '\0' && count + 1 < (int)sizeof(initials)) {
        size_t len = strcspn(word, " ");
        if (!small_word(word, len)) initials[count++] = word[0];
        word += len;
        while (*word == ' ') word++;
    }
    initials[count] = '\0';
    if (count >= 2 && strcmp(initials, typed) == 0) return 70;

    for (const char *c = folded + 1; *c != '\0'; c++) {
        if (c[-1] == ' ' && strncmp(c, typed, typed_len) == 0) return 60;
    }
    if (strstr(folded, typed) != NULL) return 40;
    return 0;
}
