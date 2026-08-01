#!/usr/bin/env python3
"""
Host-side checks for the MilesTag II encoder.

The C encoder cannot run off-device, so this re-implements the packet format
straight from the specification and then asserts that the tables and layouts
compiled into miles_tag_protocol.c agree with it. Anything that drifts - a
damage value, a command ID, the bit order - fails here rather than silently
transmitting a packet no tagger understands.

Run with:  make test      (or: python tests/test_protocol.py)
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROTOCOL_C = ROOT / "miles_tag_protocol.c"
PROTOCOL_H = ROOT / "miles_tag_protocol.h"
LOG_C = ROOT / "miles_tag_log.c"

HEADER_MARK = 2400
ONE_MARK = 1200
ZERO_MARK = 600
GAP = 600
TERMINATOR = 0xE8

# Section 2.3.3 of "The MilesTag 2 protocol", Christopher Malton, April 2011.
DAMAGE_TABLE = [1, 2, 4, 5, 7, 10, 15, 17, 20, 25, 30, 35, 40, 50, 75, 100]

# Section 2.3.1.
TEAMS = ["Red", "Blue", "Yellow", "Green"]

# Section 2.2.4. Reserved slots are named rather than omitted so the index of
# every command still equals its data byte.
COMMANDS = {
    0x00: "Admin Kill",
    0x01: "Pause/Unpause",
    0x02: "Start Game",
    0x03: "Restore Defaults",
    0x04: "Respawn",
    0x05: "Immediate Game",
    0x06: "Full Ammo",
    0x07: "End Game",
    0x08: "Reset Clock",
    0x0A: "Init Player",
    0x0B: "Explode Player",
    0x0C: "New Game Ready",
    0x0D: "Full Health",
    0x0F: "Full Armour",
    0x14: "Clear Scores",
    0x15: "Test Sensors",
    0x16: "Stun Player",
    0x17: "Disarm Player",
}
RESERVED_COMMANDS = {0x09, 0x0E, 0x10, 0x11, 0x12, 0x13}

failures = []


def check(name, condition, detail=""):
    """Record and print the result of a single assertion."""
    if condition:
        print(f"  ok   {name}")
    else:
        print(f"  FAIL {name}{': ' + detail if detail else ''}")
        failures.append(name)


def bits_to_timings(bits):
    """Reference encoder: header, then one mark/space pair per bit."""
    timings = [HEADER_MARK, GAP]
    for bit in bits:
        timings += [ONE_MARK if bit else ZERO_MARK, GAP]
    return timings


def shot_bits(player, team, damage_index):
    """[0 ppppppp][tt dddd] - 14 bits, MSB first."""
    byte0 = player & 0x7F  # top bit 0 marks a shot
    byte1 = ((team & 0x03) << 6) | ((damage_index & 0x0F) << 2)
    bits = [(byte0 >> (7 - i)) & 1 for i in range(8)]
    bits += [(byte1 >> (7 - i)) & 1 for i in range(6)]  # only the top 6 bits
    return bits


def message_bits(message_id, data):
    """[1mmmmmmm][dddddddd][0xE8] - 24 bits, MSB first."""
    bits = []
    for byte in (message_id | 0x80, data, TERMINATOR):
        bits += [(byte >> (7 - i)) & 1 for i in range(8)]
    return bits


def c_array(source, declaration):
    """Pull the brace-delimited body of a C array definition out of the source.

    None of these arrays contain nested braces, so stopping at the first closing
    brace is correct whether the definition spans one line or many.
    """
    match = re.search(re.escape(declaration) + r"\s*=\s*\{(.*?)\}", source, re.S)
    if not match:
        raise AssertionError(f"could not find {declaration}")
    return match.group(1)


def csv_field_count(format_string):
    """Count the columns a printf format string produces, ignoring the newline."""
    return format_string.replace("\\r\\n", "").count(",") + 1


def check_sweep_log():
    """Every row the sweep log writes must line up with its column header."""
    print("\nSweep log columns")
    source = LOG_C.read_text(encoding="utf-8")

    header = re.search(r'"# (index,[^"\\]*)', source)
    check("column header present", header is not None)
    if not header:
        return

    columns = csv_field_count(header.group(1))
    check("header names 10 columns", columns == 10, f"got {columns}")

    # Every data row is a quoted format string ending in a newline and starting
    # with the index; the empty columns in them are easy to miscount by hand.
    rows = re.findall(r'"(%lu,%lu,[^"]*?\\r\\n)"', source)
    check("three row formats (shot, command, other)", len(rows) == 3, f"got {len(rows)}")
    for row in rows:
        fields = csv_field_count(row)
        label = row.split(",")[2] or "<type>"
        check(f"{label} row has {columns} columns", fields == columns, f"got {fields}")


def main():
    """Run every check and return a process exit code."""
    source = PROTOCOL_C.read_text(encoding="utf-8")
    header = PROTOCOL_H.read_text(encoding="utf-8")

    print("Timings")
    for name, expected in (
        ("MILES_TAG_HEADER_MARK", HEADER_MARK),
        ("MILES_TAG_ONE_MARK", ONE_MARK),
        ("MILES_TAG_ZERO_MARK", ZERO_MARK),
        ("MILES_TAG_GAP", GAP),
    ):
        match = re.search(rf"#define {name}\s+(\d+)u", header)
        check(name, match and int(match.group(1)) == expected)

    print("\nCarrier frequencies")
    freqs = [int(v) for v in re.findall(r"\d+", c_array(source, "miles_tag_frequencies[]"))]
    check("38/40/56 kHz offered", freqs == [38000, 40000, 56000], str(freqs))

    print("\nDamage table (section 2.3.3)")
    values = [
        int(v)
        for v in re.findall(
            r"\d+", c_array(source, "miles_tag_damage_values[MILES_TAG_DAMAGE_COUNT]")
        )
    ]
    check("16 entries", len(values) == 16, f"got {len(values)}")
    check("matches the spec", values == DAMAGE_TABLE, str(values))

    print("\nTeams (section 2.3.1)")
    teams = re.findall(r'"([^"]+)"', c_array(source, "miles_tag_team_names[MILES_TAG_TEAM_COUNT]"))
    check("Red, Blue, Yellow, Green in order", teams == TEAMS, str(teams))

    print("\nCommands (section 2.2.4)")
    commands = re.findall(
        r'"([^"]+)"', c_array(source, "miles_tag_command_names[MILES_TAG_COMMAND_COUNT]")
    )
    check("24 entries", len(commands) == 24, f"got {len(commands)}")
    for index, name in COMMANDS.items():
        check(
            f"0x{index:02X} is {name}",
            index < len(commands) and commands[index] == name,
            commands[index] if index < len(commands) else "missing",
        )
    for index in RESERVED_COMMANDS:
        check(
            f"0x{index:02X} marked reserved",
            index < len(commands) and commands[index].startswith("Reserved"),
        )

    print("\nPlayer names (appendix A)")
    players = re.findall(
        r'"([^"]+)"', c_array(source, "miles_tag_player_names[MILES_TAG_NAMED_PLAYER_COUNT]")
    )
    check("60 named IDs", len(players) == 60, f"got {len(players)}")
    check("0x00 is Eagle", players and players[0] == "Eagle")
    check("0x01 is Joker", len(players) > 1 and players[1] == "Joker")
    check("0x3B is NUKE", len(players) == 60 and players[59] == "NUKE")

    print("\nShot packet layout")
    # Player 1, red team, damage code 0 -> the reference library's default shot.
    bits = shot_bits(1, 0, 0)
    check("14 bits", len(bits) == 14, f"got {len(bits)}")
    check("first bit is 0 (shot marker)", bits[0] == 0)
    timings = bits_to_timings(bits)
    check("30 timings (header + 14 bit pairs)", len(timings) == 30, f"got {len(timings)}")
    check("starts with the 2400us header", timings[0] == HEADER_MARK)
    check("every space is 600us", all(t == GAP for t in timings[1::2]))

    # Player 5, green team (0b11), damage code 15 (100 hp).
    bits = shot_bits(5, 3, 15)
    expected = [0, 0, 0, 0, 0, 1, 0, 1] + [1, 1, 1, 1, 1, 1]
    check("player 5 / green / 100hp packs correctly", bits == expected, str(bits))

    print("\nMessage packet layout")
    bits = message_bits(0x80, 50)  # Add Health, 50 hp
    check("24 bits", len(bits) == 24, f"got {len(bits)}")
    check("first bit is 1 (message marker)", bits[0] == 1)
    check(
        "terminator is 0xE8",
        bits[16:24] == [1, 1, 1, 0, 1, 0, 0, 0],
        str(bits[16:24]),
    )
    timings = bits_to_timings(bits)
    check("50 timings (header + 24 bit pairs)", len(timings) == 50, f"got {len(timings)}")

    # Air time is what the brute-force estimate is built on.
    duration = sum(timings)
    check("message air time under 40ms", duration < 40000, f"{duration}us")

    print("\nMessage IDs (section 2.2)")
    for name, value in (
        ("MilesTagMsgAddHealth", 0x80),
        ("MilesTagMsgAddRounds", 0x81),
        ("MilesTagMsgCommand", 0x83),
        ("MilesTagMsgSystemData", 0x87),
        ("MilesTagMsgClipsPickup", 0x8A),
        ("MilesTagMsgHealthPickup", 0x8B),
        ("MilesTagMsgFlagPickup", 0x8C),
    ):
        match = re.search(rf"{name} = 0x([0-9A-Fa-f]+)", header)
        check(f"{name} = 0x{value:02X}", match and int(match.group(1), 16) == value)

    print("\nBuffer sizing")
    match = re.search(r"#define MILES_TAG_MAX_TIMINGS \(2u \+ (\d+)u \* 2u\)", header)
    check(
        "max timings covers the 24-bit message",
        match and 2 + int(match.group(1)) * 2 >= 50,
    )

    check_sweep_log()

    print()
    if failures:
        print(f"{len(failures)} check(s) failed:")
        for name in failures:
            print(f"  - {name}")
        return 1

    print("All checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
