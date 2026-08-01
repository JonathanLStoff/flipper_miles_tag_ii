/**
 * MilesTag II infrared protocol encoder.
 *
 * Reference: "The MilesTag 2 protocol", Christopher Malton, April 2011, and the
 * ncmreynolds/milesTag Arduino library (https://github.com/ncmreynolds/milesTag).
 *
 * Line coding
 * -----------
 * Everything is a burst of carrier (38, 40 or 56 kHz) separated by a fixed gap:
 *
 *   header  2400us mark
 *   '1'     1200us mark
 *   '0'      600us mark
 *   gap      600us space after every mark
 *
 * Packets
 * -------
 *   shot     [header][0 ppppppp][tt dddd]              -> 14 data bits
 *   message  [header][1 mmmmmmm][dddddddd][0xE8]       -> 24 data bits
 *
 * Bits go out MSB first. The shot packet only sends the top six bits of its
 * second byte, which is why it is 14 bits rather than 16.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ timings */

#define MILES_TAG_HEADER_MARK 2400u
#define MILES_TAG_ONE_MARK    1200u
#define MILES_TAG_ZERO_MARK   600u
#define MILES_TAG_GAP         600u

/** Longest packet we build: header pair + 24 bits * (mark, space). */
#define MILES_TAG_MAX_TIMINGS (2u + 24u * 2u)

/** Carrier frequencies the protocol is used with. */
extern const uint32_t miles_tag_frequencies[];
extern const size_t miles_tag_frequency_count;

/** Duty cycles offered for the carrier. */
extern const float miles_tag_duty_cycles[];
extern const char* const miles_tag_duty_cycle_names[];
extern const size_t miles_tag_duty_cycle_count;

/* -------------------------------------------------------------- packet kinds */

typedef enum {
    MilesTagSignalShot = 0, /**< tag someone: player + team + damage */
    MilesTagSignalAddHealth, /**< 0x80, data 1..100 */
    MilesTagSignalAddRounds, /**< 0x81, data 1..100 */
    MilesTagSignalCommand, /**< 0x83, data = command byte */
    MilesTagSignalClipsPickup, /**< 0x8A, data = ammo box id 0..15 */
    MilesTagSignalHealthPickup, /**< 0x8B, data = medic box id 0..15 */
    MilesTagSignalFlagPickup, /**< 0x8C, data = flag id 0..15 */
    MilesTagSignalCustom, /**< raw message id + raw data byte */
    MilesTagSignalCount,
} MilesTagSignalType;

extern const char* const miles_tag_signal_names[MilesTagSignalCount];

/** Message IDs (section 2.2). The leading 1 bit is part of the ID. */
typedef enum {
    MilesTagMsgAddHealth = 0x80,
    MilesTagMsgAddRounds = 0x81,
    MilesTagMsgCommand = 0x83,
    MilesTagMsgSystemData = 0x87,
    MilesTagMsgClipsPickup = 0x8A,
    MilesTagMsgHealthPickup = 0x8B,
    MilesTagMsgFlagPickup = 0x8C,
} MilesTagMessageId;

/** Terminator byte that closes every message packet. */
#define MILES_TAG_MESSAGE_TERMINATOR 0xE8

/* --------------------------------------------------------------- game tables */

/** Team IDs, section 2.3.1. */
#define MILES_TAG_TEAM_COUNT 4
extern const char* const miles_tag_team_names[MILES_TAG_TEAM_COUNT];

/** Damage lookup, section 2.3.3: 4-bit code -> hit points. */
#define MILES_TAG_DAMAGE_COUNT 16
extern const uint8_t miles_tag_damage_values[MILES_TAG_DAMAGE_COUNT];

/** Player IDs are 7 bits; the first 60 have stock display names (appendix A). */
#define MILES_TAG_PLAYER_COUNT 128
#define MILES_TAG_NAMED_PLAYER_COUNT 60
extern const char* const miles_tag_player_names[MILES_TAG_NAMED_PLAYER_COUNT];

/** Single-byte commands carried by message 0x83, section 2.2.4. */
#define MILES_TAG_COMMAND_COUNT 24
extern const char* const miles_tag_command_names[MILES_TAG_COMMAND_COUNT];
/** True for command bytes the spec marks reserved (0x09, 0x0E, 0x10..0x13). */
bool miles_tag_command_is_reserved(uint8_t command);

/* --------------------------------------------------------------- the payload */

/** Everything the UI can configure, and everything the encoder needs. */
typedef struct {
    MilesTagSignalType type;
    uint8_t frequency_index; /**< index into miles_tag_frequencies */
    uint8_t duty_index; /**< index into miles_tag_duty_cycles */

    uint8_t player_id; /**< 0..127, shot packets */
    uint8_t team_id; /**< 0..3, shot packets */
    uint8_t damage_index; /**< 0..15, shot packets */

    uint8_t amount; /**< 1..100, add health / add rounds */
    uint8_t command; /**< 0x00..0x17, command message */
    uint8_t pickup_id; /**< 0..15, clip/health/flag pickups */

    uint8_t custom_message_id; /**< 0x80..0xFF, custom message */
    uint8_t custom_data; /**< 0x00..0xFF, custom message */

    uint8_t repeat; /**< how many times to send, 1..10 */
    uint8_t gap_ms; /**< pause between repeats, milliseconds */
} MilesTagConfig;

/** An encoded packet: the bytes, the bit count, and the IR timing sequence. */
typedef struct {
    uint8_t bytes[3];
    uint8_t byte_count;
    uint8_t bit_count;
    uint32_t timings[MILES_TAG_MAX_TIMINGS];
    size_t timing_count;
    uint32_t frequency;
    float duty_cycle;
} MilesTagPacket;

/** Fill a config with sensible defaults (player 1, red team, 1 damage, 56 kHz). */
void miles_tag_config_init(MilesTagConfig* config);

/** Encode `config` into `packet`. Always succeeds; every field is range-clamped. */
void miles_tag_encode(const MilesTagConfig* config, MilesTagPacket* packet);

/** "0x01 0x84" style hex dump of the packet bytes. */
void miles_tag_packet_hex(const MilesTagPacket* packet, char* out, size_t out_size);

/** Bit string as actually transmitted, e.g. "00000001 100001". */
void miles_tag_packet_bits(const MilesTagPacket* packet, char* out, size_t out_size);

/** One-line description of what the packet does, for the TX screen. */
void miles_tag_config_summary(const MilesTagConfig* config, char* out, size_t out_size);

/** Display name for a player ID: "007 Blaze" for named IDs, "064" otherwise. */
void miles_tag_player_label(uint8_t player_id, char* out, size_t out_size);

/** Total on-air duration of one packet in microseconds. */
uint32_t miles_tag_packet_duration_us(const MilesTagPacket* packet);

#ifdef __cplusplus
}
#endif
