# Deploying MusicBox to a Raspberry Pi

Owner: Agent 7 (Raspberry Pi / Deployment). Covers Plan.md §13 (systemd +
Wi-Fi AP) and §14 (mDNS). Cross-referenced from `docs/deployment.md`.

**This has not been tested on real Raspberry Pi hardware.** No Pi was
available in the environment this was written in. Everything below was
written and reviewed against upstream documentation (`man systemd.service`,
`man hostapd.conf`, `man dnsmasq`, `man NetworkManager.conf`,
`man avahi.service`) and basic syntax tooling (`bash -n` on `install.sh`;
`hostapd`/`dnsmasq`/`avahi-daemon`/`nmcli` themselves were not installed in
this environment, so their config files were not run through `-t`/`--test`
style validation). Treat first boot on a real device as a test, and read
through each file's comments before you `install.sh --with-ap` something you
depend on.

Targets: Raspberry Pi Zero 2 W, Pi 4, Pi 5, running either Raspberry Pi OS
(Debian-based, 32- or 64-bit) or Ubuntu Server (22.04+/24.04+, arm64) —
`install.sh --with-ap` auto-detects which of NetworkManager, dhcpcd, or
netplan is your active network stack and installs the matching AP config
(see §0's table). Differences between boards are called out inline (mainly:
the Zero 2 W's Wi-Fi radio is 2.4 GHz-only).

---

## 0. What gets installed where

| What | Repo path | Installed to |
|---|---|---|
| Server binary | (you build this) | `/opt/musicbox/bin/musicbox-server` |
| Server config | `deployment/config/musicbox.toml` | `/etc/musicbox/musicbox.toml` |
| systemd unit | `deployment/systemd/musicbox.service` | `/etc/systemd/system/musicbox.service` |
| SQLite DB / state | — | `/var/lib/musicbox/` |
| Music library | — (you provide the files) | `/media/musicbox/music/` (default; `--library-path` to change) |
| hostapd config (dhcpcd- and netplan-based paths) | `deployment/networking/hostapd.conf` | `/etc/hostapd/hostapd.conf` |
| dnsmasq config | `deployment/networking/dnsmasq.conf` | `/etc/dnsmasq.d/musicbox.conf` |
| Static IP, dhcpcd (Raspberry Pi OS Bullseye and earlier) | `deployment/networking/dhcpcd.conf.append` | appended into `/etc/dhcpcd.conf` |
| Static IP / AP, NetworkManager (Raspberry Pi OS Bookworm+) | `deployment/networking/nm-musicbox-ap.nmconnection` | `/etc/NetworkManager/system-connections/musicbox-ap.nmconnection` |
| Static IP, netplan (**Ubuntu Server on the Pi**) | `deployment/networking/netplan-musicbox-ap.yaml` | `/etc/netplan/90-musicbox-ap.yaml` |
| mDNS service record | `deployment/networking/musicbox.service` | `/etc/avahi/services/musicbox.service` |

All of this is applied by `deployment/install.sh` — see §2 below. You
generally should not need to copy files by hand, but the table above is
there so you can find and inspect anything the script does.

**A note on the two `musicbox.service` files.** `deployment/systemd/musicbox.service`
and `deployment/networking/musicbox.service` are unrelated files that
happen to share a name because their respective tools (systemd, Avahi) both
use `.service` as the extension for their own, differently-shaped config
format. They install to different directories and are never confused by the
tools that read them — but if you're grepping the repo, know there are two.

---

## 1. Build the server first

Install the build dependencies, then build from the repo root:

```bash
sudo apt install cmake g++ libsqlite3-dev libtag1-dev libssl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This should produce `build/server/musicbox-server`. `install.sh` looks for
it there by default; pass `--binary /path/to/musicbox-server` if you built
elsewhere (e.g. cross-compiling on a dev machine and copying the binary over
to the Pi separately — this repo does not include cross-compilation tooling,
see §9 "Recommended next tasks").

As of this writing, `musicbox-server run` starts the real epoll/kqueue event
loop and serves the real `/api/v1/*` API (docs/api.md) against your SQLite
library — `musicbox-server scan --config <path>` indexes your music first.
Both have been verified end-to-end on a development machine (build, scan a
real file, run, curl status/artists/albums/tracks/streaming+Range/playlists),
but **not yet on real Raspberry Pi hardware** — see §9. The deployment
tooling in this directory (systemd unit, AP config, install script) targets
exactly this CLI/config contract.

## 2. Copy this repo (or just `deployment/` + the binary) to the Pi

However you like — `scp`, `rsync`, or clone the repo directly on the Pi. You
need `deployment/` and a built `musicbox-server` binary.

## 3. Run the installer

Joining an existing home Wi-Fi/Ethernet network (no AP mode):

```bash
sudo ./deployment/install.sh
```

Running as a standalone offline Wi-Fi AP (the in-car scenario, Plan.md §33):

```bash
sudo MUSICBOX_WIFI_PASSPHRASE='pick-a-real-passphrase' \
    ./deployment/install.sh --with-ap
```

**Change the Wi-Fi passphrase.** The shipped `hostapd.conf` and
`nm-musicbox-ap.nmconnection` ship with the literal placeholder
`ChangeMe-MusicBox-2024` so that an unmodified checkout is obviously
insecure rather than silently weak. Either set `MUSICBOX_WIFI_PASSPHRASE`
(shown above — `install.sh` substitutes it into whichever file it installs)
or edit the installed file directly afterwards and re-run with
`--keep-existing-ap-config` so a future re-run doesn't overwrite your edit.

Run `./deployment/install.sh --help` for the full option list (custom
install prefix, library path(s), skipping `systemctl enable` for offline
image-building, etc).

**Re-running `install.sh` is safe.** It creates the `musicbox` user/group
only if missing, only writes `/etc/musicbox/musicbox.toml` if it doesn't
already exist (pass `--force` to regenerate it), and always refreshes the
systemd unit + AP configs from this repo (pass `--keep-existing-ap-config`
to preserve hand-edited AP config files instead).

## 4. Add your music

Copy files into the library path (default `/media/musicbox/music`) however
you like for now — over `scp`/`rsync`/a USB stick, or (once Agent 6 lands)
`musicbox-sync ~/Music musicbox.local`. The directory is owned by the
`musicbox` user; if you're copying in as a different user, either `chmod
g+w` the tree and add yourself to the `musicbox` group, or copy in as root
and `chown -R musicbox:musicbox` afterwards.

### External SSD mount

If your library lives on an external SSD rather than the SD card, mount it
before starting the service (e.g. an `/etc/fstab` entry mounting it at
`/media/musicbox/music` directly, or at your own mountpoint with
`--library-path` pointed at it). Find the exact systemd mount-unit name
with:

```bash
systemd-escape --path --suffix=mount /your/mount/point
```

and use that in the commented-out `After=`/`Requires=` lines at the top of
`/etc/systemd/system/musicbox.service` (via `sudo systemctl edit musicbox`,
not by hand-editing the installed unit, so a future `install.sh` re-run
doesn't discard your change) if you want the server to wait for the mount
before starting.

## 5. Start it

```bash
sudo systemctl start musicbox
sudo systemctl status musicbox
```

`install.sh` deliberately does **not** `systemctl start` the service for
you (only `enable`s it) — the real API now exists (§1), but nothing has
scanned a library on the target device yet at install time, and
auto-starting before there's anything to serve isn't very useful. Run
`musicbox-server scan --config /etc/musicbox/musicbox.toml` once, then start
the service yourself (below). Auto-scan-then-start-on-install is a
reasonable future `install.sh` enhancement (see `docs/deployment.md`), not
implemented yet.

`systemctl status musicbox` should show `active (running)` with
`User=musicbox` and no restart-loop counter climbing. Follow logs with:

```bash
journalctl -u musicbox -f
```

## 6. Verify the Wi-Fi AP came up (`--with-ap` only)

On the Pi:

```bash
# Which path got used?
nmcli connection show musicbox-ap 2>/dev/null && echo "NetworkManager AP path"
systemctl is-active hostapd 2>/dev/null && echo "hostapd path (dhcpcd or netplan)"
ls /etc/netplan/90-musicbox-ap.yaml 2>/dev/null && echo "  ...specifically the netplan path (Ubuntu)"

ip addr show wlan0     # should show 192.168.50.1/24
```

From another device (phone/laptop): look for the `MusicBox` SSID in its
Wi-Fi picker, connect with the passphrase you set, and confirm it's handed
an address in `192.168.50.10`-`192.168.50.200` (phone Wi-Fi settings, or
`ip addr`/`ipconfig` on a laptop). `192.168.50.1` should be reachable:

```bash
ping 192.168.50.1
curl http://192.168.50.1:8080/api/v1/status   # once the API is implemented
```

## 7. Verify `musicbox.local` resolves (`--with-ap` only, or if you installed avahi manually — see below)

From the same client device, once connected to the MusicBox network (or any
network the Pi is also on):

```bash
ping musicbox.local
curl http://musicbox.local:8080/api/v1/status
```

If that fails but `ping 192.168.50.1` works, mDNS/Avahi is the problem, not
the network — check `systemctl status avahi-daemon` on the Pi and
`journalctl -u avahi-daemon`. Some OSes/networks need the client to
explicitly support mDNS (Bonjour) — iOS and macOS do natively; some Android
versions and most plain Linux need `avahi-daemon`/`nss-mdns` on the client
side too, which is out of scope for the Pi-side config here.

**Known gap, documented rather than silently shipped:** per the task spec,
`install.sh` only installs/enables avahi under `--with-ap`. If you're
joining an existing home LAN (no `--with-ap`) and still want
`musicbox.local` to resolve there (needed for `musicbox-sync ~/Music
musicbox.local` per Plan.md §33's "at home" scenario), install avahi
yourself:

```bash
sudo apt install avahi-daemon
sudo cp deployment/networking/musicbox.service /etc/avahi/services/musicbox.service
sudo sed -i '/^\[server\]/a host-name=musicbox' /etc/avahi/avahi-daemon.conf
sudo systemctl enable --now avahi-daemon
```

### Client-side fallback behavior (for the iOS/sync-client agents)

Per Plan.md §14, any MusicBox client should:

```text
try resolving musicbox.local (mDNS/Bonjour)
       │
       ▼ (timeout or resolution failure)
fall back to a user-configured/last-known IP address
(e.g. the AP's fixed 192.168.50.1, or whatever the user typed in Settings)
```

mDNS resolution can be slow or fail on the very first attempt after a phone
associates with a new network (multicast can take a moment to converge, and
some phones aggressively power-save their Wi-Fi radio for multicast
traffic) — clients should not treat one failed `musicbox.local` lookup as
fatal, and should retry a couple of times with a short timeout (a second or
two total) before falling back, not fall back instantly on the first
attempt. Once a client has successfully connected once, it's reasonable to
cache the resolved IP and use it as the fallback for next time, refreshing
it opportunistically.

This behavior belongs in `client-ios/` (likely wrapping `NWBrowser`/
`NetServiceBrowser` for the mDNS half) and `sync-client/` — this repo's
`deployment/` only documents the contract those agents should implement
against; it does not implement the client side.

## 8. Undoing / re-running

* Re-run `install.sh` any time (see "safe to re-run" in §3).
* To fully remove the AP setup: `systemctl disable --now hostapd dnsmasq`
  (dhcpcd or netplan path — on the netplan path also `rm
  /etc/netplan/90-musicbox-ap.yaml && netplan apply`) or `nmcli connection
  delete musicbox-ap` (NetworkManager path), then remove the corresponding
  files listed in §0's table and `systemctl daemon-reload`.
* To stop musicbox-server entirely: `systemctl disable --now musicbox`.
  Nothing in this repo deletes `/var/lib/musicbox` (your database) or your
  music library automatically — remove those by hand if you actually want
  them gone.

## 9. Known limitations / open items

* **No real hardware testing.** Everything here is manual-review-only
  against upstream docs. In particular: exact regulatory `country_code` in
  `hostapd.conf`, whether `ht_capab` flags are right for every Pi Wi-Fi
  chip revision, and whether `ProtectSystem=strict` in the systemd unit is
  too aggressive for some future feature (e.g. if `musicbox-server`
  eventually wants to write anywhere other than `/var/lib/musicbox`) are all
  things a real device will surface that a read-through cannot.
* `run` and `scan` are real and verified on a development machine (§1);
  `status` and `doctor` (CLI subcommands, distinct from the HTTP
  `/api/v1/status` route) are not implemented yet.
* **netplan path untested against a real image.** The dhcpcd and
  NetworkManager paths at least mirror upstream Raspberry Pi OS's own
  defaults; the netplan path (`install_ap_netplan()`,
  `deployment/networking/netplan-musicbox-ap.yaml`) was written for a
  cloud-image Ubuntu Server install and reviewed against `netplan(5)`/
  `systemd.network(5)`, but not run through `netplan generate`/`netplan try`
  (not installed in this environment) or booted on a real device. In
  particular: if the base Ubuntu image's own cloud-init-generated netplan
  config already tries to manage `wlan0` as a Wi-Fi client, it will conflict
  with this file (see the comment at the top of
  `netplan-musicbox-ap.yaml`) — inspect `/etc/netplan/*.yaml` first and
  remove/disable any pre-existing `wlan0` client config before
  `install.sh --with-ap`.
* Dual NIC/dual-mode operation ("join home Wi-Fi to sync, then switch to AP
  mode for the car") is not automated. If your Pi has only one Wi-Fi radio,
  you have two real options: (a) always run in AP mode and do library sync
  over Ethernet instead, or (b) manually toggle between `--with-ap`'s AP
  config and a normal Wi-Fi client connection (`nmcli device wifi connect
  ...` / `wpa_supplicant`) when you're home vs. in the car. This repo does
  not pick one for you; see `docs/deployment.md` for further discussion.
* Cross-compilation for the Pi (building on a faster dev machine instead of
  natively on-device) is not set up — `install.sh` assumes you hand it an
  already-built ARM binary, however you produced it.
