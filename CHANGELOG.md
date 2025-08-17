# Changelog

## [2.4.0] - 2025-08-17

- Unified config actions so both "Apply Layout" and "Send to Pebble (Close)" send identical payloads via `sendToPebble()`
- `sendToPebble()` now prefers flexible layout from `oura_flexible_layout` with legacy fallback; includes `layout_rows`, `row1_*`, `row2_*`
- Added full wiring for time display settings:
  - "Show Seconds" updates tick rate on the watchface
  - "Compact Time" trims leading zero in 12h mode
  - Settings persisted in localStorage and included in payload
- Fixed debug toggle to control visibility and persist state
- Ensured `show_loading`, `theme_mode`, `date_format`, `refresh_frequency`, and custom color data are included in sends
- Fixed a syntax error in the deployable config file (`reconnect()`)
- Deployed config via `deploy-safe.sh` to Netlify with no-cache headers for `pebble-static-config.html`

## [2.3.0] - 2025-08-xx
- Previous release with OAuth fixes, color picker integration, and stability improvements.
