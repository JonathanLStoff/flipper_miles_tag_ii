#include "../miles_tag_ii.h"

/** The transmit screen: encode the current config, show it, and fire on OK. */

static void miles_tag_scene_tx_refresh(MilesTagApp* app) {
    MilesTagPacket packet;
    miles_tag_encode(&app->config, &packet);

    char summary[48];
    char hex[16];
    char bits[32];
    miles_tag_config_summary(&app->config, summary, sizeof(summary));
    miles_tag_packet_hex(&packet, hex, sizeof(hex));
    miles_tag_packet_bits(&packet, bits, sizeof(bits));

    miles_tag_tx_view_set_packet(
        app->tx_view,
        miles_tag_signal_names[app->config.type],
        summary,
        hex,
        bits,
        packet.frequency,
        packet.bit_count,
        app->config.repeat);
    miles_tag_tx_view_set_sent_count(app->tx_view, miles_tag_tx_sent_count(app->tx));
}

static void miles_tag_scene_tx_fire_callback(void* context) {
    MilesTagApp* app = context;

    MilesTagPacket packet;
    miles_tag_encode(&app->config, &packet);

    if(miles_tag_tx_send(&packet, app->config.repeat, app->config.gap_ms, app->tx)) {
        miles_tag_tx_view_set_sending(app->tx_view, true);
        notification_message(app->notifications, &sequence_blink_blue_10);
    } else {
        /* Queue full - the user is firing faster than the air time allows. */
        notification_message(app->notifications, &sequence_blink_red_10);
    }
}

void miles_tag_scene_tx_on_enter(void* context) {
    MilesTagApp* app = context;

    miles_tag_scene_tx_refresh(app);
    miles_tag_tx_view_set_sending(app->tx_view, false);
    miles_tag_tx_view_set_fire_callback(app->tx_view, miles_tag_scene_tx_fire_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewTx);
}

bool miles_tag_scene_tx_on_event(void* context, SceneManagerEvent event) {
    MilesTagApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MilesTagCustomEventTxDone) {
            miles_tag_tx_view_set_sending(app->tx_view, false);
            miles_tag_tx_view_set_sent_count(app->tx_view, miles_tag_tx_sent_count(app->tx));
            return true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        /* Keep the counter live during a long burst. */
        miles_tag_tx_view_set_sent_count(app->tx_view, miles_tag_tx_sent_count(app->tx));
    }

    return false;
}

void miles_tag_scene_tx_on_exit(void* context) {
    MilesTagApp* app = context;
    miles_tag_tx_view_set_fire_callback(app->tx_view, NULL, NULL);
}
