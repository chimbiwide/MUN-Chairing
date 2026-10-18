#include <speakers.h>
#include <string.h>

void clear_speakers(SpeakerList *list) {
    list->count = 0;
    list->current = -1;
}

int add_speaker(SpeakerList *list, const char *name) {
    return insert_speaker(list, list->count, name);
}

int insert_speaker(SpeakerList *list, int at, const char *name) {
    char clean[NAME_SIZE];
    copy_text(clean, sizeof(clean), name);
    trim_text(clean);
    if (clean[0] == '\0') return SPEAKER_EMPTY;
    if (is_waiting(list, clean)) return SPEAKER_WAITING;
    if (list->count >= MAX_SPEAKERS) return SPEAKER_FULL;

    if (at < 0) at = 0;
    if (at > list->count) at = list->count;
    memmove(&list->items[at + 1], &list->items[at], (size_t)(list->count - at) * sizeof(Speaker));
    list->items[at] = (Speaker){0};
    copy_text(list->items[at].name, NAME_SIZE, clean);
    list->count++;
    if (list->current >= at) list->current++;
    return at;
}

bool remove_speaker(SpeakerList *list, int index) {
    if (index < 0 || index >= list->count) return false;
    memmove(&list->items[index], &list->items[index + 1], (size_t)(list->count - index - 1) * sizeof(Speaker));
    list->count--;
    if (list->current == index) list->current = -1;
    else if (list->current > index) list->current--;
    return true;
}

bool move_speaker(SpeakerList *list, int index, int delta) {
    int target = index + delta;
    if (index < 0 || index >= list->count || target < 0 || target >= list->count || delta == 0) return false;
    Speaker moved = list->items[index];
    list->items[index] = list->items[target];
    list->items[target] = moved;
    if (list->current == index) list->current = target;
    else if (list->current == target) list->current = index;
    return true;
}

void set_done(SpeakerList *list, int index, bool done, double spoke) {
    if (index < 0 || index >= list->count) return;
    Speaker *speaker = &list->items[index];
    speaker->done = done;
    if (done) speaker->spoke = spoke;
    else {
        speaker->spoke = 0.0;
        speaker->note[0] = '\0';
    }
}

int next_pending(const SpeakerList *list) {
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].done && i != list->current) return i;
    }
    return -1;
}

int pending_count(const SpeakerList *list) {
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].done && i != list->current) count++;
    }
    return count;
}

int done_count(const SpeakerList *list) {
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].done) count++;
    }
    return count;
}

bool is_waiting(const SpeakerList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].done && same_name(list->items[i].name, name)) return true;
    }
    return false;
}
