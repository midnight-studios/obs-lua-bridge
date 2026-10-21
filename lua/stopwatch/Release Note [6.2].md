# StopWatch 6.2: Release Note

StopWatch is MIT-licensed. Copyright (c) Midnight Studios. Licence text: [lua/LICENSE](https://github.com/midnight-studios/obs-lua-bridge/blob/master/lua/LICENSE) in the Lua Bridge repository.

Paths such as `tests/csct/…`, `tests/obs-auto/` and `docs/investigation/…` refer to StopWatch's private development repository and aren't published.

Changes between `StopWatch-[6.1].lua` (the final 6.1, with the B-078, B-083, B-086 and B-087 fixes) and `StopWatch-[6.2].lua`, for technical review.

**Basis.** The document is built from the actual diff of the final files, `git diff --no-index -w -U0` between the two: 69 hunks. Every hunk is assigned to the entries below:
- **The generator:** the mapping comes from `tests/csct/stage8/hunk_map_62.py`. It blames each 6.2 line in a hunk (the commit that wrote the line names the entry), and adds the register IDs the new lines name.
- **Overrides:**
  - lines of the three autosave helpers (`LB.save_allowed`, `LB.save_now`, `LB.deferred_save`) belong to B-079, whichever commit wrote them;
  - every line of the script header is NF-HEADER;
  - a line that calls the bridge (`LB.`, apart from the B-079 helpers) also belongs to F-LB, when a later fix rewrote it;
  - REV14's follow-up commit (`4ed31f7`) changed three fixes at once; its lines are split by place (`timing_signature`: B-086; the marker audio pause: B-084; the rest of `set_time_text`: B-085).
- **The checks:** the tool checks the coverage both ways (every hunk has an entry, every entry has a hunk and a heading here). It also checks that every commit that changed 6.2 is accounted for.

The full mapping is in the appendix.

**Conventions.**
- **IDs:** B-### are bugs in the register (`docs/investigation/BUG-REGISTER.md`). F-LB (the new feature) and NF-* (non-functional changes) are this note's own IDs.
- **Severity:** S1 (worst) to S4, as triaged; the register marks every 6.2 fix provisional.
- **Code references:** by function name. `LB.x` is a field of the new `LB` table. Line numbers appear only in the appendix and refer to each file.
- **Reviews:** "REVn" is an independent review of a phase (`tests/csct/review/REVn/`).
- **In-OBS checks** (in `tests/obs-auto/`, run in a separate test copy of OBS through the collection guard):
  - `bridge_checks.py`: PE1, CT1, BC1–BC7, EX1, NP1;
  - `fix62_checks.py`: G2, G3, G6;
  - `fix62b_checks.py`: H2, H4 (with H1, H3 and H5 for the fixes that are in the final 6.1 too);
  - the soak: `soak.py --bridge`, SOAK-3.
- **Harness tests:** in `tests/csct/tests/`; 272 pass at this release (6.1: 260 pass, and 12 are expected failures for the fixes made in 6.2 only). The test index in the appendix lists them by entry.
- **The design:** the integration follows the project file `docs/STOPWATCH-6.2-LUABRIDGE-UPGRADE.md` (the spec), with its sections cited as §n. The user and integrator reference is `STOPWATCH-LUABRIDGE.md`, published with this note.

**How this release was made.** StopWatch is the maintainer's own script. For 6.2, the maintainer used an AI coding tool, Claude Code (Anthropic), to write most of the Lua Bridge integration, the fixes and their tests.
- **Reviews:** independent AI agents reviewed Phases 1 to 3 (REV8, REV9, REV12) and the later fixes (REV10, REV11, REV14, REV15) before the maintainer accepted them. Another reviewed the Phase 4 documents (REV13), and others verified this document both ways (VER13, VER14).
- **The maintainer** set the rules for the work, made every decision about how the script should behave (the spec's §12 and the decisions recorded in `CHANGELOG.md`), checked the results after each phase, and tested in OBS (M1, the dock by eye).
- **Tests:** the integration is covered by the harness, including a differential that compares 6.2 with 6.1 with and without a fake plugin, and by automated checks in a separate test copy of OBS.

## 1. Summary

| Category | Entries | Hunks |
|-|-|-|
| New feature | 1 (F-LB) | 53 |
| Bug fixes | 4 (B-079, B-080, B-084, B-085) | 17 (three shared with F-LB: H003, H020, H022) |
| Non-functional | 2 (NF-VERSION, NF-HEADER) | 2 |
| Unexplained changes | 0 | 0 |

| ID | Title | Category | Severity |
|-|-|-|-|
| F-LB | The Lua Bridge integration (dock and remote control) | New feature | — |
| B-079 | With Autoload in Count Up, every pause saved the whole scene collection | Bug fix | S3 (provisional) |
| B-080 | Count Down to a date: Reset showed a stale remaining time until Start | Bug fix | S3 (provisional) |
| B-084 | Count Up turned down: the stop at 0 ran the expiry path | Bug fix | S3 (provisional) |
| B-085 | Overtime with the direction toggled: counting back through 0 stayed in overtime ("+-00:00:01") | Bug fix | S3 (provisional) |
| NF-VERSION | Version number | Non-functional | — |
| NF-HEADER | Script header (6.2 section) | Non-functional | — |

**Not in the diff:**
- **B-078** (Count Down: Reset after a cosmetic settings edit returned to the paused time) was fixed in 6.2 first (`6047d14`). The same fix is in the final 6.1 (`376c850`), so the two files agree there and the diff has no hunk for it. Its tests are in `regress_6_2.lua`; see the 6.1 release note.
- **B-086** (changing the Timer Expire Event didn't reset the timer), **B-083** (Count Up showed the Timer Expired Audio and End Audio Duration) and **B-087** (after Reset or Import in the panel, the direction button offered the direction of before) were found on 2026-10-17. All three shipped in the first 6.1 file, and the final code is identical in both files (B-086: `dcb0ee4` and REV14's follow-up `4ed31f7` in both; B-083: `5c6c84b` in both; B-087: `8f3553c` in 6.2, then REV15's follow-up `fedf3b4` in both). So the diff has no hunk for them, apart from their 6.2 header lines (NF-HEADER). See the 6.1 release note.
- **B-081** (an overtime split without "+") is open, and kept as in 6.1 by decision (§5).
- **B-082** is closed: not a bug.

**Without the plugin, 6.2 behaves as 6.1** apart from the bug fixes:
- **The differential:** every scenario in `tests/csct/scenarios.lua` gives the same output on 6.2 as on 6.1, both without the plugin and with the fake plugin present (the bridge publishes, but changes nothing).
- **NP1** checks in OBS, with the plugin's DLL disabled for one session, that the timer runs and that the log has one Lua Bridge line, "not installed". It doesn't compare with 6.1.
- **B-079, B-080, B-084 and B-085** change behaviour only in the cases they fix (§3, §4).

## 2. New feature

### F-LB: The Lua Bridge integration (dock and remote control)

**What it does.** With the optional Lua Bridge plugin (`0.9.0-beta1`, helper `luabridge.lua` 1.0.0, `api_version` 1), StopWatch registers the owner `stopwatch` (or `stopwatch.<id>`) and provides:
- **a dock section** in Docks → Lua Bridge: the time, the status, Start / Pause, Reset, the optional Split / Set / Direction row, a Seconds box with − / +, and the preset rows;
- **17 commands** for the dock, obs-websocket clients (`RunCommand`) and other scripts;
- **24 state keys** (`GetState`, `StateChanged`) and **7 events** (`CustomEvent`).

The full list, with arguments and data, is in `STOPWATCH-LUABRIDGE.md`. Without the plugin, nothing is registered: `LB.detect` tries `luabridge_get_info` once, and no other bridge call is made (NP1, and the `bridge_6_2` test "helper: none found without the plugin; no bridge call, nothing logged above info").

**The code.** Everything new is inside one new top-level local, the `LB` table. The script uses 189 of LuaJIT's 200 top-level locals; 6.1 used 188.

**Part A, the declaration** (`local LB`, H003, after `local ctx`):
- the `LB` table, with no-op stubs for `LB.publish`, `LB.event` and `LB.note_text`, so the core hooks can call them before Part C replaces them;
- loading the helper inside `pcall`: first `luabridge.lua` next to the script, otherwise the copy installed with the plugin (`obs_get_module("obs-lua-bridge")` and `obs_get_module_data_path`; decision D2, option D). There is no embedded copy;
- the table also holds the B-079 state (`save_ns`, `save_pending`).

**Part C, the implementation** (H065, one block right before `script_update`; it defines no top-level locals). These are its functions:

| Function | Role |
|-|-|
| `LB.hms`, `LB.elapsed` | `display` as `HH:MM:SS` (negative with `-`), and `elapsed` as whole seconds truncated toward zero |
| `LB.note_text` | Keeps the time text `set_time_text` built, for the `time` key |
| `LB.moved`, `LB.status` | Whether the clock differs from its Reset value (0.01 s tolerance), and the status order (§7.5.1): disabled, no_source, overtime, running, expired, paused, stopped |
| `LB.time_values`, `LB.values` | The 4 time keys, and all 24 keys |
| `LB.preset_label` | The preset captions (`+30 s`, `−1 min`, `(2 left)`) |
| `LB.publish` | Sends the changed keys. A redraw sends only the time keys, at most every 200 ms but at once on a new whole second. Forced publishes send every changed key. After a failed update, every key is marked unknown (`LB.UNKNOWN`), so the next publish sends it again |
| `LB.event` | Emits an event with `display`, `elapsed` and `mode` added. Never before loading has finished, or while the script is disabled |
| `LB.adjust_clock` | `add`, `subtract` and `set_time`: range checks; blocked when Timer Manipulation is Disabled (D3); `subtract` through `subtract_time` |
| `LB.presets_on`, `LB.commands`, `LB.on_command` | The 17 handlers, and the dispatcher: ignored before loading has finished; only a republish while disabled; `pcall`, with one warning on an error; a forced publish after every handled command (and while disabled) |
| `LB.owner_for`, `LB.layout`, `LB.registration` | The owner from the Instance ID; the dock layout signature (`split`, `set`, `dir`, `presets`); and the registration: 17 commands with labels and tooltips, and the dock built for the current settings |
| `LB.detect`, `LB.status_text`, `LB.panel_texts`, `LB.log_status` | Detecting the plugin without the helper (`luabridge_get_info`); the status line; the panel's info texts (set as defaults, never saved); one log line, at info level unless a registration was refused |
| `LB.setup`, `LB.shutdown` | Register, re-register or unregister from the settings: re-register only when the owner or the layout changed, unregistering first; `bridge.shutdown()` at unload |

**Part B, the hooks** in existing functions (one or a few lines each; §7.9):

| Function | Added | Hunks |
|-|-|-|
| `set_time_text` | `LB.note_text(text)`; clears `LB.end_text` when the time shows again; a throttled `LB.publish` at the end (B-085 made it a forced publish, every changed key, on the tick that leaves overtime); `LB.pause_reason = "zero"` before a Count Up's stop at 0 (B-084) | H015, H016, H020, H022 |
| `activate` | `lb_was_running`; at the end the `started` or `paused` event (not for an expiry or a Reset: `LB.expiring`, `LB.resetting`), then a forced publish | H025, H026 |
| `time_mark_check` | the `marker` event; `LB.pause_reason = "marker"` before the marker pause; a forced publish after `update_media_names` (the next marker times, REV12) | H009–H011 |
| `timer_ended` | `LB.expiring` around its stop; the `expired` event before the expire actions; `LB.expired_seen`; `LB.end_text` for a TIMER END TEXT expiry; a forced publish at the end | H059–H062 |
| `reset` | `LB.resetting` around its stop; at the end `LB.expired_seen = false`, the `reset` event and a forced publish | H050, H051 |
| `split_time` | the `split` event, with the mark and the interval as the split text shows them (`raw_time`), and a forced publish | H053 |
| `start_overtime` | the `overtime` event and a forced publish | H057 |
| `set_stopwatch`, `update_timer_direction`, `mili`, `switch_to_expire_scene` | a forced publish at the end | H031, H033, H035, H058 |
| `sec_add_1_update` … `sec_add_3_update`, `sec_sub_1_update` … `sec_sub_3_update` | a forced publish at the end (the captions count the uses) | H037, H039, H041, H044, H046, H048 |

**Part D, the entry points:**
- **`init`:** `LB.loaded = false` at its start. After `timer_activation_signals()`, `LB.loaded = true` and `LB.setup(LB.pending or ctx.propsSet)`, before the start-up activation checks (H054, H055). So the start-up `reset` emits no event, also when `init` runs more than once while OBS loads.
- **`script_update`:** `LB.setup(settings)` in both paths, before the early return for a non-timing change and at the end (H066, H067).
- **`script_unload`:** `LB.shutdown()` first (H069).
- **`script_defaults`:** `enable_property_lua_bridge` on and `lua_bridge_instance_id` empty (H068).
- **`script_properties`:**
  - the "Lua Bridge (dock and remote control)" checkbox in Disable Features, with `property_onchange` as its callback (H064);
  - the "Lua Bridge" group, before Debugging and Backup, with the Status and Help info texts (only where `OBS_TEXT_INFO` exists, OBS 28 and later) and the Instance ID (H063).
- **`property_onchange`:** shows the group only while the feature is on, in the Advanced layout with Timer Settings expanded (like Backup and Debugging), and refreshes the panel texts (H056).

**Function headers** (comment-only hunks): the Scope lines of the functions the commands call now name `LB.commands` or `LB.adjust_clock` as callers. These are `start`, `pause`, `startpause`, `set_stopwatch`, `update_timer_direction`, `mili`, `sec_add_1_update` to `sec_add_3_update`, `subtract_time`, `sec_sub_1_update` to `sec_sub_3_update`, `reset` and `split_time` (H027–H030, H032, H034, H036, H038, H040, H042, H043, H045, H047, H049, H052). Every new function has a header in the 6.1 style.

**Decisions it implements** (spec §12 and the decisions recorded in `CHANGELOG.md`):
- **D2:** option D, the helper next to the script, otherwise the plugin's copy;
- **D3:** `add` / `subtract` / `set_time` are blocked only when Timer Manipulation is on and set to Disabled;
- **D4:** the feature is on by default;
- **D5:** no dock-only timer: without a Timer Source the status is `no_source`;
- **D6:** the section title "StopWatch";
- **D7:** a 200 ms throttle;
- **D9:** B-079;
- **2026-10-15:**
  - `text` is what the source shows;
  - `expired` only after an expiry since the last Reset;
  - `next_marker_a/b` are published whenever markers fire;
  - the group is hidden in the Basic layout;
  - `last_split` and the split event as the split text shows them.

**Reviews:** REV8 (Phase 1), REV9 (Phase 2), REV12 (Phase 3). REV8's findings were in the harness and tooling. The script fixes from REV9 and REV12 are in `eb8053e` and `3e43ae2`, listed in A.1.

**Tests:**
- **Harness:** 65 bridge tests (`bridge_6_2`, `bridge_6_2_p3`) against a fake plugin in `tests/csct/mock_obslua.lua` (its self-tests are in `mock_lua_bridge.lua`). They cover no plugin, an old plugin and failing calls; registration timing; the publish throttle and diff; every command; the event order (§11.4); negative time; the unload order; and two instances.
- **The differential,** with and without the fake plugin (§1).
- **In OBS:**
  - **CT1:** the bridge repository's contract test passes (`test_stopwatch_by_owner`, `test_stopwatch_validates_args`);
  - **BC1:** finding the plugin's helper; **BC2:** events and state; **BC3:** the decisions of 2026-10-15 (a 0 s Count Down, the end text); **BC4:** one update a second with a whole-second format; **BC5:** the event exactness for every expire event; **BC6:** the extended commands and keys; **BC7:** two instances;
  - **EX1:** the exit check;
  - **NP1:** no plugin;
  - **SOAK-3:** one hour with a websocket client polling `GetState` once a second, on a 60 s Source List countdown with Marker A and a split every 7 minutes. The final run, on this release's code (2026-10-19, after all fixes, with the soak script as fixed by REV13), passed all 8 checks:
    - **GetState:** every result was valid (3,594 polls; average latency 0.9 ms, 99th percentile 2.2 ms, maximum 6.2 ms).
    - **Events:** exact: 60 `expired`, 61 `reset`, 61 `started`, 60 `marker`, 8 `split`, 1 `paused` (the final one).
    - **Rate:** 18,312 `StateChanged` events, about 5 a second, with hundredths shown.
    - **Memory:** private memory stayed flat, 245.6 MB at 5 minutes and 248.4 MB at the end, with short peaks of 263–264 MB that came back down.
    - **The session log:** 0 Lua errors, 0 sources left, 1 leak.

    A first run on 2026-10-16, before the fixes of 2026-10-17 to 19, gave the same results.

  Every session logged the baseline leak count (1), except one PE1 session in Phase 2 that logged 2. Three repeats of PE1 logged 1 each, and the 2 was not seen again.

## 3. Bug fixes

### B-079: With Autoload in Count Up, every pause saved the whole scene collection (S3, provisional)

- **Symptom:** with "Autoload last time stamp when OBS starts" in Count Up, each pause wrote the whole scene collection file. A run of Start / Pause, from hotkeys or remote control, wrote it many times a second: 9 writes for 10 pauses in the test OBS (G2 on 6.1).
- **Cause:** every forced autosave in `autosave_timestamp` called `obs_frontend_save`.
- **Fix** (decision D9):
  - **The time:** a forced autosave still stores the time in the settings every time.
  - **The 2-second window:** `autosave_timestamp` asks `LB.save_allowed` before saving the collection, and saves at most once every 2 seconds.
  - **Inside the window:** a save there schedules one deferred save at the window's end (`LB.deferred_save`, which calls `LB.save_now`). So the last pause of a burst is always saved, at most 2 seconds late.
  - **Timer clean-up:** `remove_all_timers` makes a pending deferred save at once, then removes its timer (H008), so a timing change never drops it.
  - **Unload:** at Reload Scripts, OBS keeps the settings object, which already holds the time. At OBS exit, OBS's own save covers it.
- **Code:**
  - `autosave_timestamp` (H004, H005);
  - `LB.save_allowed`, `LB.save_now`, `LB.deferred_save` (H006);
  - `remove_all_timers` (H007, H008);
  - the `LB` fields `save_ns` and `save_pending` (H003).
- **Tests:**
  - **Harness:** `regress_6_2` has 2 tests that fail on 6.1 and 1 guard.
  - **In OBS:** G2 gives 1 write during the burst and 1 deferred write on 6.2 (9 on 6.1). G3: Reload Scripts while the save is deferred keeps the last paused time.

### B-080: Count Down to a date: Reset showed a stale remaining time until Start (S3, provisional)

- **Symptom:** a countdown to a date 2 hours away, loaded at OBS start. One hour later, Reset showed 02:00:00 instead of about 01:00:00. Start then jumped to the real remaining time.
- **Since:** 5.9 at least. It is not caused by the B-078 fix: REV10's probes on 5.9, 6.0, the original 6.1 and the fixed 6.1 show it in every version.
- **Cause:** on Reset, `update_timer_settings` first set the fresh date difference, then overwrote it with `default_seconds`, the difference at the last timing change.
- **Fix:** in `update_timer_settings`, a date countdown sets `default_seconds` to the freshly calculated difference each time the timing settings are applied: Reset, an activation with Auto reset, a scene expiry, a timing change (H024; the header in H023).
  - **Visible consequence (REV11):** after a date countdown has expired, Reset shows 00:00:00, the real remaining time.
  - **Unchanged:** hours/minutes/seconds countdowns.
- **Tests:**
  - **Harness:** `regress_6_2` has 2 tests, both failing on 6.1: Reset 1 hour after load, and Reset after a timing change while running.
  - **In OBS:** G6 passes on 6.2 and fails on 6.1, as expected.
- **The decision:** fix in 6.2 only, own commit (`bfab7f4`); 6.1 unchanged. Review: REV11.

### B-084: Count Up turned down: the stop at 0 ran the expiry path (S3, provisional)

- **Symptom:** in Count Up with the direction toggled down and "Prevent Negative Time Value" on, the timer stops at 0. It went through the expiry path: the marker notes were hidden, looping marker audio was stopped, a pending marker colour reset ran on, and 6.2 emitted the bridge event `expired`.
- **Rule and decisions** (maintainer, 2026-10-17): only a Count Down expires. This stop is a pause, as Pause does it, and the bridge reports `paused` with the reason `zero`; the published `expired` stays false.
- **Since:** 5.9 (the expiry path); the `expired` event since 6.2. Older than 6.1, so it is fixed in 6.2 only.
- **Cause:** `set_time_text` called `timer_ended` for any timer that reached 0 while running, and hid the notes and set `timer_expired` in any mode.
- **Fix:** in `set_time_text`, `timer_ended`, the note hiding and `timer_expired` are for Count Down only. A Count Up that reaches 0 runs `activate(false)` with the bridge reason `zero`, and pauses the playing marker audio, as `pause` does (`pause` is defined later in the file; REV14 found that the first version played the audio instead of pausing it) (H012, H018–H021).
- **Tests:**
  - **Harness:** `regress_6_2` has 3 tests that fail before the fix (the events and state; no expiry actions; the marker audio paused), plus 1 guard (a Count Down still expires).
  - **In OBS:** H2 passes on 6.2 and fails on the 6.2 before the fix (`started,expired`, the note hidden).
- **Reviews:** REV14, REV15.

### B-085: Overtime with the direction toggled: counting back through 0 stayed in overtime (S3, provisional)

- **Symptom:** a Count Down with "Continue counting (overtime)" and the "+" on; 3 s into overtime, the direction is toggled. The clock counted back through 0 and went on as "+-00:00:01", "+-00:00:02" … in the overtime colour, and the bridge kept `status` overtime.
- **Where the "-" came from:** nothing cleared `overtime_active` on the toggle. `set_time_text` skips its clamp at 0 and its end check while it is set, so the clock went below 0, and negative time is shown with "-" (B-018), after the overtime "+".
- **Expected** (maintainer, 2026-10-17): on crossing 0 the timer leaves overtime, with no "+" and no "-", and the colour recalculated as if the timer had never expired. Decision: the colour it would show at that time had it counted down normally.
- **Since:** 6.0 ("+-1:59:58" there); 5.9 has no overtime. Older than 6.1, so it is fixed in 6.2 only.
- **Fix** (`set_time_text`, H012–H014, H017, H022): below 0 in overtime the timer leaves overtime.
  - The clock becomes Count Down time again, counting up the way it was going.
  - Every pending marker colour reset is dropped.
  - The colour is that of the nearest marker above the time and at or below the start time (A or B, colour on), unless its "reset text colour" time has run out since it. If not, it is the normal colour. If the marker's reset time hasn't run out, the reset runs for the time it has left (REV14).
  - The markers are recalculated, and the bridge state is published at once (a forced publish: every changed key).
  - The clamp at 0 and the end check skip that one tick, which would otherwise start overtime again.
- **Tests:**
  - **Harness:** `regress_6_2` has 5 tests that fail before the fix or its follow-up:
    - plain, with the published state;
    - Marker A applying;
    - Marker A's reset run out;
    - the reset running for the time it has left;
    - a marker above the start time.

    Plus 1 guard: toggling twice in overtime stays in overtime.
  - **In OBS:** H4 (plain, Marker A, and Marker A with a reset window) passes on 6.2. The 6.2 before the fix fails it ("+-00:00:00,49", the overtime colour, `overtime` true; checked with the first two sessions, before the third was added).
- **Reviews:** REV14, REV15.

## 4. Behaviour changes

All are bug fixes (§3). The integration adds none without the plugin.

- **B-079:** with Autoload in Count Up, the collection is saved at most once every 2 seconds. The time is still stored at every pause, and the last pause of a burst is saved at the end of the window.
- **B-080:** Reset of a date countdown shows the real remaining time; after the date has passed, 00:00:00.
- **B-084:** a Count Up turned down with "Prevent Negative Time Value" stops at 0 as Pause does: the marker notes stay, playing marker audio is paused (and resumes on Start), and a pending colour reset waits for the next Start.
- **B-085:** counting back out of overtime through 0 leaves overtime and counts Count Down time again, with the colour recalculated.

**With the plugin:** the commands follow the script's own rules, as the buttons and hotkeys do. Examples: `split` only while running; `set` only in Count Up; the presets with their limits; nothing while disabled. A Reset from the dock is a user Reset, which also clears the Autoload time (B-037).

## 5. Known limits

- **D2, macOS / Linux:** finding the helper installed with the plugin (`obs_get_module_data_path`) is tested on Windows only. If the status says the helper is missing, `luabridge.lua` next to the script always works. Lua Bridge now documents loading the installed helper, and says it is tested on Windows only so far: "Using the helper installed with the plugin" in its [API.md](https://github.com/midnight-studios/obs-lua-bridge/blob/master/docs/API.md#the-helper-library-luabridgelua).
- **Two copies with the same Instance ID:** they share the owner. Only one reaches the dock, and the plugin logs "already registered and active". Documented as a user error (§7.12).
- **B-081, an overtime split without "+":** the split text source shows the elapsed overtime without "+" (as in 6.1), and the split event and `last_split` report it the same way. Decision 2026-10-15: keep it, for a later version.
- **OBS older than 28:** no Status and Help info texts in the panel. The plugin itself needs OBS 31.1.
- **No Timer Source** (D5): commands change the time, but nothing renders, and markers and expiry don't run, as in 6.1. The status is `no_source`.
- **The panel's direction button with the panel open (B-087, in the final 6.1 too):** a direction change by hotkey, the dock or a timer activation shows on the button only after Start, Pause, Reset, the direction button or a setting with a modified callback is used in the panel, or the panel is reopened. OBS refreshes a script's panel only from the panel's own buttons and settings.

## 6. Non-functional changes

### NF-VERSION: Version number

`gversion` is `"6.2"` (H002).

### NF-HEADER: Script header (6.2 section)

The script header has a new 6.2 section: new features, fixes, known limits, and notes with the "How this release was made" disclosure (D8) and the credits (H001). It is comment-only: luacheck's lexer gives identical tokens. The 6.1 section is identical to the final 6.1's.

<!-- APPENDIX -->
## 7. Appendix

Generated by `python -I tests/csct/stage8/build_release_note_62.py`.

### A.0 Verification

**VER14** (2026-10-19, report in `tests/csct/review/VER14/REPORT.md`) checked this note both ways at `bf9df12` (69 hunks), after B-084, B-085 and the move of B-083, B-086 and B-087 into the final 6.1.
- **Diff → note:**
  - `hunk_map_62.py --markdown --check-note` matches A.1 (69 rows) and reports no problems;
  - an independent `git blame -w` gives the same commits for every hunk;
  - every hunk number cited in §1–§6 points to the hunk that matches the description;
  - F-LB 53, B-079 6, B-080 2, B-084 5, B-085 5, NF-VERSION 1, NF-HEADER 1; 17 bug-fix hunks, three of them shared with F-LB.
- **Note → code:**
  - B-084 and B-085 match the code, the tests and the in-OBS checks;
  - B-083, B-086 and B-087 are identical in both files, so they have no hunk apart from header lines;
  - the harness passes (6.2: 272).
- **Findings:** wording only: the commits behind "identical", the H4 sessions, "every changed key" for a forced publish, the B-084 hook in the Part B table, and the known limit's wording. All are corrected.

**Earlier verifications.**
- **VER13** (2026-10-16): verified the first version at `439a677`, 61 hunks: 2 errors, 5 inaccuracies and 4 wording points, all corrected (`tests/csct/review/VER13/REPORT.md`).

### A.1 Hunk map

Generated by `python -I tests/csct/stage8/hunk_map_62.py --markdown` from `git diff --no-index -w -U0 StopWatch-[6.1].lua StopWatch-[6.2].lua` (the final files). Line numbers are those of each file. "Function" is the function that contains the hunk, or, for a comment-only hunk, the function its header describes. 69 hunks. Hunks per entry: F-LB 53, B-079 6, B-080 2, B-084 5, B-085 5, NF-VERSION 1, NF-HEADER 1.

| Hunk | 6.1 lines | 6.2 lines | Class | Function | New definitions | Entries | Commits |
|-|-|-|-|-|-|-|-|
| H001 | after 6 | 7–47 | comment, added | script header | — | NF-HEADER | `0525bfa`, `1debf79`, `4ed31f7`, `5c6c84b`, `7814a4e`, `8e8e955`, `a196992`, `bea7712`, `dcb0ee4`, `fedf3b4` |
| H002 | 137 | 178 | code | top level | — | NF-VERSION | `1debf79` |
| H003 | after 261 | 303–370 | code, added | top level | `local LB` | B-079, F-LB | `0210ea0`, `0d6230f`, `4c7f90b`, `eb8053e` |
| H004 | 1286 | 1395 | comment | `autosave_timestamp` (header) | — | B-079 | `dd1438c` |
| H005 | after 1300 | 1410 | code, added | `autosave_timestamp` | — | B-079 | `dd1438c` |
| H006 | after 1302 | 1413–1471 | code, added | top level | `LB.save_allowed`, `LB.save_now`, `LB.deferred_save` | B-079 | `dd1438c`, `eb8053e` |
| H007 | 2550 | 2719 | comment | `remove_all_timers` (header) | — | B-079 | `dd1438c` |
| H008 | after 2570 | 2740–2741 | code, added | `remove_all_timers` | — | B-079 | `dd1438c` |
| H009 | after 3938 | 4110 | code, added | `time_mark_check` | — | F-LB | `665487f` |
| H010 | after 4029 | 4202 | code, added | `time_mark_check` | — | F-LB | `3e43ae2` |
| H011 | after 4046 | 4220 | code, added | `time_mark_check` | — | F-LB | `eb8053e` |
| H012 | 4723 | 4897 | comment | `set_time_text` (header) | — | B-084, B-085 | `ed7fb2e` |
| H013 | after 4737 | 4912–4966 | code, added | `set_time_text` | — | B-085 | `4ed31f7`, `ed7fb2e` |
| H014 | 4741 | 4970 | code | `set_time_text` | — | B-085 | `ed7fb2e` |
| H015 | after 4813 | 5043 | code, added | `set_time_text` | — | F-LB | `4c7f90b` |
| H016 | after 4815 | 5046 | code, added | `set_time_text` | — | F-LB | `0210ea0` |
| H017 | 4831 | 5062 | code | `set_time_text` | — | B-085 | `ed7fb2e` |
| H018 | 4838 | 5069 | code | `set_time_text` | — | B-084 | `7814a4e` |
| H019 | 4852 | 5083 | code | `set_time_text` | — | B-084 | `7814a4e` |
| H020 | after 4853 | 5085–5095 | code, added | `set_time_text` | — | B-084, F-LB | `4ed31f7`, `7814a4e` |
| H021 | 4870 | 5112 | code | `set_time_text` | — | B-084 | `7814a4e` |
| H022 | after 4881 | 5124 | code, added | `set_time_text` | — | B-085, F-LB | `ed7fb2e` |
| H023 | 5103 | 5346 | comment | `update_timer_settings` (header) | — | B-080 | `bfab7f4` |
| H024 | after 5163 | 5407–5408 | code, added | `update_timer_settings` | — | B-080 | `bfab7f4` |
| H025 | after 5362 | 5608 | code, added | `activate` | — | F-LB | `4c7f90b` |
| H026 | after 5447 | 5694–5704 | code, added | `activate` | — | F-LB | `4c7f90b` |
| H027 | 5452 | 5709 | comment | `start` (header) | — | F-LB | `3e43ae2` |
| H028 | 5480 | 5737 | comment | `pause` (header) | — | F-LB | `3e43ae2` |
| H029 | 5509 | 5766 | comment | `startpause` (header) | — | F-LB | `3e43ae2` |
| H030 | 5603 | 5860 | comment | `set_stopwatch` (header) | — | F-LB | `3e43ae2` |
| H031 | after 5666 | 5924 | code, added | `set_stopwatch` | — | F-LB | `665487f` |
| H032 | 5781 | 6039 | comment | `update_timer_direction` (header) | — | F-LB | `3e43ae2` |
| H033 | after 5821 | 6080 | code, added | `update_timer_direction` | — | F-LB | `665487f` |
| H034 | 5841 | 6100 | comment | `mili` (header) | — | F-LB | `3e43ae2` |
| H035 | after 5881 | 6141 | code, added | `mili` | — | F-LB | `665487f` |
| H036 | 6020 | 6280 | comment | `sec_add_1_update` (header) | — | F-LB | `3e43ae2` |
| H037 | after 6038 | 6299 | code, added | `sec_add_1_update` | — | F-LB | `4c7f90b` |
| H038 | 6044 | 6305 | comment | `sec_add_2_update` (header) | — | F-LB | `3e43ae2` |
| H039 | after 6062 | 6324 | code, added | `sec_add_2_update` | — | F-LB | `4c7f90b` |
| H040 | 6068 | 6330 | comment | `sec_add_3_update` (header) | — | F-LB | `3e43ae2` |
| H041 | after 6086 | 6349 | code, added | `sec_add_3_update` | — | F-LB | `4c7f90b` |
| H042 | 6152 | 6415 | comment | `subtract_time` (header) | — | F-LB | `3e43ae2` |
| H043 | 6167 | 6430 | comment | `sec_sub_1_update` (header) | — | F-LB | `3e43ae2` |
| H044 | after 6185 | 6449 | code, added | `sec_sub_1_update` | — | F-LB | `4c7f90b` |
| H045 | 6191 | 6455 | comment | `sec_sub_2_update` (header) | — | F-LB | `3e43ae2` |
| H046 | after 6209 | 6474 | code, added | `sec_sub_2_update` | — | F-LB | `4c7f90b` |
| H047 | 6214 | 6479 | comment | `sec_sub_3_update` (header) | — | F-LB | `3e43ae2` |
| H048 | after 6232 | 6498 | code, added | `sec_sub_3_update` | — | F-LB | `4c7f90b` |
| H049 | 6411 | 6677 | comment | `reset` (header) | — | F-LB | `3e43ae2` |
| H050 | after 6535 | 6802 | code, added | `reset` | — | F-LB | `4c7f90b` |
| H051 | after 6568 | 6836–6839 | code, added | `reset` | — | F-LB | `0210ea0`, `4c7f90b` |
| H052 | 7089 | 7360 | comment | `split_time` (header) | — | F-LB | `3e43ae2` |
| H053 | after 7105 | 7377–7381 | code, added | `split_time` | — | F-LB | `5244fc3`, `665487f` |
| H054 | after 7350 | 7627–7632 | code, added | `init` | — | F-LB | `4c7f90b` |
| H055 | after 7394 | 7677–7682 | code, added | `init` | — | F-LB | `4c7f90b` |
| H056 | after 8177 | 8466–8470 | code, added | `property_onchange` | — | F-LB | `5244fc3`, `665487f` |
| H057 | after 8654 | 8948–8949 | code, added | `start_overtime` | — | F-LB | `665487f`, `eb8053e` |
| H058 | after 8687 | 8983 | code, added | `switch_to_expire_scene` | — | F-LB | `665487f` |
| H059 | after 8739 | 9036 | code, added | `timer_ended` | — | F-LB | `4c7f90b` |
| H060 | after 8740 | 9038–9044 | code, added | `timer_ended` | — | F-LB | `0210ea0`, `4c7f90b` |
| H061 | after 8805 | 9110 | code, added | `timer_ended` | — | F-LB | `0210ea0` |
| H062 | after 8817 | 9123 | code, added | `timer_ended` | — | F-LB | `4c7f90b` |
| H063 | after 9814 | 10121–10135 | code, added | `script_properties` | — | F-LB | `3e43ae2`, `665487f` |
| H064 | after 10007 | 10329–10334 | code, added | `script_properties` | — | F-LB | `3e43ae2`, `665487f` |
| H065 | after 10285 | 10613–11183 | code, added | top level | `LB.hms`, `LB.elapsed`, `LB.note_text`, `LB.moved`, `LB.status`, `LB.UNKNOWN`, `LB.time_values`, `LB.values`, `LB.preset_label`, `LB.publish`, `LB.event`, `LB.adjust_clock`, `LB.presets_on`, `LB.commands`, `LB.on_command`, `LB.owner_for`, `LB.layout`, `LB.registration`, `LB.detect`, `LB.status_text`, `LB.panel_texts`, `LB.log_status`, `LB.setup`, `LB.shutdown` | F-LB | `0210ea0`, `3e43ae2`, `4c7f90b`, `5244fc3`, `665487f`, `eb8053e` |
| H066 | after 10362 | 11261 | code, added | `script_update` | — | F-LB | `4c7f90b` |
| H067 | after 10377 | 11277 | code, added | `script_update` | — | F-LB | `4c7f90b` |
| H068 | after 10512 | 11413–11414 | code, added | `script_defaults` | — | F-LB | `4c7f90b` |
| H069 | after 10859 | 11762 | code, added | `script_unload` | — | F-LB | `4c7f90b` |

### A.2 Test index

Harness tests (`tests/csct/tests/`) and in-OBS checks (`tests/obs-auto/`) per entry.

| Entry | Tests | Harness tests | In-OBS checks |
|-|-|-|-|
| F-LB | 65 | `bridge_6_2`: helper: none found without the plugin; no bridge call, nothing logged above info<br>`bridge_6_2`: helper: next to the script<br>`bridge_6_2`: helper: the copy installed with the plugin<br>`bridge_6_2`: helper: the file next to the script wins over the plugin's copy<br>`bridge_6_2`: helper: a broken file next to the script falls back to the plugin's copy, without an error<br>`bridge_6_2`: helper: plugin installed but no helper anywhere (default data path has none)<br>`bridge_6_2`: status: no plugin (helper present): 'not installed' at info level, nothing above it<br>`bridge_6_2`: status: plugin too old: says to update the plugin<br>`bridge_6_2`: status: connected, with the plugin version, owner and helper source<br>`bridge_6_2`: start-up: registers only once loading has finished, with no 'reset' event, and publishes the full state<br>`bridge_6_2`: keystrokes in the panel don't register again, and a cosmetic edit emits nothing<br>`bridge_6_2`: a timing change resets the timer and emits 'reset'<br>`bridge_6_2`: the feature turned off unregisters; the timer keeps running<br>`bridge_6_2`: an Instance ID gives the owner stopwatch.<id> (the old owner is unregistered)<br>`bridge_6_2`: contract: reset, add 75, subtract 100, toggle, pause, reset<br>`bridge_6_2`: contract: the plugin rejects add with a non-integer (test_stopwatch_validates_args)<br>`bridge_6_2`: commands: add / subtract without seconds do nothing; start and pause are idempotent<br>`bridge_6_2`: commands: Timer Manipulation set to Disabled blocks add / subtract, silently<br>`bridge_6_2`: Disable Script: a running timer stops without a 'paused' event; status disabled; commands ignored<br>`bridge_6_2`: publishing while running: about 5 updates a second, only the time keys, never late on a whole second<br>`bridge_6_2`: publishing: with a whole-second format, one update a second (only when something visible changes)<br>`bridge_6_2`: publishing: a forced update sends only the keys that changed<br>`bridge_6_2`: events: start, pause, start, reset while running gives started, paused, started, reset<br>`bridge_6_2`: events: a natural Count Down expiry gives 'expired' only, with expire_event<br>`bridge_6_2`: events: an expiry with a Scene List gives expired, reset, started in that order (C14)<br>`bridge_6_2`: expired: never in Count Up, not after a Reset, yes at 0 in Count Down, not when paused mid-run<br>`bridge_6_2`: expired (decision 2026-10-15): a 0 s Count Down after Reset is 'stopped', not 'expired'<br>`bridge_6_2`: expired (decisions 1 and 3): Reset of a running 0 s Count Down gives expired, reset and ends 'stopped'<br>`bridge_6_2`: expired (decision 2026-10-15): a Count Down to a date already passed is 'stopped' after Reset<br>`bridge_6_2`: text (decision 2026-10-15): after a TIMER END TEXT expiry, text is the end text; time and display the clock<br>`bridge_6_2`: status: no Timer Source gives no_source and the hint<br>`bridge_6_2`: status_text: mode and status<br>`bridge_6_2`: hms: floored whole seconds, hours past 24, a sign for negative time; elapsed truncates toward zero<br>`bridge_6_2`: script_unload: unregisters first and stops the helper's heartbeat<br>`bridge_6_2`: publishing: a lasting set_state failure keeps the throttle (about 5 attempts a second, not every frame)<br>`bridge_6_2`: overtime: status 'overtime' is published as soon as overtime starts<br>`bridge_6_2`: marker pause (F9): the 'paused' event has reason 'marker'<br>`bridge_6_2`: status: a plugin error is not called 'too old'<br>`bridge_6_2_p3`: registration: 17 commands, each with a label and a description; a valid dock (nothing skipped)<br>`bridge_6_2_p3`: layout: Split Source, Count Down, Direction toggle and Timer Manipulation Disabled each re-register<br>`bridge_6_2_p3`: state: every key is published after registration, with the documented values<br>`bridge_6_2_p3`: state: preset captions in minutes and hours, with the uses left; a preset command counts a use<br>`bridge_6_2_p3`: state: next_marker_a / b follow the marker list; last_split and split_count follow splits<br>`bridge_6_2_p3`: state: a marker that fires without a pause publishes the next marker time at once (REV12)<br>`bridge_6_2_p3`: publishing: after a failed update, a key that became unset is cleared by the next publish (REV12)<br>`bridge_6_2_p3`: commands: split only while running; 'split' event with index, mark and running-time interval<br>`bridge_6_2_p3`: commands: a split in overtime has no '+', as the split text shows it (decision 2026-10-15; B-081)<br>`bridge_6_2_p3`: commands: set applies 'Manually set timer' in Count Up only<br>`bridge_6_2_p3`: commands: set_time sets the clock but not the start value; negative or missing seconds do nothing<br>`bridge_6_2_p3`: commands: toggle_mili hides and shows the hundredths (the time key follows)<br>`bridge_6_2_p3`: commands: toggle_direction only with 'Enable Timer Direction Toggle'<br>`bridge_6_2_p3`: commands: presets do nothing with Timer Manipulation Disabled or the feature off, and log nothing<br>`bridge_6_2_p3`: commands: all are ignored while Disable Script is on<br>`bridge_6_2_p3`: events: 'marker' for Marker A and B, with the marker time<br>`bridge_6_2_p3`: exactness: start and pause; reset while running and while stopped<br>`bridge_6_2_p3`: exactness: a natural expiry with Select, TIMER END TEXT or a scene gives only 'expired'<br>`bridge_6_2_p3`: exactness: overtime gives 'overtime', not 'expired'<br>`bridge_6_2_p3`: exactness: Source List and Auto List expiries give expired, reset, started<br>`bridge_6_2_p3`: exactness: a split gives only 'split'; a marker pause gives marker, then paused (reason marker)<br>`bridge_6_2_p3`: two instances: Instance IDs '' and '2' give two independent sections<br>`bridge_6_2_p3`: two instances: both with an empty Instance ID make the plugin report a replaced owner (a user error)<br>`bridge_6_2_p3`: panel: the feature checkbox sits in Disable Features before the note group types<br>`bridge_6_2_p3`: panel: the Lua Bridge group shows the status, Instance ID and help; it hides when the feature is off<br>`bridge_6_2_p3`: panel: the Lua Bridge group is hidden in the Basic layout, and the bridge keeps working (decision 2026-10-15)<br>`bridge_6_2_p3`: panel: without the plugin the status says 'not installed' | `bridge_checks.py` PE1, CT1, BC1–BC7, EX1, NP1; `soak.py --bridge` (SOAK-3) |
| B-079 | 3 | `regress_6_2`: B-079 a burst of pauses saves the collection once, then once more at the end of the 2 s window<br>`regress_6_2`: B-079 guard: pauses 3 s apart each save at once, with no deferred save<br>`regress_6_2`: B-079 a timing change while a save is deferred saves it at once and leaves no timer | `fix62_checks.py` G2, G3 |
| B-080 | 2 | `regress_6_2`: B-080 Count Down to a date: Reset after 1 h shows the real remaining time, not the time at load<br>`regress_6_2`: B-080 Count Down to a date: after a timing change while running, a later Reset shows the real remaining time (REV11) | `fix62_checks.py` G6 |
| B-084 | 4 | `regress_6_2`: B-084 Count Up turned down stops at 0 as a pause: 'paused' (reason zero), no 'expired'; published expired false<br>`regress_6_2`: B-084 Count Up turned down: the stop at 0 runs no expiry actions (note stays, colour reset waits, not expired)<br>`regress_6_2`: B-084 guard: Count Down still expires at 0 (Select: 'expired', expired true)<br>`regress_6_2`: B-084 (REV14) the stop at 0 pauses the playing marker audio, as Pause does | `fix62b_checks.py` H2 |
| B-085 | 6 | `regress_6_2`: B-085 counting back out of overtime through 0 leaves overtime: no '+', no '-', the normal colour, counting up<br>`regress_6_2`: B-085 leaving overtime with Marker A above the time: Marker A's colour (it would show, counting down normally)<br>`regress_6_2`: B-085 leaving overtime: Marker A's colour reset time has run out since the marker, so the normal colour<br>`regress_6_2`: B-085 guard: toggling twice in overtime stays in overtime, counting up with '+'<br>`regress_6_2`: B-085 (REV14) leaving overtime inside Marker A's colour reset time: the reset runs for the time it has left<br>`regress_6_2`: B-085 (REV14) a marker above the start time never fires counting down, so it doesn't colour the timer | `fix62b_checks.py` H4 |
| NF-VERSION | 0 | — | — |
| NF-HEADER | 0 | — | — |

Tests in `regress_6_2.lua` for entries that are not part of this diff (fixed in the final 6.1 as well): B-078: 9; B-083: 3; B-086: 6; B-087: 2.
