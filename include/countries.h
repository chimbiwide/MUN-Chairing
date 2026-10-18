#ifndef COUNTRIES_H
#define COUNTRIES_H

#include <stdbool.h>

typedef struct {
    const char *name;       // the short name used on placards: "United States", "Côte d'Ivoire"
    const char *aliases;    // other names and abbreviations separated by '|': "USA|US|United States of America"
    bool p5;                // permanent member of the Security Council
} Country;

// the 193 UN member states followed by the observer states (Holy See, State of Palestine), alphabetical
extern const Country COUNTRIES[];
extern const int COUNTRY_COUNT;

// index into COUNTRIES whose name or alias is same_name as text, -1 when none
int find_country(const char *text);
bool is_p5(const char *name);
// match_name against name, and also against the aliases when name is a known country
// (an alias match scores 5 less than the same match on the name, never below 1)
int match_score(const char *name, const char *query);
// COUNTRIES indices that match query, best first (score, then shorter name, then alphabetical).
// returns how many were written, at most max
int search_countries(const char *query, int *out, int max);

#endif
