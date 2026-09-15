# Raspberry Pi Deployment

Owner: Agent 7 (Deployment). See `Plan.md` §13-14 for requirements
(`musicbox.service`, Wi-Fi AP at `192.168.50.1`, offline operation, mDNS at
`musicbox.local`).

Status: deployment tooling complete (systemd unit, Wi-Fi AP config for the
legacy dhcpcd stack, current NetworkManager/Bookworm stack, and Ubuntu
Server's netplan/networkd stack, dnsmasq DHCP config, Avahi mDNS service
definition, an idempotent installer, and a step-by-step manual walkthrough).
**Not validated on real Raspberry Pi hardware** — no device was available in
the environment this was produced in; see "Known limitations" below and
`deployment/README.md` §9.

Also blocked on: Agent 4's REST API layer not being implemented yet — see
"Integration dependencies" below. `run` itself works (starts the real event
loop) but only serves a fixed temporary demo response, not real endpoints;
the deployment plumbing targets the CLI/config contract Agent 4 is building
toward, but hasn't been exercised against a working API.

For the actual step-by-step instructions (build, install, verify AP,
verify mDNS, check service health), see **`deployment/README.md`** — this
document is the summary/contract; that one is the runbook.

## What's here

```text
deployment/
├── config/musicbox.toml           deployment-target default server config
├── systemd/musicbox.service       systemd unit (hardened, runs as `musicbox` user)
├── networking/
│   ├── hostapd.conf                Wi-Fi AP (SSID MusicBox, WPA2-PSK) — dhcpcd and netplan paths
│   ├── dnsmasq.conf                DHCP for AP clients (192.168.50.10+)
│   ├── dhcpcd.conf.append          static 192.168.50.1 on wlan0 — Raspberry Pi OS Bullseye and earlier
│   ├── nm-musicbox-ap.nmconnection static AP + IP via NetworkManager — Raspberry Pi OS Bookworm+ (default path there)
│   ├── netplan-musicbox-ap.yaml    static 192.168.50.1 on wlan0 via netplan — Ubuntu Server on the Pi
│   ├── musicbox-unmanaged.conf     NetworkManager drop-in, only for the "keep hostapd on Bookworm" alternative
│   ├── musicbox-ap-address.service companion static-IP unit for that alternative
│   └── musicbox.service            Avahi service definition -> musicbox.local (note: same filename as the systemd unit above, different tool/directory — see deployment/README.md §0)
├── install.sh                      idempotent installer, --with-ap opt-in
└── README.md                       step-by-step walkthrough + verification steps
```

## Design decisions worth knowing about

* **The Wi-Fi AP is not run by `musicbox-server` or its systemd unit.**
  hostapd/dnsmasq/NetworkManager each run under their own systemd units,
  entirely decoupled from `musicbox.service`. This is why `musicbox.service`
  can be hardened aggressively (`ProtectSystem=strict`, `NoNewPrivileges`,
  an empty `CapabilityBoundingSet`, `RestrictAddressFamilies=AF_INET
  AF_INET6 AF_UNIX`, no raw sockets) without any tradeoff against AP
  functionality — the server only ever needs a plain unprivileged TCP
  listen socket and read/write access to its own config/data directories.
* **Three supported paths for the AP layer**, auto-detected by `install.sh`
  (`nmcli`+NetworkManager, then `dhcpcd`, then `netplan`, in that order):
  classic `hostapd` + `dnsmasq` + a static-IP stanza in `/etc/dhcpcd.conf`
  (Raspberry Pi OS "Bullseye" and earlier); a native NetworkManager AP
  connection profile with a manual static IP (Raspberry Pi OS "Bookworm"+,
  where `dhcpcd` isn't installed and NetworkManager owns `wlan0` by
  default); or `hostapd` + `dnsmasq` + a static-IP netplan config for
  `wlan0` (**Ubuntu Server on the Pi**, whose default stack is netplan +
  systemd-networkd — neither NetworkManager nor dhcpcd). All three hand DHCP
  duty to the same `dnsmasq.conf`. A fourth option (keep hostapd even on
  Bookworm, by marking `wlan0` "unmanaged" in NetworkManager) is provided
  for anyone who'd rather not depend on NetworkManager's AP support; see
  `deployment/README.md`.
* **2.4 GHz / channel 7 by default**, not 5 GHz, because the Raspberry Pi
  Zero 2 W's onboard radio is 2.4 GHz-only; Pi 4/5 deployments that don't
  need Zero 2 W compatibility can switch `hostapd.conf`/the NM profile to
  5 GHz for better throughput (commented inline in `hostapd.conf`).
* **`avahi-daemon`'s `host-name=` is force-set to `musicbox`** rather than
  relying on the device's actual hostname matching, so `musicbox.local`
  resolves regardless of what the Pi is actually called.
* Per the task spec, `install.sh` only installs/enables Avahi under
  `--with-ap`. If you join an existing LAN instead (no AP mode) and still
  want `musicbox.local` there — needed for the "at home" sync scenario in
  Plan.md §33 — install avahi manually; three commands, given in
  `deployment/README.md` §7.

## mDNS client fallback behavior (for Agent 5 / iOS and Agent 6 / sync-client)

Per Plan.md §14:

```text
try mDNS (musicbox.local)
       │
       ▼ (fails or times out — allow a couple of quick retries, mDNS
       │  convergence after associating with a new Wi-Fi network can take
       │  a moment)
fall back to a configured/last-known-good IP
       (e.g. 192.168.50.1 when in AP mode, or a previously resolved address)
```

Clients should not treat a single failed lookup as permanent — cache the
last successfully resolved address and prefer it as the fallback target,
refreshing opportunistically rather than only on failure. See
`deployment/README.md` §7 for the full writeup this section summarizes.

## Public interfaces other agents should know about

* **Config file**: TOML at `/etc/musicbox/musicbox.toml` once deployed
  (dev default remains `config/musicbox.toml` per `docs/development.md`).
  Schema is whatever `server/include/musicbox/config/Config.hpp` defines —
  deployment does not add fields, only ships a populated instance of it
  with deployment-appropriate paths (`/var/lib/musicbox/musicbox.db`,
  `/media/musicbox/music`).
* **CLI**: `musicbox-server run --config <path>` is what the systemd unit
  invokes. Deployment assumes `--config` is (already documented as, per
  `docs/development.md`) a supported flag; no other flags are passed by the
  unit.
* **mDNS name**: `musicbox.local`, `[network].mdns_name = "musicbox"` in the
  config, matching the Avahi `host-name` override installed by
  `install.sh --with-ap`. If Agent 3/4's mDNS-advertising code (if any is
  added inside `musicbox-server` itself, e.g. via an embedded mDNS
  responder rather than relying on the system `avahi-daemon`) picks a
  different name, update both this file and `deployment/networking/
  musicbox.service`'s advertised name to match — they are not
  automatically kept in sync.
* **Wi-Fi AP addressing**: gateway/DNS `192.168.50.1`, DHCP pool
  `192.168.50.10`-`192.168.50.200`/24. Nothing in `musicbox-server` needs to
  know these — they're purely a network-layer concern — but any
  hardcoded-IP fallback in client code (§ above) should default to
  `192.168.50.1` to match.

## Integration dependencies

For this deployment tooling to do anything useful end-to-end, it needs:

1. `musicbox-server run --config <path>` to serve the real `/api/v1/*` API
   (the event loop itself already runs — see `server/src/main.cpp`'s
   "TEMPORARY Milestone-1 demo wiring" — Agent 4's API layer is what's
   missing).
2. The config loader (`musicbox::config::loadConfigFile`, already declared
   in `server/include/musicbox/config/Config.hpp`) to be implemented and
   wired into `run`, since the systemd unit's only contract with the binary
   is `--config /etc/musicbox/musicbox.toml`.
3. `GET /api/v1/status` (Plan.md §10) to exist, so `deployment/README.md`'s
   verification `curl` command and any future health-check tooling
   (`musicbox-server status`, `doctor`) have something to hit.
4. No new requirement on Agent 3's library scanner: it should work
   unmodified against `/media/musicbox/music` (or whatever
   `--library-path` points at), same as any other directory.

## Known limitations

* Not tested on real Raspberry Pi hardware (no device available in this
  environment) — see `deployment/README.md` §9 for the full list of things
  that specifically need a real device to validate (regulatory
  `country_code`, exact `ht_capab` support per Wi-Fi chip revision, whether
  `ProtectSystem=strict` is too tight once the server does more).
* `install.sh` was exercised with `bash -n` (syntax) and its
  marker-block-replacement helper was unit-tested standalone in a scratch
  directory; it was not run end-to-end as root against a real filesystem
  (no disposable Linux box in this environment either — this was authored
  on macOS).
* `hostapd`, `dnsmasq`, `nmcli`, `netplan`, and `avahi-daemon` were not
  installed in this environment, so their config files were reviewed by hand
  against upstream man-page documentation rather than validated with e.g.
  `hostapd -t hostapd.conf` or `netplan generate`. Recommend running those
  (and `dnsmasq --test --conf-file=...`) on an actual Debian/Raspberry Pi OS
  or Ubuntu box, or in a Docker container with the packages installed,
  before first real deployment.
* The netplan path is newer and less battle-tested than the other two — it
  hasn't been checked against a real cloud-init-provisioned Ubuntu Server
  image, which may ship its own `wlan0` netplan config that conflicts with
  ours (see the comment in `netplan-musicbox-ap.yaml` and
  `deployment/README.md` §9).
* Single-radio dual-mode (home Wi-Fi client for sync, then AP mode for the
  car) is a manual toggle, not automated — see `deployment/README.md` §9.

See `deployment/README.md` for the full walkthrough, verification steps,
and additional detail behind every decision summarized above.
