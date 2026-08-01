#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>

#include "miles_tag_protocol.h"
#include "miles_tag_tx.h"
#include "views/miles_tag_sweep_view.h"
#include "views/miles_tag_tx_view.h"
#include "scenes/miles_tag_scenes.h"

#define MILES_TAG_APP_NAME "MilesTag II TX"

/** Where the settings live between runs. The magic is a single byte. */
#define MILES_TAG_SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define MILES_TAG_SETTINGS_MAGIC 0x54 /* 'T' */
/** Bumped when the saved layout changes; an older file falls back to defaults. */
#define MILES_TAG_SETTINGS_VERSION 2

typedef enum {
    MilesTagViewVarItemList,
    MilesTagViewSubmenu,
    MilesTagViewWidget,
    MilesTagViewTx,
    MilesTagViewSweep,
    MilesTagViewByteInput,
} MilesTagViewId;

typedef enum {
    /** The signal type changed; the current config list must be rebuilt. */
    MilesTagCustomEventRebuildConfig = 100,
    /** A transmit burst or a sweep finished. */
    MilesTagCustomEventTxDone,
    /**
     * OK was pressed on a config row; the event value is this base plus the
     * MilesTagConfigItem of that row.
     *
     * VariableItemList runs both its change and its enter callbacks while
     * holding its view model's mutex, and scene_manager_next_scene() runs the
     * outgoing scene's on_exit handler - which touches that same model. Doing
     * the navigation directly from the callback would therefore deadlock on a
     * non-recursive mutex, so every row action is deferred through the event
     * loop instead.
     */
    MilesTagCustomEventItemBase = 200,
} MilesTagCustomEvent;

/**
 * Controls that can appear on the config screens. Which of them are present
 * depends on the selected signal type and, for sweeps, on which fields are
 * marked unknown - so the scenes keep a row -> control map instead of relying
 * on fixed indices.
 */
typedef enum {
    MilesTagConfigItemFrequency,
    MilesTagConfigItemType,
    MilesTagConfigItemPlayer,
    MilesTagConfigItemTeam,
    MilesTagConfigItemDamage,
    MilesTagConfigItemAmount,
    MilesTagConfigItemCommand,
    MilesTagConfigItemPickup,
    MilesTagConfigItemCustomId,
    MilesTagConfigItemCustomData,
    MilesTagConfigItemDuty,
    MilesTagConfigItemRepeat,
    MilesTagConfigItemGap,
    MilesTagConfigItemTransmit,
    MilesTagConfigItemAbout,
    /* Brute-force screen only */
    MilesTagConfigItemSweepPlayer,
    MilesTagConfigItemSweepTeam,
    MilesTagConfigItemSweepDamage,
    MilesTagConfigItemSweepCommand,
    MilesTagConfigItemSweepLog,
    MilesTagConfigItemSweepTotal,
    MilesTagConfigItemSweepEta,
    MilesTagConfigItemSweepStart,
} MilesTagConfigItem;

#define MILES_TAG_CONFIG_ITEM_MAX 20

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    VariableItemList* var_item_list;
    Submenu* submenu;
    Widget* widget;
    ByteInput* byte_input;
    MilesTagTxView* tx_view;
    MilesTagSweepView* sweep_view;

    MilesTagTx* tx;
    MilesTagConfig config;
    MilesTagSweepOptions sweep_options;

    /** Scratch value for the byte input screen. */
    uint8_t byte_input_value;

    /** Row index -> MilesTagConfigItem for the config list being shown. */
    uint8_t config_item_map[MILES_TAG_CONFIG_ITEM_MAX];
    uint8_t config_item_count;
} MilesTagApp;

/** Record which control lives on the next row of the config list being built. */
void miles_tag_config_map_add(MilesTagApp* app, MilesTagConfigItem item);

/** Settings persistence. Both are best-effort: a failure just means defaults. */
void miles_tag_settings_load(MilesTagConfig* config, MilesTagSweepOptions* sweep_options);
void miles_tag_settings_save(const MilesTagConfig* config, const MilesTagSweepOptions* sweep_options);
