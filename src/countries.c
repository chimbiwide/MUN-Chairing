#include <countries.h>
#include <text.h>
#include <ctype.h>
#include <string.h>

// alphabetical by fold_name, the observer states among the rest. aliases: abbreviations, other common
// names and the name un.org lists where it differs (Nauru told the UN in June 2026 it is now Naoero)
const Country COUNTRIES[] = {
    {"Afghanistan", "", false},
    {"Albania", "", false},
    {"Algeria", "", false},
    {"Andorra", "", false},
    {"Angola", "", false},
    {"Antigua and Barbuda", "Antigua & Barbuda|Antigua", false},
    {"Argentina", "", false},
    {"Armenia", "", false},
    {"Australia", "", false},
    {"Austria", "", false},
    {"Azerbaijan", "", false},
    {"Bahamas", "The Bahamas|Bahamas (The)", false},
    {"Bahrain", "", false},
    {"Bangladesh", "", false},
    {"Barbados", "", false},
    {"Belarus", "", false},
    {"Belgium", "", false},
    {"Belize", "", false},
    {"Benin", "", false},
    {"Bhutan", "", false},
    {"Bolivia", "Plurinational State of Bolivia|Bolivia (Plurinational State of)", false},
    {"Bosnia and Herzegovina", "Bosnia|Bosnia & Herzegovina", false},
    {"Botswana", "", false},
    {"Brazil", "", false},
    {"Brunei", "Brunei Darussalam", false},
    {"Bulgaria", "", false},
    {"Burkina Faso", "", false},
    {"Burundi", "", false},
    {"Cabo Verde", "Cape Verde", false},
    {"Cambodia", "", false},
    {"Cameroon", "", false},
    {"Canada", "", false},
    {"Central African Republic", "CAR", false},
    {"Chad", "", false},
    {"Chile", "", false},
    {"China", "PRC|People's Republic of China|China (the People's Republic of)", true},
    {"Colombia", "", false},
    {"Comoros", "", false},
    {"Costa Rica", "", false},
    {"Côte d'Ivoire", "Ivory Coast|Cote d'Ivoire", false},
    {"Croatia", "", false},
    {"Cuba", "", false},
    {"Cyprus", "", false},
    {"Czechia", "Czech Republic", false},
    {"Democratic Republic of the Congo",
     "DRC|DR Congo|Congo-Kinshasa|Democratic Republic of Congo|Congo, Democratic Republic of the", false},
    {"Denmark", "", false},
    {"Djibouti", "", false},
    {"Dominica", "", false},
    {"Dominican Republic", "", false},
    {"Ecuador", "", false},
    {"Egypt", "Arab Republic of Egypt", false},
    {"El Salvador", "", false},
    {"Equatorial Guinea", "", false},
    {"Eritrea", "", false},
    {"Estonia", "", false},
    {"Eswatini", "Swaziland", false},
    {"Ethiopia", "", false},
    {"Fiji", "", false},
    {"Finland", "", false},
    {"France", "", true},
    {"Gabon", "", false},
    {"Gambia", "The Gambia|Gambia (Republic of The)", false},
    {"Georgia", "", false},
    {"Germany", "", false},
    {"Ghana", "", false},
    {"Greece", "", false},
    {"Grenada", "", false},
    {"Guatemala", "", false},
    {"Guinea", "", false},
    {"Guinea-Bissau", "", false},
    {"Guyana", "", false},
    {"Haiti", "", false},
    {"Holy See", "Vatican|Vatican City", false},
    {"Honduras", "", false},
    {"Hungary", "", false},
    {"Iceland", "", false},
    {"India", "", false},
    {"Indonesia", "", false},
    {"Iran", "Islamic Republic of Iran|Iran (Islamic Republic of)", false},
    {"Iraq", "", false},
    {"Ireland", "", false},
    {"Israel", "", false},
    {"Italy", "", false},
    {"Jamaica", "", false},
    {"Japan", "", false},
    {"Jordan", "", false},
    {"Kazakhstan", "", false},
    {"Kenya", "", false},
    {"Kiribati", "", false},
    {"Kuwait", "", false},
    {"Kyrgyzstan", "Kyrgyz Republic", false},
    {"Laos", "Lao PDR|Lao People's Democratic Republic", false},
    {"Latvia", "", false},
    {"Lebanon", "", false},
    {"Lesotho", "", false},
    {"Liberia", "", false},
    {"Libya", "", false},
    {"Liechtenstein", "", false},
    {"Lithuania", "", false},
    {"Luxembourg", "", false},
    {"Madagascar", "", false},
    {"Malawi", "", false},
    {"Malaysia", "", false},
    {"Maldives", "", false},
    {"Mali", "", false},
    {"Malta", "", false},
    {"Marshall Islands", "", false},
    {"Mauritania", "", false},
    {"Mauritius", "", false},
    {"Mexico", "", false},
    {"Micronesia", "Federated States of Micronesia|Micronesia (Federated States of)", false},
    {"Moldova", "Republic of Moldova|Moldova, Republic of", false},
    {"Monaco", "", false},
    {"Mongolia", "", false},
    {"Montenegro", "", false},
    {"Morocco", "", false},
    {"Mozambique", "", false},
    {"Myanmar", "Burma", false},
    {"Namibia", "", false},
    {"Nauru", "Naoero|Republic of Naoero", false},
    {"Nepal", "", false},
    {"Netherlands", "Holland|Kingdom of the Netherlands|The Netherlands|Netherlands (Kingdom of the)", false},
    {"New Zealand", "NZ", false},
    {"Nicaragua", "", false},
    {"Niger", "", false},
    {"Nigeria", "", false},
    {"North Korea", "DPRK|Democratic People's Republic of Korea|Korea, Democratic People's Republic of", false},
    {"North Macedonia", "Macedonia", false},
    {"Norway", "", false},
    {"Oman", "", false},
    {"Pakistan", "", false},
    {"Palau", "", false},
    {"Panama", "", false},
    {"Papua New Guinea", "PNG", false},
    {"Paraguay", "", false},
    {"Peru", "", false},
    {"Philippines", "The Philippines", false},
    {"Poland", "", false},
    {"Portugal", "", false},
    {"Qatar", "", false},
    {"Republic of the Congo", "Congo|Congo-Brazzaville|Republic of Congo|Congo, Republic of the", false},
    {"Romania", "", false},
    {"Russia", "Russian Federation", true},
    {"Rwanda", "", false},
    {"Saint Kitts and Nevis", "St Kitts and Nevis|St Kitts & Nevis|Saint Kitts & Nevis|St Kitts|Saint Kitts", false},
    {"Saint Lucia", "St Lucia", false},
    {"Saint Vincent and the Grenadines",
     "St Vincent|St Vincent and the Grenadines|St Vincent & the Grenadines|Saint Vincent & the Grenadines|"
     "Saint Vincent", false},
    {"Samoa", "", false},
    {"San Marino", "", false},
    {"São Tomé and Príncipe", "Sao Tome and Principe|Sao Tome & Principe|Sao Tome", false},
    {"Saudi Arabia", "KSA|Kingdom of Saudi Arabia", false},
    {"Senegal", "", false},
    {"Serbia", "", false},
    {"Seychelles", "", false},
    {"Sierra Leone", "", false},
    {"Singapore", "", false},
    {"Slovakia", "Slovak Republic", false},
    {"Slovenia", "", false},
    {"Solomon Islands", "", false},
    {"Somalia", "", false},
    {"South Africa", "", false},
    {"South Korea", "ROK|Republic of Korea|Korea|Korea, Republic of", false},
    {"South Sudan", "", false},
    {"Spain", "", false},
    {"Sri Lanka", "", false},
    {"State of Palestine", "Palestine|Palestine, State of", false},
    {"Sudan", "", false},
    {"Suriname", "", false},
    {"Sweden", "", false},
    {"Switzerland", "", false},
    {"Syria", "Syrian Arab Republic", false},
    {"Tajikistan", "", false},
    {"Tanzania", "United Republic of Tanzania|Tanzania, United Republic of", false},
    {"Thailand", "", false},
    {"Timor-Leste", "East Timor", false},
    {"Togo", "", false},
    {"Tonga", "", false},
    {"Trinidad and Tobago", "Trinidad|Trinidad & Tobago", false},
    {"Tunisia", "", false},
    {"Türkiye", "Turkey", false},
    {"Turkmenistan", "", false},
    {"Tuvalu", "", false},
    {"Uganda", "", false},
    {"Ukraine", "", false},
    {"United Arab Emirates", "UAE", false},
    {"United Kingdom", "UK|Britain|Great Britain|United Kingdom of Great Britain and Northern Ireland", true},
    {"United States", "USA|US|America|United States of America", true},
    {"Uruguay", "", false},
    {"Uzbekistan", "", false},
    {"Vanuatu", "", false},
    {"Venezuela", "Bolivarian Republic of Venezuela|Venezuela, Bolivarian Republic of", false},
    {"Vietnam", "Viet Nam", false},
    {"Yemen", "", false},
    {"Zambia", "", false},
    {"Zimbabwe", "", false},
};

#define COUNTRY_TOTAL ((int)(sizeof(COUNTRIES) / sizeof(COUNTRIES[0])))
static_assert(COUNTRY_TOTAL == 195, "193 member states and 2 observer states");

const int COUNTRY_COUNT = COUNTRY_TOTAL;

// copies the alias at *cursor (up to the next '|') into out and steps past it, false when none are left
static bool next_alias(const char **cursor, char *out, int size) {
    const char *alias = *cursor;
    if (alias == NULL || *alias == '\0') return false;
    size_t len = strcspn(alias, "|");
    *cursor = alias[len] == '|' ? alias + len + 1 : alias + len;
    if (len >= (size_t)size) len = (size_t)size - 1;
    memcpy(out, alias, len);
    out[len] = '\0';
    return true;
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

int find_country(const char *text) {
    char folded[NAME_SIZE * 2];
    fold_name(text, folded, sizeof(folded));
    if (folded[0] == '\0') return -1;

    char alias[NAME_SIZE * 2];
    for (int i = 0; i < COUNTRY_TOTAL; i++) {
        if (folds_to(COUNTRIES[i].name, folded)) return i;
        const char *cursor = COUNTRIES[i].aliases;
        while (next_alias(&cursor, alias, sizeof(alias))) {
            if (folds_to(alias, folded)) return i;
        }
    }
    return -1;
}

bool is_p5(const char *name) {
    int index = find_country(name);
    return index >= 0 && COUNTRIES[index].p5;
}

// the best match_name over everything COUNTRIES[index] is called, 5 less and never below 1,
// 0 when nothing matches. the placard name counts too, for a roster that wrote "USA"
static int alias_score(int index, const char *query) {
    int best = match_name(COUNTRIES[index].name, query);
    const char *cursor = COUNTRIES[index].aliases;
    char alias[NAME_SIZE * 2];
    while (best < 100 && next_alias(&cursor, alias, sizeof(alias))) {
        int score = match_name(alias, query);
        if (score > best) best = score;
    }
    if (best <= 0) return 0;
    return best > 5 ? best - 5 : 1;
}

int match_score(const char *name, const char *query) {
    int score = match_name(name, query);
    if (score >= 100) return score;
    int index = find_country(name);
    if (index < 0) return score;
    int alias = alias_score(index, query);
    return alias > score ? alias : score;
}

int search_countries(const char *query, int *out, int max) {
    if (out == NULL || max <= 0) return 0;
    char typed[NAME_SIZE * 2];
    fold_name(query, typed, sizeof(typed));
    if (typed[0] == '\0') return 0;

    int found[COUNTRY_TOTAL];
    int scores[COUNTRY_TOTAL];
    int lengths[COUNTRY_TOTAL];
    int count = 0;
    for (int i = 0; i < COUNTRY_TOTAL; i++) {
        int score = match_name(COUNTRIES[i].name, query);
        if (score < 100) {
            int alias = alias_score(i, query);
            if (alias > score) score = alias;
        }
        if (score <= 0) continue;

        char folded[NAME_SIZE * 2];
        fold_name(COUNTRIES[i].name, folded, sizeof(folded));
        int length = (int)strlen(folded);
        // insertion sort: ties keep the alphabetical order they arrive in
        int at = count++;
        while (at > 0 && (scores[at - 1] < score || (scores[at - 1] == score && lengths[at - 1] > length))) {
            found[at] = found[at - 1];
            scores[at] = scores[at - 1];
            lengths[at] = lengths[at - 1];
            at--;
        }
        found[at] = i;
        scores[at] = score;
        lengths[at] = length;
    }

    int written = count < max ? count : max;
    memcpy(out, found, (size_t)written * sizeof(int));
    return written;
}
