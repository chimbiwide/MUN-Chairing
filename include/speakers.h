#ifndef SPEAKERS_H
#define SPEAKERS_H

#include <stdbool.h>
#include <text.h>

#define MAX_SPEAKERS 256

// what add_speaker and insert_speaker return when they don't add
#define SPEAKER_FULL -1
#define SPEAKER_EMPTY -2
#define SPEAKER_WAITING -3

typedef struct {
    char name[NAME_SIZE];
    char note[NOTE_SIZE];   // "yielded by Brazil", "questions"
    int limit;              // seconds for this speech, 0 means the list's usual time
    double spoke;           // seconds used, filled in when done
    bool done;              // crossed out
} Speaker;

// a speakers' list: crossed-out entries stay so the room can see who spoke
typedef struct {
    Speaker items[MAX_SPEAKERS];
    int count;
    int current;            // index of the speaker holding the floor, -1 when nobody
} SpeakerList;

void clear_speakers(SpeakerList *list);
// appends name (trimmed). refuses an empty name, a full list, and a name that is
// already waiting, i.e. not crossed out (same_name, the current speaker counts as waiting)
int add_speaker(SpeakerList *list, const char *name);
// the same, at position at (clamped to 0..count); current keeps pointing at the same speaker
int insert_speaker(SpeakerList *list, int at, const char *name);
// current keeps pointing at the same speaker, or becomes -1 when it was the one removed
bool remove_speaker(SpeakerList *list, int index);
// swaps with the neighbour delta (-1 up, +1 down) away; false at either end. current follows its speaker
bool move_speaker(SpeakerList *list, int index, int delta);
// crossing out also stores spoke; uncrossing clears spoke and the note.
// crossing out the current speaker leaves current as it is, the caller moves it
void set_done(SpeakerList *list, int index, bool done, double spoke);
// first entry that is neither crossed out nor current, -1 when none
int next_pending(const SpeakerList *list);
// entries neither crossed out nor current
int pending_count(const SpeakerList *list);
int done_count(const SpeakerList *list);
// not crossed out (pending or current), compared with same_name
bool is_waiting(const SpeakerList *list, const char *name);

#endif
