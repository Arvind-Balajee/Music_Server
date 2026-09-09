# 0005: No authentication/authorization in the MVP

## Context

MusicBox is a private device the user carries and connects their own phone to,
either on a home LAN or on its own offline access point. `Plan.md` §31 explicitly
warns against "unnecessary authentication complexity in the MVP."

## Decision

Ship v1 with no login/token/auth layer. Every endpoint is reachable by anyone who
can reach the device's Wi-Fi. Security effort instead goes into input validation:
path traversal prevention, malformed HTTP rejection, range validation, and
resolving all file access through database IDs rather than client-supplied paths
(`docs/architecture.md` §7).

## Alternatives Considered

* **API key / bearer token**: reasonable future addition once the device is used on
  shared/home networks by default, but adds setup friction (provisioning the iOS app
  with a key) disproportionate to the current single-user, single-device,
  own-access-point use case.
* **Full user accounts**: unnecessary complexity explicitly excluded by `Plan.md`.

## Consequences

* Anyone on the MusicBox Wi-Fi network (home LAN or its own AP) can browse and
  stream the library and modify playlists. Acceptable for a personal device on a
  network the user controls.
* This must be revisited before any deployment scenario where the device joins a
  network the user doesn't fully trust (e.g. a shared/public Wi-Fi) — flagged here
  rather than silently assumed away.
