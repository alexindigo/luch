# Luch (Луч)

Link router for Linux/Wayland — the system handler for `http`/`https`
URLs. When any app asks the system to open a link, Luch pops up a fast,
centered picker above everything (wlr-layer-shell overlay) listing your
installed browsers and profiles; pick one, the URL opens there, the
popup is gone.

Named after the Soviet Luch Satellite Data Relay Network: it intercepts
a signal (a link click) and relays it to the right destination.

![Luch picker](assets/screenshot.png)

## Status

v1 is the picker popup. There is no rules engine yet — the config
format reserves `rules[]` so v2 (domain/regex routing, tracker
stripping, remember-per-site) bolts on without migration.

Wayland-only (niri, mangowc, KDE, any compositor with
`zwlr_layer_shell_v1`).

## Build

Dependencies: Qt6 (Core, Gui, Quick/Declarative, Widgets, WaylandClient),
`layer-shell-qt` (Arch: `extra/layer-shell-qt`), `wayland-protocols`,
`xdgiconqml` (the `XdgIcon` QML module that resolves browser icons),
`libseccomp` (the offline plugin worker's network sandbox),
`wayland-scanner`, and a C++20 compiler + CMake ≥ 3.21.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

`wl-clipboard` is an optional runtime nicety so Ctrl+C survives the
popup exiting. If the `XdgIcon` module is somehow missing at runtime the
picker still works, falling back to letter monograms.

## Wiring it up

```sh
luch --set-default
```

wraps `xdg-mime default luch.desktop x-scheme-handler/http
x-scheme-handler/https`. Or do it by hand:

```sh
xdg-mime default luch.desktop x-scheme-handler/http
xdg-mime default luch.desktop x-scheme-handler/https
```

Luch is never registered at install time — only when you ask.

## Usage

```sh
luch <url|html-file>   # show the picker for a URL or local HTML file
luch --inspect <url>   # print plugin roster + payload as JSON (no GUI)
luch --daemon          # resident settings daemon (owns settings.json + tray)
luch --settings        # open the settings UI directly (no daemon needed)
luch --set-default
luch --version
```

Targets are http(s) URLs and local HTML files (`text/html`,
`application/xhtml+xml`) — pass a path or a `file://` URI. Anything
else is refused with exit 2 and a message naming the detected type.

Keyboard: `1`–`9` launch directly, arrow keys + `Enter` or `Space` to
choose, `Esc` dismisses, `Ctrl+C` copies the target. Click works too —
including the copy glyph next to the target line at the bottom, which
always shows what is about to be opened (host and filename are never
truncated; long paths collapse to ` … ` and wrap).

Exit codes: `0` a browser was launched or the target was copied, `1`
dismissed without launching, `2` the target was not routable.

## Config

`~/.config/luch/config.json` (honors `XDG_CONFIG_HOME`), created with
defaults on first run:

```json
{
  "version": 1,
  "ui": { "accent": "cyan", "theme": "light" },
  "browsers": [
    {
      "id": "firefox-work",
      "name": "Firefox (Work)",
      "exec": "firefox -P work %u",
      "icon": "firefox",
      "source": "manual",
      "hidden": false
    }
  ],
  "rules": [],
  "focus": { "followOnOpen": true }
}
```

- The picker lists every installed `.desktop` entry that handles
  `x-scheme-handler/http` (minus `NoDisplay`/`Hidden` ones) — or, for
  HTML file targets, entries declaring `text/html`.
- `browsers[]` entries with `"source": "manual"` are added to the list
  (browser profiles, remote browsers, anything with an Exec line —
  `%u` receives the URL or the `file://` form, `%f` the file path).
- An entry with `"hidden": true` keeps the entry in config but removes
  it from the picker — any entry, scanned or manual.
- `rules[]` is parsed but ignored in v1.
- `focus.followOnOpen` (default `true`): after you pick, Luch mints an
  `xdg-activation-v1` token (compositor-agnostic Wayland focus request)
  and attaches it to the launched browser so the receiving window takes
  focus. Set it to `false` to never mint or attach the token.

## Focus-follow expectations

Focus-follow works when the browser can redeem xdg-activation tokens,
i.e. **runs native Wayland** and knows how to use the token:

- Firefox and native-Wayland Chromium redeem it on cold start and on
  remote-open of a running instance (the token rides the remoting
  payload, in-band).
- For Chromium-family browsers on X11-default desktop files, focus
  stays put. Run native Wayland instead — e.g.
  `--ozone-platform=wayland` on the command line, or persistently in
  `~/.config/chromium-flags.conf`:
  ```
  --ozone-platform=wayland
  ```
- The token rides D-Bus `platform_data` on the `Application.Open` /
  `org.gtk.Application.Activate` transport paths for running instances
  that speak those (GLib/Gtk apps), and the Mozilla `OpenURL`
  command-line blob for Firefox.

## Transports

At pick time, for a **running** instance, Luch talks to the browser
through its own remoting channel instead of spawning a new process —
in this order, falling back down the chain on any failure:

1. **D-Bus** — Mozilla remote (`OpenURL(ay)` on
   `org.mozilla.<app>.<profile>`) for Firefox/Thunderbird/LibreWolf;
   `org.freedesktop.Application.Open` for apps implementing it;
   `org.gtk.Application.Activate` with the app's startup context for
   GTK/GApplication browsers (GNOME Web).
2. **Chromium singleton socket** — `<profile>/SingletonSocket` per
   Chromium's own client protocol (cookie-verified, retry loop).
3. **CLI spawn** — always the floor; also the whole story for cold
   starts.

A second `luch` invocation while the picker is open forwards its target
into the same popup as a queued item (D-Bus `app.luch` when a session
bus is available, `QLocalServer` socket otherwise), shown as a carousel
(counter badge, chevrons, indicator dots); `Shift+←/→` moves between
queued targets, `Esc` dismisses the current one, `Shift+Esc` drops the
whole queue.

## Plugins

Luch has a plugin architecture: runtime-loadable `.so` plugins
(`QPluginLoader`) that run as a **sequential chain** over a
system-owned phase ladder — **Unwrap → Clean → Detect** — and build a
per-target **payload**:

```json
{
  "original": {"kind": "url", "url": "…", "raw": "…"},
  "url": "<effective URL — last trace entry that produced one>",
  "detected": [{"plugin": "phishing-check", "verdict": "malicious", "source": "hagezi-tif"}],
  "trace": [
    {"plugin": "debounce", "iteration": 1, "data": {"url": "https://example.com/?utm_medium=social&foo=1", "debouncedFrom": "…"}},
    {"plugin": "brave-clean-url", "iteration": 1, "data": {"url": "https://example.com/?foo=1", "strippedParams": ["utm_medium"]}}
  ]
}
```

- Each stage receives **chain state** `{kind, url (the current working
  URL), raw, original}` and runs to **fixpoint**: its own output is fed
  back until it noops or the `maxHops` cap (default 10) stops
  adversarial ping-pong. Every iteration is a trace entry — the full
  redirect lineage stays inspectable.
- `priorSlices` (earlier slices by id) is a read-only side channel for
  non-URL data; no plugin ever hardcodes whose `"url"` to read.
- The only reserved key inside a slice is `"url"` — present only when
  the plugin changed the working URL. All other keys are plugin-owned;
  the reserved set only grows additively (never redefines a key).
- Manifests declare `"phase"` (`unwrap` | `clean` | `detect` — closed
  set, unknown → invalid) and `"inet"` (capability declaration).
  There is no `"priority"`: position is decided by the phase ladder —
  within Unwrap offline before online, elsewhere by id.

A plugin ships as a pair: `<id>.so` plus a sidecar manifest `<id>.json`
(`{"id", "title", "description", "phase", "inet"}`). Discovery order
(first id wins, so a userland pair replaces a built-in as a unit):

1. `$LUCH_PLUGINS_DIR` (if set)
2. `~/.local/share/luch/plugins/`
3. the installed plugins dir (`<libdir>/luch/plugins`)

### Workers and the network sandbox

Two shared worker threads run every plugin stage off the UI thread:

- **Offline worker** — a `QThread` with a per-thread seccomp filter:
  `socket()` is allowed for `AF_UNIX` (D-Bus, local IPC) and fails for
  `AF_INET`/`AF_INET6` — an offline plugin provably cannot make an
  outside request at the syscall level. All `inet: false` plugins run
  here (object affinity, sequential dispatch).
- **Inet worker** — unfiltered event loop for `inet: true` plugins.
  Online plugins never block: their stages dispatch queued and the
  slice lands later via `patchSlice`; slice presence *is* the stage
  state (a dispatched stage with no slice yet is working — its light
  shimmers). Results arriving after launch/copy/dismiss are dropped.

**Privacy-first**: declared-online (`inet: true`) plugins are disabled
by default — nothing phones home until you opt in per plugin in
settings. User configuration may only *downgrade* (force a declared
online plugin onto the blocked worker); nothing lets a plugin escalate
itself online. This is a *network* sandbox only — no crash isolation
yet.

### Settings

`~/.config/luch/settings.json`, owned by the minimal settings daemon
(`luch --daemon`, tray icon with toggles):

```json
{
  "ui": { "showDissection": false },
  "maxHops": 10,
  "logPayload": false,
  "plugins": { "<id>": { "enabled": true, "inet": false } }
}
```

Pickers are short-lived and read settings at startup. `luch --settings`
opens the settings UI directly. The legacy per-plugin
`~/.config/luch/plugins/<id>.json` files are migrated once
automatically. `config.json` stays purely user-authored.

### Bundled plugins

- **`debounce`** (unwrap, offline) — rule-based redirect extraction
  from the URL itself (Brave's debounce list); emits `url` +
  `debouncedFrom` when unwrapped and a `shortener` flag when the host
  matches a wrapper pattern (the future online fallback's trigger).
- **`brave-clean-url`** (clean, offline) — query filtering (including
  the conditional `mkt_tok`/`h_*` trackers) and the clean-urls ruleset;
  emits `url` + `strippedCount`/`strippedParams` when stripping
  produced a different URL.

Both are driven by Brave's own vendored lists (`assets/lists/`,
MPL-2.0) plus a public-suffix-list snapshot for the debounce failsafes.
Refresh the lists with `tools/update-lists.sh`.

### The picker UI

- **Top lights subpanel** — one cluster per chain plugin, in chain
  order: dim (queued), shimmer (working), green (noop), amber
  (found/changed), red (dangerous); every trace entry pulses the bulb,
  the resting color is the final outcome; declared-online plugins carry
  an "online" marker.
- **Left-edge variant pills** — one radio pill per URL variant the
  chain produced (original always present); the last variant
  auto-selects as pills materialize, and the main panel only ever knows
  the presented URL: footer, launch and copy all follow the selection.
- **Bottom dissection panel** — URL anatomy (scheme / host / domain
  via eTLD+1 / port / path / query / fragment). Hidden by default;
  auto-shows on a red verdict; pinnable via settings. A red verdict
  also shows a "flagged by {source}" warning line. No launch gating —
  the danger is loud, not blocking.

Third-party plugins: build against the installed
`<includedir>/luch/luchaugmenter.h`, then drop the `.so` + manifest pair
into a discovery dir. ABI note: pre-1.0 the interface vtable may still
change; the IID bumps on any breaking change.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE). REUSE-compliant via
[REUSE.toml](REUSE.toml).
