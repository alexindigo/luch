# Changelog

## [0.2.0] — 2026-08-31

### Feature

Luch gains a plugin architecture: runtime-loadable `.so` plugins with
sidecar manifests augment a per-target payload (`{"original": …,
"plugins": {<id>: {…}}}`) that flows from the engine toward the UI. The
first plugin, `urlclean`, unwraps redirect wrappers and strips tracking
parameters with Brave's own data lists (debounce, query-filter,
clean-urls), offering the cleaned destination as `plugins.urlclean.url`.
Launch and copy keep using the original URL — payload-aware UI arrives
in a later round. `luch --inspect <url>` prints the roster and payload
as JSON without touching Wayland/QML. The picker also gains accessibility
annotations (named tiles, list, footer, copy control, queue chrome,
alert on errors) exposed over AT-SPI.

- feat: urlclean plugin — Brave-parity tracker stripping as first .so (`a6d44a4`)
- feat: plugin augmentation framework — .so plugins, payload pipeline, roster (`338bb59`)
- feat: URL-cleaning engine — eTLD matcher + Brave-style three-stage cleaner (`88cc0e6`)
- feat: vendor Brave URL-cleaning lists and PSL snapshot (`c23c98f`)
- feat: accessibility annotations for the picker (`b4e7b6e`)

### Fix

The footer's long-URL handling is corrected: instead of collapsing the
entire middle to a stub while the window ballooned to fit the full text,
the middle now elides only as much as needed — as two fragments around a
spaced accent-colored marker — and the tail matches the middle tone.
Surface opacity bumped 85% → 90% so labels survive busy backgrounds.
Routine transport narration dropped to the debug category so launches
stay silent.

- fix: elide footer middle only as much as needed (`25a73de`)
- fix: quiet routine transport logs to the debug category (`4c3b494`)

## [0.1.0] — 2026-08-28

### Feature

The link router arrives whole. The layer-shell picker popup lists your
installed browsers as icon tiles (digits and arrows to choose); it routes
http(s) URLs and local HTML files, shows exactly what is about to open in a
segmented footer with protected host and tail, and a copy control takes the
target as text instead of opening it. Focus follows the link: an
xdg-activation token rides every open path — the spawned child's environment
on cold start, and the browser's own remoting payload in-band for a running
instance (Mozilla `OpenURL` carries it as `STARTUP_TOKEN` in argv[0],
Chromium's singleton socket as `--xdg-activation-token=`, GTK/GApplication
`Activate` as `desktop-startup-id` in platform-data). A second `luch`
invocation while the picker is open queues its target into the same popup
(carousel badge, chevrons, indicator dots), and the whole session exits 0
when anything was routed or copied, 1 otherwise.

- feat: single-instance queue with carousel navigation (`0414fe0`)
- feat: Chromium singleton-socket transport (`7220398`)
- feat: D-Bus transport for running browser instances (`c796871`)
- feat: focus-follow via xdg-activation token (`ebbba56`)
- feat: universal target footer with copy control (`e5ba679`)
- feat: accept local HTML files as targets (`b0b855a`)
- feat: velja-style horizontal prompt (`686373d`)
- feat: desktop integration, icon, docs (`f7e9449`)
- feat: browser registry, config, and launch flow (`1d1de05`)
- scaffold: Qt6 + LayerShellQt overlay popup shell (`88240dc`)

### Fix

Child processes no longer inherit Luch's terminal (browsers used to print
into the calling tty and hold it open), no longer inherit
`QT_WAYLAND_SHELL_INTEGRATION` (Qt browsers died on launch), and manual
config entries honor `hidden: true` like scanned ones do.

- fix: honor hidden flag on manual config entries (`c31f35f`)
- fix: don't leak QT_WAYLAND_SHELL_INTEGRATION to launched apps (`3c89d59`)
- fix: detach child stdio from the terminal (`27ad570`)
- fix: render browser icons — XdgIcon is a resolver, not a visual (`3d2c61a`)

### Docs

- docs: focus-follow requires native-Wayland browsers (`d1d2da6`)
- docs: use full desktop screenshot in README (`8c443d8`)
- docs: add picker screenshot to README (`54e4257`)
- build: make xdgiconqml a required dependency (`37f44b6`)
