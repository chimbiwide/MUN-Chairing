#include <demo.h>
#include <chair.h>
#include <views.h>
#include <string.h>

static const char *delegations[] = {
    "Argentina", "Australia", "Brazil", "Canada", "China", "Côte d'Ivoire", "Egypt", "France",
    "Germany", "India", "Indonesia", "Japan", "Kenya", "Mexico", "Nigeria", "Norway",
    "Pakistan", "Russia", "Saudi Arabia", "South Africa", "South Korea", "Türkiye",
    "United Kingdom", "United States",
};

static void build_committee(Session *session) {
    copy_text(session->committee, TITLE_SIZE, "DISEC · General Assembly First Committee");
    copy_text(session->agenda, TOPIC_SIZE, "Regulating lethal autonomous weapons systems");
    int count = (int)(sizeof(delegations) / sizeof(delegations[0]));
    for (int i = 0; i < count; i++) add_delegate(&session->roster, delegations[i]);
    for (int i = 0; i < session->roster.count; i++) {
        Attendance attendance = (i % 5 == 1) ? PRESENT_VOTING : PRESENT;
        if (i == 15 || i == 21) attendance = ABSENT;
        set_attendance(session, i, attendance);
    }
    finish_roll_call(session);
}

// one finished speech on the floor: starts the next speaker at *t and crosses them out after seconds
static void speak(Session *session, double *t, double seconds) {
    toggle_floor(session, *t);
    *t += seconds;
    next_speaker(session, *t);
    *t += 6.0;
}

static void add_all(Session *session, SpeakerList *list, const char *const *names, int count) {
    for (int i = 0; i < count; i++) add_to_list(session, list, names[i]);
}

static void build_gsl(Session *session, double now, double elapsed) {
    const char *speakers[] = { "United States", "China", "Kenya", "Brazil", "Germany", "Japan", "Mexico", "South Africa", "Pakistan" };
    add_all(session, &session->gsl, speakers, 9);
    double t = now - 400.0;
    speak(session, &t, 58.0);
    speak(session, &t, 61.5);
    speak(session, &t, 47.0);
    // Brazil holds the floor
    toggle_floor(session, now - elapsed);
}

static void propose(Session *session, MotionType type, int total, int speaker, const char *proposer, const char *topic) {
    Motion motion = { .type = type, .total = total, .speaker = speaker };
    copy_text(motion.proposer, NAME_SIZE, proposer);
    copy_text(motion.topic, TOPIC_SIZE, topic);
    propose_motion(session, &motion, NULL, 0);
}

static void build_motions(Session *session) {
    propose(session, MOTION_MOD, 540, 45, "France", "Accountability for autonomous strikes");
    propose(session, MOTION_UNMOD, 600, 0, "United States", "Drafting working papers");
    propose(session, MOTION_MOD, 600, 30, "Japan", "Verification and transparency");
}

// France proposed it and spoke first. last uses up 11 of the 12 slots so the speaker on the floor
// is the last one who fits
static void build_mod(Session *session, double now, bool last) {
    Motion motion = { .type = MOTION_MOD, .total = 540, .speaker = 45 };
    copy_text(motion.proposer, NAME_SIZE, "France");
    copy_text(motion.topic, TOPIC_SIZE, "Accountability for autonomous strikes");
    double t = now - 1000.0;
    start_caucus(session, &motion, t);
    const char *speakers[] = { "Germany", "India", "Kenya", "Mexico", "Nigeria", "Egypt" };
    add_all(session, &session->caucus.speakers, speakers, 6);
    speak(session, &t, 44.0);
    speak(session, &t, 45.0);
    speak(session, &t, 38.0);
    double elapsed = 12.0;
    if (last) {
        const char *more[] = { "Argentina", "Australia", "Canada", "China", "Indonesia", "Pakistan" };
        add_all(session, &session->caucus.speakers, more, 6);
        // 11 slots used: the 12th speaker is the last who fits, Pakistan won't
        for (int i = 0; i < 8; i++) speak(session, &t, 45.0);
        elapsed = 5.0;
    }
    toggle_floor(session, now - elapsed);
}

static void build_unmod(Session *session, double now) {
    Motion motion = { .type = MOTION_UNMOD, .total = 600 };
    copy_text(motion.proposer, NAME_SIZE, "United States");
    copy_text(motion.topic, TOPIC_SIZE, "Drafting working papers");
    start_caucus(session, &motion, now - 300.0);
    toggle_floor(session, now - 198.0);
}

bool demo_scene_resized(int scene) {
    return scene == 0 || scene == 2 || scene == 4 || scene == 6 || scene == 1;
}

const char *build_demo_scene(App *app, int scene) {
    Session *session = &app->session;
    double now = app->now;
    init_session(session);
    forget_history(&app->history);
    build_committee(session);
    app->overlay = OVERLAY_NONE;
    app->present = false;
    app->blank = false;
    app->add_to_gsl = false;
    app->armed = ARM_NONE;
    app->list_scroll = 0.0f;
    app->speaker_pick.text[0] = '\0';
    app->motion_quick[0] = '\0';
    invalidate_pickers(app);

    switch (scene) {
        case 0:
            build_gsl(session, now, 23.4);
            build_motions(session);
            return "gsl";
        case 1:
            build_gsl(session, now, 23.4);
            build_motions(session);
            focus_motion_line(app, "mod 10:45 egypt arms transfers");
            return "motions";
        case 2:
            build_gsl(session, now - 1200.0, 30.0);
            next_speaker(session, now - 1150.0);
            build_mod(session, now, false);
            return "mod";
        case 3:
            build_gsl(session, now - 1200.0, 30.0);
            next_speaker(session, now - 1150.0);
            build_mod(session, now, true);
            return "mod_last_speaker";
        case 4:
            build_gsl(session, now - 1200.0, 30.0);
            next_speaker(session, now - 1150.0);
            build_unmod(session, now);
            return "unmod";
        case 5:
            build_gsl(session, now, 67.0);
            return "overtime";
        case 6:
            build_gsl(session, now - 1200.0, 30.0);
            next_speaker(session, now - 1150.0);
            build_mod(session, now, false);
            app->present = true;
            return "present_mod";
        case 7:
            build_unmod(session, now);
            app->present = true;
            return "present_unmod";
        case 8:
            open_overlay(app, OVERLAY_SETUP);
            copy_text(app->roster_pick.text, sizeof(app->roster_pick.text), "kor");
            return "setup";
        case 9:
            open_overlay(app, OVERLAY_ROLLCALL);
            app->roll_cursor = 9;
            return "rollcall";
        case 10: {
            open_overlay(app, OVERLAY_VOTE);
            copy_text(app->vote_title, sizeof(app->vote_title), "Draft Resolution 1.2");
            for (int i = 0; i < session->roster.count; i++) {
                if (i < 12) session->roster.items[i].ballot = BALLOT_YES;
                else if (i < 16) session->roster.items[i].ballot = BALLOT_NO;
                else if (i < 19) session->roster.items[i].ballot = BALLOT_ABSTAIN;
            }
            app->vote_cursor = 19;
            return "vote";
        }
        case 11:
            open_overlay(app, OVERLAY_SETTINGS);
            return "settings";
        case 12:
            build_gsl(session, now, 23.4);
            open_overlay(app, OVERLAY_HELP);
            return "help";
        case 13:
            build_gsl(session, now - 1200.0, 30.0);
            next_speaker(session, now - 1150.0);
            build_mod(session, now, false);
            open_overlay(app, OVERLAY_LOG);
            return "log";
        case 14:
            build_gsl(session, now, 51.0);
            build_motions(session);
            session->settings.dark = true;
            return "dark";
        case 15:
            build_gsl(session, now, 23.4);
            set_phase(session, PHASE_VOTING, now);
            return "voting_phase";
        default:
            // Brazil finished, the chair takes motions with the room watching
            build_gsl(session, now - 30.0, 50.0);
            next_speaker(session, now - 20.0);
            build_motions(session);
            app->present = true;
            return "present_motions";
    }
}
