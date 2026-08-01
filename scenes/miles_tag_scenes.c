#include "miles_tag_scenes.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const miles_tag_scene_on_enter_handlers[])(void*) = {
#include "miles_tag_scenes_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const miles_tag_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "miles_tag_scenes_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const miles_tag_scene_on_exit_handlers[])(void*) = {
#include "miles_tag_scenes_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers miles_tag_scene_handlers = {
    .on_enter_handlers = miles_tag_scene_on_enter_handlers,
    .on_event_handlers = miles_tag_scene_on_event_handlers,
    .on_exit_handlers = miles_tag_scene_on_exit_handlers,
    .scene_num = MilesTagSceneCount,
};
