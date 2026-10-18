#ifndef DEMO_H
#define DEMO_H

#include <stdbool.h>
#include <ui.h>

#define DEMO_SCENES 17

// fills app with a sample committee arranged for one scene (clocks relative to app->now)
// and returns a short name for the screenshot file
const char *build_demo_scene(App *app, int scene);
// whether the scene is also shot at the other window sizes
bool demo_scene_resized(int scene);

#endif
