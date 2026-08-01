#include "miles_tag_protocol.h"

#include <furi.h>

const uint32_t miles_tag_frequencies[] = {38000, 40000, 56000};
const size_t miles_tag_frequency_count = COUNT_OF(miles_tag_frequencies);

const float miles_tag_duty_cycles[] = {0.33f, 0.40f, 0.50f};
const char* const miles_tag_duty_cycle_names[] = {"33%", "40%", "50%"};
const size_t miles_tag_duty_cycle_count = COUNT_OF(miles_tag_duty_cycles);

const char* const miles_tag_signal_names[MilesTagSignalCount] = {
    "Shot",
    "Add Health",
    "Add Rounds",
    "Command",
    "Clips Pickup",
    "Health Pickup",
    "Flag Pickup",
    "Custom Msg",
};

const char* const miles_tag_team_names[MILES_TAG_TEAM_COUNT] = {
    "Red",
    "Blue",
    "Yellow",
    "Green",
};

/* Section 2.3.3 - the 4-bit damage field is an index into this table. */
const uint8_t miles_tag_damage_values[MILES_TAG_DAMAGE_COUNT] = {
    1,
    2,
    4,
    5,
    7,
    10,
    15,
    17,
    20,
    25,
    30,
    35,
    40,
    50,
    75,
    100,
};

/* Appendix A - stock display names for player IDs 0x00..0x3B. */
const char* const miles_tag_player_names[MILES_TAG_NAMED_PLAYER_COUNT] = {
    "Eagle", "Joker", "Raven", "Sarge", "Angel", "Cosmo", "Gecko", "Blaze", "Camo",  "Fury",
    "Flash", "Gizmo", "Homer", "Storm", "Habit", "Click", "Ronin", "Lucky", "Radar", "Blade",
    "Ninja", "Magic", "Gonzo", "Cobra", "Pappy", "Rambo", "Snake", "Audie", "Sting", "Zeena",
    "Bugsy", "Viper", "Jewel", "Genie", "Logan", "Razor", "Slick", "Venom", "Rocky", "Saber",
    "Crush", "Titan", "Orbit", "Vixen", "Tank",  "Rogue", "Sheik", "Gizmo", "Siren", "Dozer",
    "Micro", "LgtMG", "HvyMG", "ZOOKA", "ROCKT", "GRNDE", "CLYMR", "MINE",  "BOMB",  "NUKE",
};

/* Section 2.2.4 - data byte of message 0x83. */
const char* const miles_tag_command_names[MILES_TAG_COMMAND_COUNT] = {
    "Admin Kill", /* 0x00 */
    "Pause/Unpause", /* 0x01 */
    "Start Game", /* 0x02 */
    "Restore Defaults", /* 0x03 */
    "Respawn", /* 0x04 */
    "Immediate Game", /* 0x05 */
    "Full Ammo", /* 0x06 */
    "End Game", /* 0x07 */
    "Reset Clock", /* 0x08 */
    "Reserved 0x09", /* 0x09 */
    "Init Player", /* 0x0A */
    "Explode Player", /* 0x0B */
    "New Game Ready", /* 0x0C */
    "Full Health", /* 0x0D */
    "Reserved 0x0E", /* 0x0E */
    "Full Armour", /* 0x0F */
    "Reserved 0x10", /* 0x10 */
    "Reserved 0x11", /* 0x11 */
    "Reserved 0x12", /* 0x12 */
    "Reserved 0x13", /* 0x13 */
    "Clear Scores", /* 0x14 */
    "Test Sensors", /* 0x15 */
    "Stun Player", /* 0x16 */
    "Disarm Player", /* 0x17 */
};

bool miles_tag_command_is_reserved(uint8_t command) {
    return command == 0x09 || command == 0x0E || (command >= 0x10 && command <= 0x13);
}

void miles_tag_config_init(MilesTagConfig* config) {
    furi_check(config);

    config->type = MilesTagSignalShot;
    config->frequency_index = 2; /* 56 kHz, the MilesTag II default */
    config->duty_index = 2; /* 50%, matching the reference library */

    config->player_id = 1;
    config->team_id = 0;
    config->damage_index = 0; /* 1 hit point */

    config->amount = 10;
    config->command = 0x00;
    config->pickup_id = 0;

    config->custom_message_id = MilesTagMsgAddHealth;
    config->custom_data = 0x00;

    config->repeat = 1;
    config->gap_ms = 50;
}

/** Append one bit's mark/space pair to the timing list. */
static void miles_tag_push_bit(MilesTagPacket* packet, bool bit) {
    furi_check(packet->timing_count + 2 <= MILES_TAG_MAX_TIMINGS);
    packet->timings[packet->timing_count++] = bit ? MILES_TAG_ONE_MARK : MILES_TAG_ZERO_MARK;
    packet->timings[packet->timing_count++] = MILES_TAG_GAP;
}

/** Append the top `bits` bits of `byte`, MSB first. */
static void miles_tag_push_bits(MilesTagPacket* packet, uint8_t byte, uint8_t bits) {
    for(uint8_t i = 0; i < bits; i++) {
        miles_tag_push_bit(packet, (byte >> (7 - i)) & 1);
        packet->bit_count++;
    }
}

void miles_tag_encode(const MilesTagConfig* config, MilesTagPacket* packet) {
    furi_check(config);
    furi_check(packet);

    memset(packet, 0, sizeof(MilesTagPacket));

    uint8_t frequency_index = config->frequency_index;
    if(frequency_index >= miles_tag_frequency_count) frequency_index = 0;
    packet->frequency = miles_tag_frequencies[frequency_index];

    uint8_t duty_index = config->duty_index;
    if(duty_index >= miles_tag_duty_cycle_count) duty_index = miles_tag_duty_cycle_count - 1;
    packet->duty_cycle = miles_tag_duty_cycles[duty_index];

    /* Header: one long mark, then the standard gap. */
    packet->timings[packet->timing_count++] = MILES_TAG_HEADER_MARK;
    packet->timings[packet->timing_count++] = MILES_TAG_GAP;

    if(config->type == MilesTagSignalShot) {
        /* [0 ppppppp][tt dddd] - 8 bits then 6 bits. */
        packet->bytes[0] = config->player_id & 0x7F;
        packet->bytes[1] = (uint8_t)((config->team_id & 0x03) << 6) |
                           (uint8_t)((config->damage_index & 0x0F) << 2);
        packet->byte_count = 2;

        miles_tag_push_bits(packet, packet->bytes[0], 8);
        miles_tag_push_bits(packet, packet->bytes[1], 6);
    } else {
        /* [1mmmmmmm][dddddddd][0xE8] - three whole bytes. */
        uint8_t message_id;
        uint8_t data;

        switch(config->type) {
        case MilesTagSignalAddHealth:
            message_id = MilesTagMsgAddHealth;
            data = config->amount;
            break;
        case MilesTagSignalAddRounds:
            message_id = MilesTagMsgAddRounds;
            data = config->amount;
            break;
        case MilesTagSignalCommand:
            message_id = MilesTagMsgCommand;
            data = config->command;
            break;
        case MilesTagSignalClipsPickup:
            message_id = MilesTagMsgClipsPickup;
            data = config->pickup_id & 0x0F;
            break;
        case MilesTagSignalHealthPickup:
            message_id = MilesTagMsgHealthPickup;
            data = config->pickup_id & 0x0F;
            break;
        case MilesTagSignalFlagPickup:
            message_id = MilesTagMsgFlagPickup;
            data = config->pickup_id & 0x0F;
            break;
        case MilesTagSignalCustom:
        default:
            message_id = config->custom_message_id;
            data = config->custom_data;
            break;
        }

        /* Bit 7 always set: it is what marks the packet as a message. */
        packet->bytes[0] = message_id | 0x80;
        packet->bytes[1] = data;
        packet->bytes[2] = MILES_TAG_MESSAGE_TERMINATOR;
        packet->byte_count = 3;

        miles_tag_push_bits(packet, packet->bytes[0], 8);
        miles_tag_push_bits(packet, packet->bytes[1], 8);
        miles_tag_push_bits(packet, packet->bytes[2], 8);
    }
}

void miles_tag_packet_hex(const MilesTagPacket* packet, char* out, size_t out_size) {
    furi_check(packet);
    furi_check(out);

    size_t offset = 0;
    for(uint8_t i = 0; i < packet->byte_count && offset < out_size; i++) {
        int written = snprintf(
            out + offset, out_size - offset, (i == 0) ? "%02X" : " %02X", packet->bytes[i]);
        if(written <= 0) break;
        offset += (size_t)written;
    }
    if(out_size) out[out_size - 1] = '\0';
}

void miles_tag_packet_bits(const MilesTagPacket* packet, char* out, size_t out_size) {
    furi_check(packet);
    furi_check(out);

    size_t offset = 0;
    uint8_t remaining = packet->bit_count;

    for(uint8_t i = 0; i < packet->byte_count && remaining; i++) {
        uint8_t bits = remaining > 8 ? 8 : remaining;
        if(i && offset + 1 < out_size) out[offset++] = ' ';
        for(uint8_t b = 0; b < bits && offset + 1 < out_size; b++) {
            out[offset++] = ((packet->bytes[i] >> (7 - b)) & 1) ? '1' : '0';
        }
        remaining -= bits;
    }
    if(offset < out_size) out[offset] = '\0';
    if(out_size) out[out_size - 1] = '\0';
}

void miles_tag_player_label(uint8_t player_id, char* out, size_t out_size) {
    furi_check(out);

    player_id &= 0x7F;
    if(player_id < MILES_TAG_NAMED_PLAYER_COUNT) {
        snprintf(out, out_size, "%03u %s", player_id, miles_tag_player_names[player_id]);
    } else {
        snprintf(out, out_size, "%03u", player_id);
    }
}

void miles_tag_config_summary(const MilesTagConfig* config, char* out, size_t out_size) {
    furi_check(config);
    furi_check(out);

    switch(config->type) {
    case MilesTagSignalShot: {
        char player[24];
        miles_tag_player_label(config->player_id, player, sizeof(player));
        snprintf(
            out,
            out_size,
            "%s, %s team, %u dmg",
            player,
            miles_tag_team_names[config->team_id & 0x03],
            miles_tag_damage_values[config->damage_index & 0x0F]);
        break;
    }
    case MilesTagSignalAddHealth:
        snprintf(out, out_size, "Add %u health", config->amount);
        break;
    case MilesTagSignalAddRounds:
        snprintf(out, out_size, "Add %u rounds", config->amount);
        break;
    case MilesTagSignalCommand:
        snprintf(
            out,
            out_size,
            "Cmd 0x%02X %s",
            config->command,
            (config->command < MILES_TAG_COMMAND_COUNT) ?
                miles_tag_command_names[config->command] :
                "Unknown");
        break;
    case MilesTagSignalClipsPickup:
        snprintf(out, out_size, "Clips pickup, box %u", config->pickup_id & 0x0F);
        break;
    case MilesTagSignalHealthPickup:
        snprintf(out, out_size, "Health pickup, box %u", config->pickup_id & 0x0F);
        break;
    case MilesTagSignalFlagPickup:
        snprintf(out, out_size, "Flag pickup, flag %u", config->pickup_id & 0x0F);
        break;
    case MilesTagSignalCustom:
    default:
        snprintf(
            out,
            out_size,
            "Msg 0x%02X data 0x%02X",
            config->custom_message_id | 0x80,
            config->custom_data);
        break;
    }
}

uint32_t miles_tag_packet_duration_us(const MilesTagPacket* packet) {
    furi_check(packet);

    uint32_t total = 0;
    for(size_t i = 0; i < packet->timing_count; i++) {
        total += packet->timings[i];
    }
    return total;
}
