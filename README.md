# MilesTag II TX — Flipper Zero

Transmit any MilesTag II laser tag infrared signal from the Flipper's built-in IR
LED: shots with a chosen player ID, team colour and damage; health and ammo
grants; the full admin command set; game-box pickups; and arbitrary raw messages.
When you don't know what a tagger expects, a brute-force mode sweeps every
combination for you.

No extra hardware — the Flipper's own IR LED and the standard firmware are enough.

## Protocol

Implemented from *The MilesTag 2 protocol* (Christopher Malton, April 2011) and
cross-checked against the [ncmreynolds/milesTag](https://github.com/ncmreynolds/milesTag)
Arduino library, a copy of which is vendored in [milesTagArduino/](milesTagArduino/)
for reference. That copy is **not** compiled — see the explicit `sources=` list in
[application.fam](application.fam), because the default source glob is recursive
and would otherwise try to build the Arduino `.cpp`.

### Line coding

Every symbol is a burst of carrier followed by a fixed gap:

| Symbol | Carrier on | Gap |
| ------ | ---------- | --- |
| Header | 2400 µs | 600 µs |
| `1` | 1200 µs | 600 µs |
| `0` | 600 µs | 600 µs |

Carrier is 38, 40 or 56 kHz (56 kHz is the MilesTag II default). Bits go out MSB
first.

### Shot packet — 14 bits

```text
[header][0 ppppppp][tt dddd]
```

* `0` marks the packet as a shot
* `ppppppp` — player ID, 0–127 (IDs 0–59 have stock display names: 000 Eagle, 001 Joker, …)
* `tt` — team: 0 Red, 1 Blue, 2 Yellow, 3 Green
* `dddd` — damage code, not the damage number:

| Code | HP | Code | HP | Code | HP | Code | HP |
| ---- | -- | ---- | -- | ---- | -- | ---- | -- |
| 0 | 1 | 4 | 7 | 8 | 20 | 12 | 40 |
| 1 | 2 | 5 | 10 | 9 | 25 | 13 | 50 |
| 2 | 4 | 6 | 15 | 10 | 30 | 14 | 75 |
| 3 | 5 | 7 | 17 | 11 | 35 | 15 | 100 |

Only the top six bits of the second byte are transmitted, which is why the packet
is 14 bits rather than 16.

### Message packet — 24 bits

```text
[header][1 mmmmmmm][dddddddd][0xE8]
```

| ID | Message | Data byte |
| -- | ------- | --------- |
| `0x80` | Add Health | 1–100 |
| `0x81` | Add Rounds | 1–100 |
| `0x83` | Command | command byte, see below |
| `0x87` | System Data | cloning / scoring payloads (not sent by this app) |
| `0x8A` | Clips pickup | ammo box ID 0–15 |
| `0x8B` | Health pickup | medic box ID 0–15 |
| `0x8C` | Flag pickup | flag ID 0–15 |

Commands carried by `0x83`:

| Byte | Command | Byte | Command |
| ---- | ------- | ---- | ------- |
| `0x00` | Admin Kill | `0x0B` | Explode Player |
| `0x01` | Pause/Unpause | `0x0C` | New Game (ready) |
| `0x02` | Start Game | `0x0D` | Full Health |
| `0x03` | Restore Defaults | `0x0F` | Full Armour |
| `0x04` | Respawn | `0x14` | Clear Scores |
| `0x05` | Immediate New Game | `0x15` | Test Sensors |
| `0x06` | Full Ammo | `0x16` | Stun Player |
| `0x07` | End Game | `0x17` | Disarm Player |
| `0x08` | Reset Clock | | |
| `0x0A` | Initialize Player | | |

`0x09`, `0x0E` and `0x10`–`0x13` are reserved by the spec; they are listed in the
app so every command's position still matches its data byte.

## Using the app

`Apps → Infrared → MilesTag II TX`. The main menu has three entries.

### Send Signal

One screen holds everything, and the rows below **Signal** change to match it —
you only ever see fields that the selected packet actually carries:

| Row | Values |
| --- | ------ |
| Frequency | 38 / 40 / 56 kHz |
| Signal | Shot, Add Health, Add Rounds, Command, Clips/Health/Flag Pickup, Custom Msg |
| *Shot* | Player ID, Team, Damage |
| *Add Health / Add Rounds* | Amount 1–100 |
| *Command* | Command (OK opens the full named list) |
| *Pickups* | Box / Flag ID 0–15 |
| *Custom Msg* | Message ID `0x80`–`0xFF`, Data byte (OK opens a hex editor) |
| Duty cycle | 33 / 40 / 50 % |
| Repeat | 1–10 copies per trigger pull |
| Repeat gap | 10–250 ms |
| Transmit | opens the fire screen |

Player ID and Command rows can be scrolled with left/right, or press OK to drill
into a full list — players are shown with their MilesTag names ("007 Blaze").

On the fire screen you get the encoded bytes, the exact bit string going out and
the carrier in use. **OK** fires; hold **OK** to keep firing.

### Brute Force

For when you don't know the player ID, team colour or carrier a target expects.

Every field is independently either **Fixed** or **Sweep all**, so you can:

| Player | Team | Result |
| ------ | ---- | ------ |
| Sweep all | Fixed → Red | all 128 player IDs, red team only — 128 packets |
| Fixed → 007 Blaze | Sweep all | player 7 against each of the four colours — 4 packets |
| Sweep all | Sweep all | every player in every colour — 512 packets |
| Fixed | Fixed | the single configured packet |

Choosing **Fixed** reveals a row underneath for the value to hold it at. Damage
works the same way, and frequency is either one carrier or **All** (×3). The
screen shows the resulting packet count and a time estimate before you commit.

Sweep dimensions are only offered for fields the packet actually has: player,
team and damage for shots, the command byte for commands.

Controls on the run screen:

* **OK** — Start, then Pause while running, then Resume. Pausing keeps your place,
  so resuming carries on at the next untried combination rather than starting over.
* **Left** — Reset back to the first combination (only when paused or finished).
* **Back** — pause and leave. The radio is never left running.

Editing the sweep settings and returning to the run screen discards a paused
position, since it no longer belongs to the sweep you're now describing.

### The sweep log

With **Log to file** on (the default), every combination that goes on the air is
appended to `/ext/apps_data/miles_tag_ii/sweep_log.csv`, in the order it was
tried — so when a tagger finally reacts you can look up what was being sent.

```text
# MilesTag II TX - sweep started 2026-08-01 14:32:05
# Player+Team @ all freqs, 1536 combinations
# index,freq_hz,type,player_id,player_name,team_id,team_name,damage_hp,command_hex,command_name
1,38000,Shot,0,Eagle,0,Red,1,,
2,38000,Shot,0,Eagle,1,Blue,1,,
...
# paused after 137 combinations
```

Lines are written after transmission, so the file only ever claims what actually
went out. Writes are buffered and flushed in batches — the SD card is much slower
than the radio, and a zero-delay sweep would otherwise be paced by the filesystem.
Pausing flushes and closes the file, so it's complete and readable on the card
while the sweep sits paused; resuming appends a new session header.

Runs accumulate in the one file. Delete it from the Flipper's file browser (or
over `qFlipper`) when you want a clean slate.

## Building

```sh
make setup            # install ufbt and pull the Flipper SDK
make build            # build the .fap
make install SD=E:    # copy it to an SD card mounted at E:
make launch           # build and run on a USB-connected Flipper
make test             # host-side protocol checks
make clean
```

`ufbt` builds into `~/.ufbt/build/miles_tag_ii.fap`, a per-user cache shared by
every ufbt app — `make install` and `make launch` know where to find it.

`make test` runs [tests/test_protocol.py](tests/test_protocol.py), which
re-derives the packet formats from the specification and asserts that the tables
and layouts compiled into [miles_tag_protocol.c](miles_tag_protocol.c) still agree
with it. It needs no hardware.

## Layout

| File | Contents |
| ---- | -------- |
| [miles_tag_protocol.c](miles_tag_protocol.c) | packet encoding, lookup tables, formatters |
| [miles_tag_tx.c](miles_tag_tx.c) | IR worker thread: bursts and brute-force sweeps |
| [miles_tag_log.c](miles_tag_log.c) | buffered CSV log of every combination swept |
| [miles_tag_ii.c](miles_tag_ii.c) | app setup, views, scene wiring |
| [miles_tag_settings.c](miles_tag_settings.c) | settings saved to app data between runs |
| [scenes/](scenes/) | one file per screen |
| [views/](views/) | the custom fire and sweep screens |

Two Flipper GUI details are worth knowing if you extend this:

* `VariableItemList` runs its change **and** enter callbacks while holding its own
  view-model mutex, and `scene_manager_next_scene()` runs the outgoing scene's
  `on_exit` — which touches that same model. Navigating straight from a callback
  deadlocks, so row actions are deferred through custom events. (`Submenu` calls
  its callbacks outside the lock, so those scenes navigate directly.)
* A variable item's value count is a `uint8_t`, so a full 0–255 byte cannot be a
  row; the custom data byte uses a `ByteInput` screen instead.

## Legal

For use on your own equipment and in games you're part of. Transmitting admin
commands (Admin Kill, End Game, Restore Defaults) at someone else's laser tag
event is a good way to get thrown out of it.
