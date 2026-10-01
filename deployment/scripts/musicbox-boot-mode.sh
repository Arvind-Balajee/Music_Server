#!/usr/bin/env bash
#
# MusicBox boot-time mode selector.
#
# Run once at boot (via musicbox-boot-mode.service, see deployment/systemd/)
# to decide what this device should be doing based on which Wi-Fi network,
# if any, it's currently on:
#
#   - Connected to the home network (HOME_SSID below, "Mean_girls"): this is
#     assumed to be a shared home network where the server is either run by
#     hand or not wanted at all. This script only mounts the library SSD and
#     otherwise leaves the box alone — no AP, no musicbox.service.
#   - Connected to any *other* SSID, or not associated with any Wi-Fi at
#     all (the in-car / away-from-home case, Plan.md §33): mounts the SSD
#     and brings the box up as its own standalone Wi-Fi AP running the
#     server, same as `install.sh --with-ap` sets up statically.
#
# This targets the NetworkManager network stack (Raspberry Pi OS Bookworm+,
# or Ubuntu Server's default) — see deployment/README.md. It assumes:
#
#   1. `deployment/install.sh --with-ap` has already been run once, so the
#      "musicbox-ap" NetworkManager connection profile and musicbox.service
#      both exist on this device.
#   2. musicbox.service is NOT left `systemctl enable`d — this script starts
#      and stops it directly, so a systemd-level auto-start would fight with
#      it. Run `systemctl disable musicbox` once after installing.
#   3. A NetworkManager profile for "Mean_girls" already exists (e.g. you
#      ran `nmcli device wifi connect Mean_girls password ...` once) with
#      normal autoconnect, so NetworkManager will join it on its own
#      whenever it's in range, before this script even looks at anything.
#   4. The library SSD has an /etc/fstab entry for SSD_MOUNT_POINT below
#      (see deployment/README.md "External SSD mount") so a plain
#      `mount <path>` here is enough to mount it.
#
# Safe to re-run: every step checks current state before acting.

set -euo pipefail

# ---------------------------------------------------------------------------
# Config — edit these to match your setup
# ---------------------------------------------------------------------------

HOME_SSID="Mean_girls"
AP_CONNECTION="musicbox-ap"
WIFI_IFACE="wlan0"
SSD_MOUNT_POINT="/media/musicbox/music"

# How long to give NetworkManager to finish autoconnecting to a known
# network before deciding "not connected" and falling back to AP mode.
WIFI_WAIT_SECONDS=20
WIFI_POLL_INTERVAL=2

# How long to wait for the library SSD to enumerate (USB SSDs can be slow to
# show up right at boot).
SSD_WAIT_SECONDS=20
SSD_POLL_INTERVAL=2

log()  { logger -t musicbox-boot-mode "$*" 2>/dev/null || true; printf '[musicbox-boot-mode] %s\n' "$*"; }
warn() { logger -t musicbox-boot-mode "WARNING: $*" 2>/dev/null || true; printf '[musicbox-boot-mode] WARNING: %s\n' "$*" >&2; }

if [[ $EUID -ne 0 ]]; then
    printf '[musicbox-boot-mode] ERROR: must be run as root\n' >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# SSD
# ---------------------------------------------------------------------------

mount_ssd() {
    if mountpoint -q "$SSD_MOUNT_POINT"; then
        log "SSD already mounted at $SSD_MOUNT_POINT"
        return 0
    fi

    local waited=0
    while (( waited < SSD_WAIT_SECONDS )); do
        if mount "$SSD_MOUNT_POINT" 2>/dev/null; then
            log "mounted SSD at $SSD_MOUNT_POINT"
            return 0
        fi
        sleep "$SSD_POLL_INTERVAL"
        waited=$(( waited + SSD_POLL_INTERVAL ))
    done

    warn "could not mount $SSD_MOUNT_POINT after ${SSD_WAIT_SECONDS}s — check 'journalctl -xe' and /etc/fstab. Continuing anyway."
    return 1
}

# ---------------------------------------------------------------------------
# Wi-Fi state (NetworkManager)
# ---------------------------------------------------------------------------

# SSID currently associated on $WIFI_IFACE, or empty if not connected.
current_ssid() {
    local conn
    conn="$(nmcli -t -g GENERAL.CONNECTION device show "$WIFI_IFACE" 2>/dev/null || true)"
    if [[ -z "$conn" || "$conn" == "--" ]]; then
        return 0
    fi
    nmcli -t -g 802-11-wireless.ssid connection show "$conn" 2>/dev/null || true
}

# Block until NetworkManager has either associated to something or we time
# out. Prints the SSID (possibly empty) on stdout.
wait_for_wifi() {
    local waited=0 ssid
    while (( waited < WIFI_WAIT_SECONDS )); do
        ssid="$(current_ssid)"
        if [[ -n "$ssid" ]]; then
            echo "$ssid"
            return 0
        fi
        sleep "$WIFI_POLL_INTERVAL"
        waited=$(( waited + WIFI_POLL_INTERVAL ))
    done
    echo ""
}

# ---------------------------------------------------------------------------
# AP + server
# ---------------------------------------------------------------------------

stop_ap_and_server() {
    if nmcli -t -f NAME connection show --active 2>/dev/null | grep -qx "$AP_CONNECTION"; then
        log "bringing down $AP_CONNECTION connection"
        nmcli connection down "$AP_CONNECTION" || warn "failed to bring down $AP_CONNECTION"
    fi
    if systemctl is-active --quiet musicbox.service; then
        log "stopping musicbox.service"
        systemctl stop musicbox.service || warn "failed to stop musicbox.service"
    fi
}

start_ap_and_server() {
    # musicbox-ap ships with autoconnect enabled (see
    # deployment/networking/nm-musicbox-ap.nmconnection) so it can come up
    # on its own if this script never runs. Since we're driving it
    # explicitly here based on which SSID we're on, turn that off so
    # NetworkManager's own autoconnect priority race can't preempt joining
    # $HOME_SSID first.
    nmcli connection modify "$AP_CONNECTION" autoconnect no 2>/dev/null || true

    if ! nmcli -t -f NAME connection show --active 2>/dev/null | grep -qx "$AP_CONNECTION"; then
        log "bringing up $AP_CONNECTION connection"
        nmcli connection up "$AP_CONNECTION"
    else
        log "$AP_CONNECTION already up"
    fi

    if ! systemctl is-active --quiet musicbox.service; then
        log "starting musicbox.service"
        systemctl start musicbox.service
    else
        log "musicbox.service already running"
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
    # A missing SSD shouldn't stop us from picking a network mode (mount_ssd
    # has already warned), so don't let `set -e` abort on its failure.
    mount_ssd || true

    # Make sure the AP profile never races NetworkManager's own autoconnect
    # logic ahead of a chance to join $HOME_SSID.
    nmcli connection modify "$AP_CONNECTION" autoconnect no 2>/dev/null || true

    local ssid
    ssid="$(wait_for_wifi)"

    if [[ "$ssid" == "$HOME_SSID" ]]; then
        log "on '$HOME_SSID' — home mode: SSD mounted, no server/AP"
        stop_ap_and_server
    elif [[ -n "$ssid" ]]; then
        log "on unrecognized SSID '$ssid' — treating as away from home, starting AP mode"
        start_ap_and_server
    else
        log "no Wi-Fi association after ${WIFI_WAIT_SECONDS}s — starting AP mode"
        start_ap_and_server
    fi
}

main "$@"
