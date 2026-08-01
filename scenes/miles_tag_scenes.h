#pragma once

#include <gui/scene_manager.h>

/* Scene ids */
#define ADD_SCENE(prefix, name, id) MilesTagScene##id,
typedef enum {
#include "miles_tag_scenes_config.h"
    MilesTagSceneCount,
} MilesTagScene;
#undef ADD_SCENE

extern const SceneManagerHandlers miles_tag_scene_handlers;

/* Generate the handler prototypes for every scene in the config above. */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "miles_tag_scenes_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent);
#include "miles_tag_scenes_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void*);
#include "miles_tag_scenes_config.h"
#undef ADD_SCENE
