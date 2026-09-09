#!/usr/bin/env bash
#
# MusicBox Raspberry Pi installer.
#
# Installs the musicbox-server binary + config as a systemd service running
# under a dedicated unprivileged "musicbox" user, and — only with --with-ap —
# the Wi-Fi access point (hostapd/dnsmasq or NetworkManager) and mDNS
# (avahi) configuration described in deployment/README.md.
#
# Safe to re-run: every step below either checks before writing or overwrites
# only files this repo owns outright (see the "Overwrite policy" comment
# near the top of each install_* function). NOT tested on real Raspberry Pi
# hardware — this was written and reviewed on a development machine with no
# Pi available; read deployment/README.md before trusting it on a device you
# care about, and treat first boot as a test, not a known-good deploy.
#
# Usage:
#   sudo ./install.sh [options]
#
# Options:
#   --with-ap             Also install & enable the Wi-Fi AP (hostapd/
#                          dnsmasq or NetworkManager, auto-detected) and
#                          avahi mDNS. Default: off — many installs join an
#                          existing home LAN instead of running their own AP.
#   --binary PATH          Path to the built musicbox-server executable.
#                          Default: search common CMake build output dirs
#                          (see find_binary()).
#   --prefix PATH          Install prefix for the binary. Default: /opt/musicbox
#   --config-dir PATH      Config directory. Default: /etc/musicbox
#   --data-dir PATH        SQLite DB / runtime state directory.
#                          Default: /var/lib/musicbox
#   --library-path PATH    Music library root. Default: /media/musicbox/music
#                          May be given more than once for multiple roots.
#   --force                Overwrite an existing /etc/musicbox/musicbox.toml
#                          instead of leaving it untouched (see policy below).
#   --keep-existing-ap-config
#                          With --with-ap: if hostapd.conf/dnsmasq.conf/the
#                          NetworkManager profile already exist, leave them
#                          as-is instead of re-copying from this repo. Use
#                          this once you've hand-edited the installed copies
#                          (e.g. changed the Wi-Fi passphrase in place)
#                          and don't want install.sh to clobber that.
#   --keep-hostapd-on-bookworm
#                          With --with-ap on a NetworkManager system: use the
#                          classic hostapd.conf/dnsmasq.conf path (marking
#                          wlan0 "unmanaged" in NetworkManager) instead of
#                          the default NetworkManager-native AP profile. See
#                          deployment/README.md for why you might want this.
#   --skip-enable          Install everything but don't run `systemctl
#                          enable`/`restart` (useful when building an SD card
#                          image offline, chrooted, where systemctl can't
#                          talk to a running systemd).
#   -h, --help             Show this help.
#
# Environment:
#   MUSICBOX_WIFI_PASSPHRASE   WPA2-PSK passphrase to install in place of the
#                               placeholder in hostapd.conf /
#                               nm-musicbox-ap.nmconnection. If unset, the
#                               placeholder "ChangeMe-MusicBox-2024" ships as
#                               a literal, deliberately-recognizable default —
#                               you MUST change it before relying on this
#                               device's Wi-Fi being private. See
#                               deployment/README.md.
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

WITH_AP=0
FORCE=0
KEEP_EXISTING_AP_CONFIG=0
KEEP_HOSTAPD_ON_BOOKWORM=0
SKIP_ENABLE=0
BINARY_PATH=""
PREFIX="/opt/musicbox"
CONFIG_DIR="/etc/musicbox"
DATA_DIR="/var/lib/musicbox"
LIBRARY_PATHS=()

log()  { printf '[musicbox-install] %s\n' "$*"; }
warn() { printf '[musicbox-install] WARNING: %s\n' "$*" >&2; }
die()  { printf '[musicbox-install] ERROR: %s\n' "$*" >&2; exit 1; }

print_help() {
    # Reprint the usage block at the top of this file.
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-ap) WITH_AP=1; shift ;;
        --binary) BINARY_PATH="$2"; shift 2 ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        --config-dir) CONFIG_DIR="$2"; shift 2 ;;
        --data-dir) DATA_DIR="$2"; shift 2 ;;
        --library-path) LIBRARY_PATHS+=("$2"); shift 2 ;;
        --force) FORCE=1; shift ;;
        --keep-existing-ap-config) KEEP_EXISTING_AP_CONFIG=1; shift ;;
        --keep-hostapd-on-bookworm) KEEP_HOSTAPD_ON_BOOKWORM=1; shift ;;
        --skip-enable) SKIP_ENABLE=1; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) die "unknown option '$1' (see --help)" ;;
    esac
done

if [[ ${#LIBRARY_PATHS[@]} -eq 0 ]]; then
    LIBRARY_PATHS=("/media/musicbox/music")
fi

if [[ $EUID -ne 0 ]]; then
    die "must be run as root (sudo ./install.sh ...)"
fi

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

find_binary() {
    if [[ -n "$BINARY_PATH" ]]; then
        [[ -x "$BINARY_PATH" ]] || die "--binary '$BINARY_PATH' does not exist or isn't executable"
        echo "$BINARY_PATH"
        return
    fi
    local candidates=(
        "$SCRIPT_DIR/../build/server/musicbox-server"
        "$SCRIPT_DIR/../build-release/server/musicbox-server"
        "$SCRIPT_DIR/../server/musicbox-server"
    )
    for c in "${candidates[@]}"; do
        if [[ -x "$c" ]]; then
            echo "$c"
            return
        fi
    done
    die "couldn't find a built musicbox-server binary; build it first (cmake --build build) or pass --binary <path>"
}

# Idempotently insert/replace a MUSICBOX-owned block in a file that we don't
# otherwise own (e.g. /etc/dhcpcd.conf, /etc/avahi/avahi-daemon.conf), using
# marker comments so re-running this script updates our block in place
# instead of appending duplicates.
replace_marked_block() {
    local file="$1" begin_marker="$2" end_marker="$3" content_file="$4"
    touch "$file"
    if grep -qF "$begin_marker" "$file"; then
        # Replace the existing block (from begin marker to end marker, inclusive).
        local tmp
        tmp="$(mktemp)"
        awk -v b="$begin_marker" -v e="$end_marker" '
            $0 == b { skip=1 }
            !skip { print }
            $0 == e { skip=0 }
        ' "$file" > "$tmp"
        mv "$tmp" "$file"
    fi
    {
        echo "$begin_marker"
        cat "$content_file"
        echo "$end_marker"
    } >> "$file"
}

substitute_passphrase() {
    local src="$1" dst="$2"
    local passphrase="${MUSICBOX_WIFI_PASSPHRASE:-}"
    if [[ -z "$passphrase" ]]; then
        warn "MUSICBOX_WIFI_PASSPHRASE not set — installing $dst with the placeholder passphrase from $src. CHANGE IT before relying on this device (see deployment/README.md)."
        cp "$src" "$dst"
    else
        if [[ ${#passphrase} -lt 8 || ${#passphrase} -gt 63 ]]; then
            die "MUSICBOX_WIFI_PASSPHRASE must be 8-63 characters (WPA2-PSK requirement), got ${#passphrase}"
        fi
        # Escape sed's replacement-text metacharacters (\, &, and the /
        # delimiter) so an arbitrary passphrase can't break the expression.
        local escaped="${passphrase//\\/\\\\}"
        escaped="${escaped//&/\\&}"
        escaped="${escaped//\//\\/}"
        sed -e "s/ChangeMe-MusicBox-2024/${escaped}/" "$src" > "$dst"
    fi
}

service_active_or_installed() {
    # True if a systemd unit exists (installed), whether or not it's running —
    # used to detect "is this an NM system or a dhcpcd system" without
    # requiring the unit to already be active (e.g. first boot).
    systemctl list-unit-files "$1" >/dev/null 2>&1 && \
        systemctl list-unit-files "$1" | grep -q "$1"
}

# ---------------------------------------------------------------------------
# 1. musicbox system user/group
# ---------------------------------------------------------------------------

install_user() {
    if getent group musicbox >/dev/null 2>&1; then
        log "group 'musicbox' already exists, skipping"
    else
        groupadd --system musicbox
        log "created system group 'musicbox'"
    fi

    if id -u musicbox >/dev/null 2>&1; then
        log "user 'musicbox' already exists, skipping"
    else
        useradd --system --gid musicbox --home-dir "$DATA_DIR" \
            --no-create-home --shell /usr/sbin/nologin musicbox
        log "created system user 'musicbox' (no login, home=$DATA_DIR)"
    fi
}

# ---------------------------------------------------------------------------
# 2. Binary + config + data dirs
#
# Overwrite policy:
#   - the binary is always (re)installed — that's the point of re-running
#     this script after a rebuild.
#   - musicbox.toml is written ONLY if missing, unless --force. This is the
#     one file admins are expected to hand-edit (library paths, log level),
#     so we never clobber it silently.
# ---------------------------------------------------------------------------

install_binary_and_config() {
    local binary
    binary="$(find_binary)"

    install -d -m 755 "$PREFIX/bin"
    install -m 755 -o root -g root "$binary" "$PREFIX/bin/musicbox-server"
    log "installed musicbox-server binary to $PREFIX/bin/musicbox-server"

    install -d -m 755 -o root -g musicbox "$CONFIG_DIR"

    install -d -m 750 -o musicbox -g musicbox "$DATA_DIR"

    for lib in "${LIBRARY_PATHS[@]}"; do
        install -d -m 755 -o musicbox -g musicbox "$lib"
        log "ensured library directory $lib exists (owner musicbox:musicbox)"
    done

    local config_dst="$CONFIG_DIR/musicbox.toml"
    if [[ -f "$config_dst" && $FORCE -eq 0 ]]; then
        log "$config_dst already exists, leaving it untouched (pass --force to overwrite)"
        return
    fi

    if [[ ${#LIBRARY_PATHS[@]} -eq 1 && "${LIBRARY_PATHS[0]}" == "/media/musicbox/music" && "$DATA_DIR" == "/var/lib/musicbox" ]]; then
        # Common case: no --library-path/--data-dir overrides, so the
        # checked-in template already says exactly the right thing. Install
        # it verbatim rather than regenerating an equivalent file, so the
        # template stays the single source of truth for the default config.
        install -m 640 -o root -g musicbox "$SCRIPT_DIR/config/musicbox.toml" "$config_dst"
        log "wrote $config_dst (from deployment/config/musicbox.toml)"
    else
        # --library-path and/or --data-dir were overridden: the static
        # template can't express that (TOML arrays don't sed-substitute
        # cleanly), so generate an equivalent file with the same shape.
        local tmp
        tmp="$(mktemp)"
        {
            echo '[server]'
            echo 'address = "0.0.0.0"'
            echo 'port = 8080'
            echo
            echo '[library]'
            echo 'paths = ['
            for lib in "${LIBRARY_PATHS[@]}"; do
                printf '    "%s",\n' "$lib"
            done
            echo ']'
            echo
            echo '[database]'
            printf 'path = "%s/musicbox.db"\n' "$DATA_DIR"
            echo
            echo '[network]'
            echo 'mdns_name = "musicbox"'
            echo
            echo '[logging]'
            echo 'level = "info"'
        } > "$tmp"
        install -m 640 -o root -g musicbox "$tmp" "$config_dst"
        rm -f "$tmp"
        log "wrote $config_dst (generated, --library-path/--data-dir overridden)"
    fi
}

# ---------------------------------------------------------------------------
# 3. systemd unit
#
# Overwrite policy: always reinstalled from this repo (it's not meant to be
# hand-edited in place — use `systemctl edit musicbox` for local overrides,
# which survives this).
# ---------------------------------------------------------------------------

install_systemd_unit() {
    local dst="/etc/systemd/system/musicbox.service"
    local joined_paths
    joined_paths="$(printf '%s ' "${LIBRARY_PATHS[@]}")"
    joined_paths="${joined_paths% }"
    sed "s|@MUSICBOX_LIBRARY_PATH@|${joined_paths}|" \
        "$SCRIPT_DIR/systemd/musicbox.service" > "$dst"
    chmod 644 "$dst"
    log "installed $dst (library ReadOnlyPaths=$joined_paths)"

    if [[ $SKIP_ENABLE -eq 1 ]]; then
        log "skipping systemctl enable/daemon-reload (--skip-enable)"
        return
    fi
    systemctl daemon-reload
    systemctl enable musicbox.service
    log "enabled musicbox.service (not starting it automatically — see deployment/README.md for why, and run 'systemctl start musicbox' when ready)"
}

# ---------------------------------------------------------------------------
# 4. --with-ap: Wi-Fi AP + DHCP + mDNS
#
# Overwrite policy: hostapd.conf/dnsmasq.conf/the NM profile are reinstalled
# from this repo each run UNLESS --keep-existing-ap-config is given (use
# that once you've hand-edited the installed copies, e.g. changed the
# passphrase directly instead of via MUSICBOX_WIFI_PASSPHRASE).
# ---------------------------------------------------------------------------

install_ap_networkmanager() {
    log "NetworkManager detected: installing the native AP connection profile (skipping hostapd)"
    local dst="/etc/NetworkManager/system-connections/musicbox-ap.nmconnection"
    if [[ -f "$dst" && $KEEP_EXISTING_AP_CONFIG -eq 1 ]]; then
        log "$dst already exists, leaving it untouched (--keep-existing-ap-config)"
    else
        substitute_passphrase "$SCRIPT_DIR/networking/nm-musicbox-ap.nmconnection" "$dst"
        chmod 600 "$dst"
        chown root:root "$dst"
        log "installed $dst"
    fi

    install_dnsmasq

    if [[ $SKIP_ENABLE -eq 0 ]]; then
        nmcli connection reload || warn "nmcli connection reload failed; you may need to reboot for the AP profile to take effect"
        nmcli connection up musicbox-ap || warn "could not bring musicbox-ap up now; it will auto-connect on next boot"
    fi
}

install_ap_hostapd_dhcpcd() {
    log "dhcpcd detected: installing hostapd + dnsmasq + static-IP dhcpcd stanza"

    install -d -m 755 /etc/hostapd
    local hostapd_dst="/etc/hostapd/hostapd.conf"
    if [[ -f "$hostapd_dst" && $KEEP_EXISTING_AP_CONFIG -eq 1 ]]; then
        log "$hostapd_dst already exists, leaving it untouched (--keep-existing-ap-config)"
    else
        substitute_passphrase "$SCRIPT_DIR/networking/hostapd.conf" "$hostapd_dst"
        log "installed $hostapd_dst"
    fi

    if [[ -f /etc/default/hostapd ]] && ! grep -q '^DAEMON_CONF="/etc/hostapd/hostapd.conf"' /etc/default/hostapd; then
        sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
        log "pointed /etc/default/hostapd at /etc/hostapd/hostapd.conf"
    fi

    replace_marked_block /etc/dhcpcd.conf \
        "# BEGIN MUSICBOX (managed by deployment/install.sh, do not edit by hand)" \
        "# END MUSICBOX" \
        "$SCRIPT_DIR/networking/dhcpcd.conf.append"
    log "updated /etc/dhcpcd.conf with the MusicBox static-IP block"

    install_dnsmasq

    if [[ $SKIP_ENABLE -eq 0 ]]; then
        systemctl unmask hostapd.service 2>/dev/null || true
        systemctl enable hostapd.service
        systemctl restart dhcpcd.service || warn "failed to restart dhcpcd; reboot to apply the static IP"
        systemctl restart hostapd.service || warn "failed to restart hostapd; check 'journalctl -u hostapd'"
    fi
}

install_ap_hostapd_unmanaged_nm() {
    log "NetworkManager detected but --keep-hostapd-on-bookworm requested: marking wlan0 unmanaged"
    install -d -m 755 /etc/NetworkManager/conf.d
    cp "$SCRIPT_DIR/networking/musicbox-unmanaged.conf" /etc/NetworkManager/conf.d/musicbox-unmanaged.conf
    cp "$SCRIPT_DIR/networking/musicbox-ap-address.service" /etc/systemd/system/musicbox-ap-address.service

    install -d -m 755 /etc/hostapd
    local hostapd_dst="/etc/hostapd/hostapd.conf"
    if [[ -f "$hostapd_dst" && $KEEP_EXISTING_AP_CONFIG -eq 1 ]]; then
        log "$hostapd_dst already exists, leaving it untouched (--keep-existing-ap-config)"
    else
        substitute_passphrase "$SCRIPT_DIR/networking/hostapd.conf" "$hostapd_dst"
    fi
    if [[ -f /etc/default/hostapd ]] && ! grep -q '^DAEMON_CONF="/etc/hostapd/hostapd.conf"' /etc/default/hostapd; then
        sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
    fi

    install_dnsmasq

    if [[ $SKIP_ENABLE -eq 0 ]]; then
        systemctl reload NetworkManager || true
        systemctl daemon-reload
        systemctl enable --now musicbox-ap-address.service
        systemctl unmask hostapd.service 2>/dev/null || true
        systemctl enable --now hostapd.service
    fi
}

install_dnsmasq() {
    install -d -m 755 /etc/dnsmasq.d
    local dst="/etc/dnsmasq.d/musicbox.conf"
    if [[ -f "$dst" && $KEEP_EXISTING_AP_CONFIG -eq 1 ]]; then
        log "$dst already exists, leaving it untouched (--keep-existing-ap-config)"
    else
        cp "$SCRIPT_DIR/networking/dnsmasq.conf" "$dst"
        log "installed $dst"
    fi
    if [[ $SKIP_ENABLE -eq 0 ]]; then
        systemctl enable dnsmasq.service 2>/dev/null || warn "dnsmasq.service not found — is dnsmasq installed? (apt install dnsmasq)"
        systemctl restart dnsmasq.service 2>/dev/null || warn "failed to restart dnsmasq; check 'journalctl -u dnsmasq'"
    fi
}

install_avahi() {
    install -d -m 755 /etc/avahi/services
    cp "$SCRIPT_DIR/networking/musicbox.service" /etc/avahi/services/musicbox.service
    log "installed /etc/avahi/services/musicbox.service"

    # Force the advertised hostname to musicbox.local regardless of the
    # device's actual `hostname`, by setting host-name= in avahi-daemon.conf.
    # Idempotent: replaces any existing host-name= line under [server],
    # otherwise appends one.
    local conf="/etc/avahi/avahi-daemon.conf"
    if [[ -f "$conf" ]]; then
        if grep -qE '^\s*host-name=' "$conf"; then
            sed -i 's/^\s*host-name=.*/host-name=musicbox/' "$conf"
        else
            sed -i '/^\[server\]/a host-name=musicbox' "$conf"
        fi
        log "set host-name=musicbox in $conf"
    else
        warn "$conf not found — is avahi-daemon installed? (apt install avahi-daemon)"
    fi

    if [[ $SKIP_ENABLE -eq 0 ]]; then
        systemctl enable avahi-daemon.service 2>/dev/null || warn "avahi-daemon.service not found — is avahi-daemon installed?"
        systemctl restart avahi-daemon.service 2>/dev/null || warn "failed to restart avahi-daemon; check 'journalctl -u avahi-daemon'"
    fi
}

install_ap() {
    if command -v nmcli >/dev/null 2>&1 && systemctl is-active --quiet NetworkManager 2>/dev/null; then
        if [[ $KEEP_HOSTAPD_ON_BOOKWORM -eq 1 ]]; then
            install_ap_hostapd_unmanaged_nm
        else
            install_ap_networkmanager
        fi
    elif command -v dhcpcd >/dev/null 2>&1 || service_active_or_installed dhcpcd.service; then
        install_ap_hostapd_dhcpcd
    else
        warn "could not detect NetworkManager or dhcpcd — skipping AP network setup."
        warn "Install hostapd+dnsmasq (or NetworkManager) manually; see deployment/README.md."
    fi
    install_avahi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

log "installing musicbox-server (prefix=$PREFIX config=$CONFIG_DIR data=$DATA_DIR library=${LIBRARY_PATHS[*]})"

install_user
install_binary_and_config
install_systemd_unit

if [[ $WITH_AP -eq 1 ]]; then
    install_ap
else
    log "--with-ap not given: skipping Wi-Fi AP / dnsmasq / avahi setup (device will use whatever network it's already joined to)"
fi

log "done."
log "Next: review $CONFIG_DIR/musicbox.toml, then 'systemctl start musicbox' and check 'systemctl status musicbox'."
log "See deployment/README.md for verification steps (AP up, musicbox.local resolving, service healthy)."
