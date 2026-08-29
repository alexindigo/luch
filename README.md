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

Dependencies: Qt6 (Core, Gui, Quick/Declarative, WaylandClient),
`layer-shell-qt` (Arch: `extra/layer-shell-qt`), `wayland-protocols`,
`xdgiconqml` (the `XdgIcon` QML module that resolves browser icons),
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

## License

GPL-3.0-or-later — see [LICENSE](LICENSE). REUSE-compliant via
[REUSE.toml](REUSE.toml).
