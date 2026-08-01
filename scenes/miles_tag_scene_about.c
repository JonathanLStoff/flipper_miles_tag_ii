#include "../miles_tag_ii.h"

/** Protocol crib sheet, so the packet layouts are on the device itself. */

void miles_tag_scene_about_on_enter(void* context) {
    MilesTagApp* app = context;

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "\e#%s\e#\n"
        "Transmits MilesTag II laser tag\n"
        "signals from the built-in IR LED.\n"
        "\n"
        "\e#Line coding\e#\n"
        "Carrier: 38 / 40 / 56 kHz\n"
        "Header: %u us mark\n"
        "One: %u us mark\n"
        "Zero: %u us mark\n"
        "Gap: %u us after every mark\n"
        "\n"
        "\e#Shot packet (14 bits)\e#\n"
        "[hdr][0ppppppp][ttdddd]\n"
        "p = player ID 0-127\n"
        "t = team: Red/Blue/Yellow/Green\n"
        "d = damage code, 0=1hp .. 15=100hp\n"
        "\n"
        "\e#Message packet (24 bits)\e#\n"
        "[hdr][1mmmmmmm][dddddddd][0xE8]\n"
        "0x80 Add Health, data 1-100\n"
        "0x81 Add Rounds, data 1-100\n"
        "0x83 Command, data 0x00-0x17\n"
        "0x8A Clips pickup, box 0-15\n"
        "0x8B Health pickup, box 0-15\n"
        "0x8C Flag pickup, flag 0-15\n"
        "Custom lets you send any ID/data.\n"
        "\n"
        "\e#Reference\e#\n"
        "MilesTag 2 protocol, C. Malton,\n"
        "April 2011; github.com/ncmreynolds/\n"
        "milesTag\n",
        MILES_TAG_APP_NAME,
        MILES_TAG_HEADER_MARK,
        MILES_TAG_ONE_MARK,
        MILES_TAG_ZERO_MARK,
        MILES_TAG_GAP);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewWidget);
}

bool miles_tag_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void miles_tag_scene_about_on_exit(void* context) {
    MilesTagApp* app = context;
    widget_reset(app->widget);
}
