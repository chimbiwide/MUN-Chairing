#ifndef ROSTER_H
#define ROSTER_H

#include <stdbool.h>
#include <text.h>

#define MAX_DELEGATES 256

// what add_delegate returns when it doesn't add
#define DELEGATE_FULL -1
#define DELEGATE_EMPTY -2
#define DELEGATE_DUPLICATE -3

typedef enum {
    ABSENT,
    PRESENT,
    PRESENT_VOTING,     // may not abstain on substantive votes
} Attendance;

typedef enum {
    BALLOT_NONE,
    BALLOT_YES,
    BALLOT_NO,
    BALLOT_ABSTAIN,
} Ballot;

typedef struct {
    char name[NAME_SIZE];
    Attendance attendance;
    Ballot ballot;          // substantive vote in progress
    bool veto;              // permanent member under Security Council rules
    int gsl_speeches;
    int caucus_speeches;
    double spoken;          // seconds over all speeches
} Delegate;

typedef struct {
    Delegate items[MAX_DELEGATES];
    int count;
    bool roll_taken;        // false until the first roll call, attendance is ignored before that
} Roster;

typedef struct {
    int yes;
    int no;
    int abstain;
    int missing;            // present but no ballot yet
    bool vetoed;            // a delegate with veto voted no
} Tally;

void clear_roster(Roster *roster);
// trims name and appends it as ABSENT with veto set from is_p5(). refuses empty, full and same_name duplicates
int add_delegate(Roster *roster, const char *name);
bool remove_delegate(Roster *roster, int index);
// same_name lookup, -1 when missing
int find_delegate(const Roster *roster, const char *name);
// Adds every name in pasted text. Text with line breaks is split on lines only (a spreadsheet
// column, "Korea, Republic of" stays whole) and only the first tab-separated cell of a line counts;
// text on one line is split on commas and semicolons. Leading list markers like "1.", "2)", "-", "*",
// "•" and surrounding quotes are stripped. Returns how many were added; duplicates are skipped.
int import_delegates(Roster *roster, const char *text);
// alphabetical by fold_name
void sort_delegates(Roster *roster);
// roster indices matching query by match_score, best first (score, then roster order).
// an empty query lists everyone in roster order. present_only skips delegates that
// count_present would not count. returns how many were written, at most max
int match_delegates(const Roster *roster, const char *query, bool present_only, int *out, int max);
// PRESENT plus PRESENT_VOTING once the roll is taken, otherwise everyone on the roster
int count_present(const Roster *roster);
int count_attendance(const Roster *roster, Attendance attendance);
// sets every ballot to BALLOT_NONE
void clear_ballots(Roster *roster);
// ballots of the delegates counted by count_present
Tally count_ballots(const Roster *roster);

// more than half: n / 2 + 1, 0 when n <= 0
int simple_majority(int n);
// at least two thirds: ceil(2n / 3), 0 when n <= 0
int two_thirds(int n);
// at least a third of the members: ceil(n / 3), 0 when n <= 0
int one_third(int n);

#endif
