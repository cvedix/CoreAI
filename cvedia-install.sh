#!/bin/bash
# aka get.cvedia.com
# The { } block ensures the entire script is read from the pipe before
# executing, so we can safely reconnect stdin to the terminal.
{

# When piped (curl | bash), reconnect stdin to the terminal
# so interactive package prompts (EULA, network config) work.
#
# Two cases to handle:
#   1. Old sudo (no use_pty): stdin is the pipe itself, not a TTY
#   2. New sudo (use_pty, default on Ubuntu 22.04+): sudo allocates a PTY for
#      the child, so stdin appears to be a TTY. But after curl finishes and the
#      pipe closes, sudo stops relaying input to that PTY. The result: dialog
#      renders on screen but keyboard input never arrives.
#
# We detect case 2 by checking if the parent process (sudo) has stdin from a
# pipe: [ -p /proc/$PPID/fd/0 ]. In both cases, we find the real terminal and
# use text-based (teletype) prompts for maximum compatibility.
NEED_TTY_FIX=0
if [ ! -t 0 ]; then
    NEED_TTY_FIX=1
elif [ -n "$SUDO_USER" ] && [ -p "/proc/$PPID/fd/0" ] 2>/dev/null; then
    NEED_TTY_FIX=1
fi

if [ "$NEED_TTY_FIX" = "1" ]; then
    if [ -n "$SUDO_USER" ]; then
        PARENT_TTY=$(readlink /proc/$PPID/fd/2 2>/dev/null)
        if [ -n "$PARENT_TTY" ] && [ -c "$PARENT_TTY" ]; then
            exec < "$PARENT_TTY" 2>/dev/null && export DEBIAN_FRONTEND=teletype
        fi
    fi
    if [ ! -t 0 ]; then
        exec < /dev/tty 2>/dev/null || export DEBIAN_FRONTEND=noninteractive
    fi
fi

set -o pipefail

if [[ $EUID -ne 0 ]]; then
    echo "Please run this script as root or with sudo, eg:"
    echo "sudo $0 $*"
    exit 1
fi

INSTALLER_VERSION=2026.02.19
echo "-- Installer v${INSTALLER_VERSION}"

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:$PATH
APT_SRC_BASE=/etc/apt/sources.list.d
FORCE_SSL=${FORCE_SSL-0}
FORCE_HTTP=${FORCE_HTTP-0}

# skip auto install
SKIP_INSTALL=${SKIP_INSTALL-0}
# skip instructions at the end
SKIP_INSTRUCTIONS=${SKIP_INSTRUCTIONS-0}
# skip nx detection for auto install
SKIP_NX_DETECTION=${SKIP_NX_DETECTION-0}

# install cockpit for server management (experimental)
INSTALL_COCKPIT=${INSTALL_COCKPIT-0}
# install cvedia-box (fully managed box)
INSTALL_CVEDIA_BOX=${INSTALL_CVEDIA_BOX-0}
# ignore locks and install latest version anyway
IGNORE_VERSION_LOCK=${IGNORE_VERSION_LOCK-0}
# lock to a specific version (user override)
VERSION_LOCK=${VERSION_LOCK-""}
# install thin client only
THIN_CLIENT=${THIN_CLIENT-0}
# install rtserver alongside thin client
RTSERVER=${RTSERVER-0}
# force reinstall even if already installed
FORCE_REINSTALL=${FORCE_REINSTALL-0}

BASE_URI=${BASE_URI-apt.cvedia.com}
APT_CMD=$(command -v apt-get || command -v apt)

# Test HTTPS availability and fallback to HTTP if needed
PROTO=http

if [ "$FORCE_HTTP" = "1" ]; then
    echo "-- Forcing HTTP protocol"
    PROTO=http
elif [ "$FORCE_SSL" = "1" ]; then
    echo "-- Forcing HTTPS protocol"
    PROTO=https
else
    # Auto-detect: prefer HTTPS, fallback to HTTP if it fails
    echo "-- Testing HTTPS connectivity..."
    if command -v curl >/dev/null 2>&1; then
        # Test HTTPS with a quick connection attempt
        if curl -fsSL --connect-timeout 5 --max-time 10 "https://${BASE_URI}/key.gpg" >/dev/null 2>&1; then
            PROTO=https
            echo "-- HTTPS is available, using secure connection"
        else
            PROTO=http
            echo "-- HTTPS failed (possibly due to clock skew or missing CA certificates), falling back to HTTP"
            echo "-- WARN: Using insecure HTTP connection. Consider fixing system time or installing ca-certificates"
        fi
    else
        # curl not available yet, default to HTTP (we'll install curl later)
        PROTO=http
        echo "-- curl not available, defaulting to HTTP (will retry HTTPS detection after curl installation)"
    fi
fi

BASE_URL=${PROTO}://$BASE_URI

# first pass
ARCH=${ARCH-$(uname -p)}

# second pass
if [ -z "$ARCH" ] || [ "$ARCH" = "unknown" ] ; then
    if [ -n "$(command -v lscpu)" ] ; then
        x=$(lscpu | head -n1 | rev | cut -d' ' -f1 | rev)
    fi

    if [ -z "$x" ] && [ -n "$(command -v uname)" ] ; then
        x=$(uname -m)
    fi

    if [ -z "$x" ] || [ "$x" = "unknown" ]; then
        bits=0
        is_arm=0
        
        if [ -n "$(command -v getconf)" ] ; then
            bits=$(getconf LONG_BIT)
        fi
        
        if [ -f "/proc/cpuinfo" ] ; then
            is_arm=$(grep -ci "arm" /proc/cpuinfo)
        fi

        if [ "$is_arm" -gt "0" ] && [ "$bits" = "64" ] ; then
            ARCH=aarch64
        elif [ "$is_arm" -gt "0" ] ; then # 32 bit arm
            ARCH=armv7l
        else # not arm
            ARCH=x86_64
        fi
    fi
    
    ARCH=$x
fi

if [ ! -d "$APT_SRC_BASE" ] ; then
    echo "Error: $APT_SRC_BASE does not exist, aborting!"
    exit 1
fi

if [ -n "$APT_CMD" ] ; then
    CURL_WAS_MISSING=0
    if [ -z "$(command -v curl)" ] ; then
        CURL_WAS_MISSING=1
        (
            set -x
            if ! $APT_CMD update ; then $APT_CMD update --fix-missing; fi
            DEBIAN_FRONTEND=noninteractive $APT_CMD install curl -y
        )
        RT=$?
        if [ "$RT" -ne "0" ] && [ -z "$(command -v curl)" ]; then
            echo "Failed to install curl, aborting!"
            exit 1
        fi
    fi

    # Retry HTTPS detection if curl was just installed
    if [ "$CURL_WAS_MISSING" = "1" ] && [ "$FORCE_HTTP" != "1" ] && [ "$FORCE_SSL" != "1" ]; then
        echo "-- Retrying HTTPS connectivity test with newly installed curl..."
        if curl -fsSL --connect-timeout 5 --max-time 10 "https://${BASE_URI}/key.gpg" >/dev/null 2>&1; then
            PROTO=https
            BASE_URL=${PROTO}://$BASE_URI
            echo "-- HTTPS is available, upgraded to secure connection"
        else
            echo "-- HTTPS still unavailable, continuing with HTTP"
        fi
    fi

    ### HARDWARE DETECTION ####################################################
    
    IS_ROCKCHIP=${IS_ROCKCHIP-0}
    IS_JETSON=${IS_JETSON-0}
    IS_NVIDIA=${IS_NVIDIA-0}
    IS_INTEL_NUC=${IS_INTEL_NUC-0}
    
    UNHOLD_EXTRAS=""
    PACKAGES_TO_HOLD=""

    if [ "$IGNORE_VERSION_LOCK" = "0" ] ; then
        # Detect Rockchip devices
        if grep -qi "rockchip" /proc/cpuinfo 2>/dev/null; then
            IS_ROCKCHIP=1
            echo "-- Detected Rockchip device"
        elif [ -e "/dev/rknpu" ] || [ -e "/sys/kernel/debug/rknpu" ] || [ -e "/sys/kernel/debug/rkrga" ]; then
            IS_ROCKCHIP=1
            echo "-- Detected Rockchip NPU"
        elif [ -d "/sys/devices/platform/rknpu" ]; then
            IS_ROCKCHIP=1
            echo "-- Detected Rockchip platform"
        fi

        # Detect Jetson devices and L4T version
        JETPACK_VERSION=""
        if [ -f "/etc/nv_tegra_release" ]; then
            IS_JETSON=1
            # Parse L4T version from /etc/nv_tegra_release
            # Format: "# R32 (release), REVISION: 7.1, ..."
            JETPACK_MAJOR=$(grep -oP "R\K[0-9]+" /etc/nv_tegra_release 2>/dev/null | head -n1)
            JETPACK_MINOR=$(grep -oP "REVISION: \K[0-9]+\.[0-9]+" /etc/nv_tegra_release 2>/dev/null | head -n1)

            if [ -n "$JETPACK_MAJOR" ]; then
                JETPACK_VERSION="R${JETPACK_MAJOR}"
                if [ -n "$JETPACK_MINOR" ]; then
                    JETPACK_VERSION="${JETPACK_VERSION}.${JETPACK_MINOR}"
                fi
                echo "-- Detected Jetson device (L4T ${JETPACK_VERSION})"
            else
                echo "-- Detected Jetson device"
            fi
        fi

        # Detect NVIDIA GPU
        # if [ "$IS_ROCKCHIP" = "0" ] && [ "$IS_JETSON" = "0" ] ; then
        #     # Install pciutils for hardware detection (not critical if fails)
        #     if [ -z "$(command -v lspci)" ] ; then
        #         echo "Installing pciutils for hardware detection..."
        #         $APT_CMD install pciutils -y >/dev/null 2>&1 || echo "WARN: Could not install pciutils"
        #     fi
        #
        #     if command -v lspci >/dev/null 2>&1; then
        #         NVIDIA_COUNT=$(lspci 2>/dev/null | grep -ci "nvidia" || echo "0")
        #         if [ "$NVIDIA_COUNT" -gt "0" ]; then
        #             IS_NVIDIA=1
        #             echo "-- Detected NVIDIA GPU"
        #         fi
        #     elif command -v nvidia-smi >/dev/null 2>&1; then
        #         if nvidia-smi >/dev/null 2>&1; then
        #             IS_NVIDIA=1
        #             echo "-- Detected NVIDIA GPU"
        #         fi
        #     fi
        # fi

        # Fetch version locks and dependencies based on hardware detection
        HARDWARE_TYPE="default"

        if [ "$IS_ROCKCHIP" = "1" ] ; then
            HARDWARE_TYPE="rockchip"
        elif [ "$IS_JETSON" = "1" ] ; then
            HARDWARE_TYPE="jetson"
        elif [ "$IS_NVIDIA" = "1" ] ; then
            HARDWARE_TYPE="nvidia"
        fi

        # Detect Hub8 hardware (Intel N305 / Nano-N305L2C2)
        IS_HUB8=${IS_HUB8-0}
        HUB8_BOARD=$(cat /sys/class/dmi/id/board_name 2>/dev/null || true)
        HUB8_CPU=$(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 || true)
        if [ "$HUB8_BOARD" = "Nano-N305L2C2" ] || echo "$HUB8_CPU" | grep -qi "N305"; then
            IS_HUB8=1
            echo "-- Detected Hub8 hardware (board: ${HUB8_BOARD:-unknown})"
        fi

        if [ -n "$HARDWARE_TYPE" ]; then
            if [ -n "$VERSION_LOCK" ]; then
                # User provided VERSION_LOCK, skip remote fetch
                echo "-- Using user-specified version lock: ${VERSION_LOCK}"
            else
                # Fetch version lock if available
                echo "-- Checking for ${HARDWARE_TYPE} version lock..."
                VERSION_LOCK=$(curl -fsSL "https://bin.cvedia.com/${HARDWARE_TYPE}.version" 2>/dev/null || echo "")

                if [ -n "$VERSION_LOCK" ]; then
                    echo "-- Found version lock: ${VERSION_LOCK}"
                fi
            fi

            # Fetch additional dependencies if available
            echo "-- Checking for ${HARDWARE_TYPE} dependencies..."
            HARDWARE_DEPENDS=$(curl -fsSL "https://bin.cvedia.com/${HARDWARE_TYPE}.depends" 2>/dev/null || echo "")

            if [ -n "$HARDWARE_DEPENDS" ]; then
                # Dependencies file format: space-separated packages, optionally with versions
                # Examples: "librockchip-rknn librockchip-mpp" or "librockchip-rknn=2.3.2 librockchip-mpp"
                for pkg in $HARDWARE_DEPENDS; do
                    # Extract package name (strip version if present)
                    pkg_name="${pkg%%=*}"
                    UNHOLD_EXTRAS="$UNHOLD_EXTRAS $pkg_name"

                    # If package has version lock, add to hold list
                    if echo "$pkg" | grep -q "="; then
                        PACKAGES_TO_HOLD="$PACKAGES_TO_HOLD $pkg_name"
                    fi
                done

                # shellcheck disable=SC2086
                INSTALL_EXTRAS="$INSTALL_EXTRAS $HARDWARE_DEPENDS"
                echo "-- Added ${HARDWARE_TYPE} dependencies: ${HARDWARE_DEPENDS}"
            fi
        fi
    else
        if [ -n "$VERSION_LOCK" ]; then
            echo "-- Using user-specified version lock: ${VERSION_LOCK}"
        fi
        echo "-- Ignoring hardware detection due to IGNORE_VERSION_LOCK"
    fi

    echo "Adding cvedia repository key..."
    
    # ubuntu 22+
    if [ -n "$(command -v gpg)" ] ; then
        KEY_PATH=/etc/apt/trusted.gpg.d/cvedia.gpg
        if [ -e "$KEY_PATH" ] ; then
            rm -rf "$KEY_PATH"
        fi

        mkdir -p /etc/apt/trusted.gpg.d &> /dev/null
        curl --fail -sqko - ${BASE_URL}/key.gpg | gpg --dearmour -o $KEY_PATH
        RT=$?
        # gpg honours the caller's umask, so a restrictive root umask (e.g. 0077)
        # leaves the keyring 0600. apt verifies signatures as the unprivileged
        # '_apt' user, which then cannot read it and reports the repository as
        # unsigned. Stock ubuntu-keyring files are 0644; match them.
        [ -e "$KEY_PATH" ] && chmod 644 "$KEY_PATH"
    # old ubuntu / deb
    elif [ -n "$(command -v apt-key)" ] ; then
        curl --fail -sqko - ${BASE_URL}/key.gpg | apt-key add -
        RT=$?
        # same umask exposure on the legacy keyring written by apt-key
        [ -e /etc/apt/trusted.gpg ] && chmod 644 /etc/apt/trusted.gpg
    else
        echo "-- No tool available (gpg or apt-key) to add cvedia repository key, aborting!"
        exit 1
    fi

    if [ "$RT" -ne "0" ] ; then
        echo "Failed to add cvedia repository key, aborting!"
        exit 1
    fi

    APT_LST_FN=$APT_SRC_BASE/cvedia.list
    
    if [ -e "$APT_LST_FN" ] ; then
        echo "    CVEDIA apt list file already exist, overwriting..."
    fi
    
    if [ -n "$(command -v dpkg-query)" ] ; then
        LIBC6=$(dpkg-query -W -f='${Version}' libc6 | cut -d'-' -f1 | tr -d '.')
    elif [ -n "$(command -v ldd)" ] ; then
        LIBC6=$(ldd --version | head -n1 | rev | cut -d' ' -f1 | rev | tr -d '.')
    fi

    if [ -z "$LIBC6" ] ; then
        echo "WARN: Failed to determine libc6 version, assuming legacy"
        LIBC6=000
    fi

    if [[ "${#LIBC6}" -gt 3 ]] ; then
        LIBC6=${LIBC6:0:3}
    elif [[ "${#LIBC6}" -lt 3 ]] ; then
        LIBC6=$(printf "%-3s" $LIBC6 | tr ' ' '0')
    fi

    echo "Adding cvedia repository..."

    # Determine apt component: hub8 hardware gets curated channel, others get main/legacy
    APT_COMPONENT="${APT_COMPONENT:-main}"
    if [ "$IS_HUB8" = "1" ]; then
        APT_COMPONENT="hub8"
        echo "-- Using hub8 apt channel for curated updates"
    fi

    # Hosts that fail this libc check are served by the 'legacy' component, which
    # carries no *-nxplugin-thin packages at all (thin exists only in main/hub8,
    # from 2026.1.x on). Derive the capability here from the same expression that
    # picks the component, so the two can never disagree.
    #shellcheck disable=SC2235
    if ( [ "$ARCH" = "x86_64" ] && [ "$LIBC6" -ge 234 ] ) || ( [ "$ARCH" != "x86_64" ] && [ "$LIBC6" -ge 231 ] ) ; then
        SUPPORTS_THIN=1
    else
        SUPPORTS_THIN=0
    fi

    # Human-readable form of the packed LIBC6 value (231 -> 2.31) for messages.
    if [ "$LIBC6" = "000" ] ; then
        LIBC6_TEXT="unknown"
    else
        LIBC6_TEXT="${LIBC6:0:1}.${LIBC6:1}"
    fi

    {
        echo "# added by get.cvedia.com @ $(date)"
        echo ""

        if [ "$SUPPORTS_THIN" = "1" ] ; then
            echo "deb ${BASE_URL} cvedia ${APT_COMPONENT}"
            echo "#deb ${BASE_URL} cvedia legacy"
        else
            echo "# old version of libc6 detected, using legacy repository"
            echo "#deb ${BASE_URL} cvedia main"
            echo "deb ${BASE_URL} cvedia legacy"
        fi
        
        if [ -n "$POOLS" ] ; then
            for p in $POOLS; do
                echo "deb ${BASE_URL} cvedia $p"
            done
        fi
    } > $APT_LST_FN

    ### NX VMS DETECTION #######################################################

    if [ "$SKIP_NX_DETECTION" = "0" ] && [ -z "$INSTALL_NX" ] ; then
        echo "-- Checking for existing NX VMS installation..."

        # Check for mediaserver binary in /opt/*/mediaserver/bin/mediaserver
        for path in /opt/*/mediaserver/bin/mediaserver; do
            if [ -f "$path" ]; then
                echo "-- Detected NX VMS installation at: $path"

                # The install directory carries the OEM brand (/opt/<brand>/mediaserver),
                # and build_info.txt is the VMS's own declaration of it, so prefer that
                # and fall back to the directory name.
                NX_DIR=$(dirname "$(dirname "$path")")
                NX_BRAND=$(basename "$(dirname "$NX_DIR")")
                if [ -r "${NX_DIR}/build_info.txt" ]; then
                    NX_BRAND_DECLARED=$(sed -n 's/^brand=//p' "${NX_DIR}/build_info.txt" | tr -d '[:space:]')
                    if [ -n "$NX_BRAND_DECLARED" ]; then
                        NX_BRAND="$NX_BRAND_DECLARED"
                    fi
                fi
                NX_BRAND=$(printf '%s' "$NX_BRAND" | tr '[:upper:]' '[:lower:]')

                # White-label plugins are only published for these brands; every other
                # VMS (stock networkoptix, metavms, ...) uses the generic package.
                case "$NX_BRAND" in
                    hanwha|piko) INSTALL_NX="$NX_BRAND" ;;
                    *)           INSTALL_NX="cvedia-rt" ;;
                esac

                echo "-- Detected VMS brand: ${NX_BRAND}"
                echo "-- Automatically installing ${INSTALL_NX} NX plugin (thin)"
                break
            fi
        done
    fi

    ### INSTALL ###############################################################

    APT_REINSTALL=""
    if [ "$FORCE_REINSTALL" = "1" ] ; then
        APT_REINSTALL="--reinstall"
        echo "-- Force reinstall enabled"
    fi

    if [ "$SKIP_INSTALL" = "0" ] ; then

        # Recover from broken dpkg state if a previous install was aborted
        # (e.g. user cancelled during EULA or network config prompts)
        for broken_pkg in cvedia-rt cvedia-rt-nxplugin cvedia-rt-nxplugin-thin; do
            if dpkg -l "$broken_pkg" 2>/dev/null | grep -qE "^i[HUF]"; then
                echo "-- Detected broken ${broken_pkg} from a previous aborted install, recovering..."
                dpkg --purge --force-remove-reinstreq "$broken_pkg" 2>/dev/null || true
            fi
        done

        HAS_NX=0
        for path in /opt/*/mediaserver/bin/mediaserver; do
            if [ -f "$path" ]; then
                HAS_NX=1
                break
            fi
        done

        if [ "$THIN_CLIENT" = "1" ] ; then
            echo "-- Installing thin client..."

            if [ "$HAS_NX" = "0" ] ; then
                echo "-- NX install not found, aborting!"
                exit 1
            elif [ "$SUPPORTS_THIN" != "1" ] ; then
                # The thin plugin needs glibc 2.34+; this host is on the legacy
                # component, which ships no thin packages. Say so instead of
                # letting apt fail with "Unable to locate package". The full
                # plugin is not offered automatically here because it depends on
                # cvedia-rt, i.e. it would pull in the very runtime a thin-only
                # install is meant to avoid.
                echo "-- ERROR: The thin VMS plugin requires Ubuntu 22.04+ (glibc >= 2.34);"
                echo "--        detected glibc: ${LIBC6_TEXT}."
                echo "--        Install the full plugin and runtime instead:"
                echo "--          INSTALL_NX=cvedia-rt VERSION_LOCK=2025.1.2 curl -fsSL http://get.cvedia.com | sudo bash"
                exit 1
            else
                # Honour the detected or caller-supplied brand so OEM boxes get their
                # white-label plugin instead of the generic one.
                NX_BRAND_PKG="${INSTALL_NX:-cvedia-rt}"

                # Starting from 2026.1.2, remove legacy fat plugin if present and use thin only
                if dpkg -l "${NX_BRAND_PKG}-nxplugin" 2>/dev/null | grep -q "^ii"; then
                    echo "-- Removing legacy ${NX_BRAND_PKG}-nxplugin in favor of thin client..."
                    $APT_CMD remove "${NX_BRAND_PKG}-nxplugin" -yf || true
                fi

                THIN_PACKAGES="${NX_BRAND_PKG}-nxplugin-thin"
                if [ "$RTSERVER" = "1" ] ; then
                    echo "-- RTSERVER enabled, will also install cvedia-rt"
                    THIN_PACKAGES="$THIN_PACKAGES cvedia-rt"
                fi

                #shellcheck disable=SC2086
                (
                    set -x
                    if ! $APT_CMD update ; then $APT_CMD update --fix-missing; fi
                    $APT_CMD install $THIN_PACKAGES -yf --allow-downgrades --install-recommends $APT_REINSTALL
                )
                RT=$?

                if [ "$RT" -ne "0" ] ; then
                    echo "ERROR: Failed to install, aborting!"
                    exit 1
                fi
            fi
        else
            if [ "$IS_INTEL_NUC" = "1" ] ; then
                (
                    set +e
                    export DEBIAN_FRONTEND=noninteractive
                    echo "-- Installing Intel NPU / IGP drivers..."
                    
                    apt install -y software-properties-common || true
                    add-apt-repository -y ppa:kobuk-team/intel-graphics || true

                    dpkg --purge --force-remove-reinstreq intel-driver-compiler-npu intel-fw-npu intel-level-zero-npu intel-level-zero-npu-dbgsym || true
                    
                    apt install -y \
                        libze-intel-gpu1 libze1 intel-metrics-discovery intel-opencl-icd \
                        clinfo intel-media-va-driver-non-free libmfx-gen1 libvpl2 libvpl-tools \
                        libva-glx2 va-driver-all vainfo libze-dev libtbb12 || true
                    
                    echo "-- Downloading bundle..."
                    ITEMP=$(mktemp -d)
                    cd "$ITEMP"
                    curl -sqko - https://bin.cvedia.com/intel-npu-bundle.tar.gz | tar xz
                    echo "-- Installing bundle..."
                    apt install ./*.deb -yf --reinstall --allow-downgrades
                    cd /
                    rm -fr "$ITEMP"
                    ldconfig
                )
            fi

            # Build package list with version lock if available
            CVEDIA_PKG="cvedia-rt"

            if [ -n "$VERSION_LOCK" ]; then
                CVEDIA_PKG="cvedia-rt=${VERSION_LOCK}"
                PACKAGES_TO_HOLD="cvedia-rt"
                echo "-- Locking cvedia-rt to version ${VERSION_LOCK}"
            fi

            if [ -n "$INSTALL_NX" ] ; then
                # cvedia-rt >= 2026.1.1 conflicts with fat nxplugin, must use thin
                NX_THIN_MIN=202611
                if [ -n "$VERSION_LOCK" ]; then
                    NX_VER_CMP=$(echo "$VERSION_LOCK" | tr -d '.')
                else
                    # No version lock = latest available, which is >= 2026.1.1
                    NX_VER_CMP=$NX_THIN_MIN
                fi

                if [ "$NX_VER_CMP" -ge "$NX_THIN_MIN" ] 2>/dev/null && [ "$SUPPORTS_THIN" = "1" ] ; then
                    NX_PKG="${INSTALL_NX}-nxplugin-thin"

                    # Remove legacy fat plugin if present
                    if dpkg -l "${INSTALL_NX}-nxplugin" 2>/dev/null | grep -q "^ii"; then
                        echo "-- Removing legacy ${INSTALL_NX}-nxplugin in favor of thin client..."
                        $APT_CMD remove "${INSTALL_NX}-nxplugin" -yf || true
                    fi
                else
                    NX_PKG="${INSTALL_NX}-nxplugin"
                    echo "-- WARN: Installing legacy NX plugin (${NX_PKG}). This package is deprecated and will not be available in future versions."
                    if [ "$SUPPORTS_THIN" != "1" ] ; then
                        # Upgrading is not an option on this host, so don't suggest it.
                        echo "-- WARN: The thin plugin requires Ubuntu 22.04+ (glibc >= 2.34); detected glibc: ${LIBC6_TEXT}."
                    else
                        echo "-- WARN: Consider upgrading to version 2026.1.1+ which uses the thin NX plugin."
                    fi
                fi

                # Unhold both old fat and new thin packages for smooth transition
                UNHOLD_EXTRAS="$UNHOLD_EXTRAS ${INSTALL_NX}-nxplugin ${INSTALL_NX}-nxplugin-thin"

                if [ -n "$VERSION_LOCK" ]; then
                    PACKAGES_TO_HOLD="$PACKAGES_TO_HOLD $NX_PKG"
                    NX_PKG="${NX_PKG}=${VERSION_LOCK}"
                fi

                INSTALL_EXTRAS="$INSTALL_EXTRAS $NX_PKG"
            fi
            
            if [ "$INSTALL_COCKPIT" = "1" ] ; then
                INSTALL_EXTRAS="$INSTALL_EXTRAS cvedia-cockpit"
                UNHOLD_EXTRAS="$UNHOLD_EXTRAS cvedia-cockpit"
            fi

            if [ "$INSTALL_CVEDIA_BOX" = "1" ] ; then
                INSTALL_EXTRAS="$INSTALL_EXTRAS cvedia-box"
                UNHOLD_EXTRAS="$UNHOLD_EXTRAS cvedia-box"
            fi

            # Always unhold CVEDIA packages before installation (in case they were previously held)
            echo "-- Unholding CVEDIA packages (if held)..."
            (
                set -x
                apt-mark unhold cvedia-rt cvedia-rt-nxplugin cvedia-rt-nxplugin-thin $UNHOLD_EXTRAS 2>/dev/null || true
            )

            echo "CVEDIA repository added, installing..."

            #shellcheck disable=SC2086
            (
                set -x
                if ! $APT_CMD update ; then $APT_CMD update --fix-missing; fi
                $APT_CMD install $CVEDIA_PKG $INSTALL_EXTRAS -yf --allow-downgrades --install-recommends $APT_REINSTALL
            )
            RT=$?

            if [ "$RT" -ne "0" ] ; then
                echo "ERROR: Failed to install, aborting!"
                exit 1
            fi

            # Hold packages at locked version to prevent apt upgrade
            if [ -n "$PACKAGES_TO_HOLD" ]; then
                echo "-- Holding packages at version ${VERSION_LOCK} to prevent automatic upgrades..."
                #shellcheck disable=SC2086
                (
                    set -x
                    apt-mark hold $PACKAGES_TO_HOLD
                )
                RT=$?
                if [ "$RT" -eq "0" ]; then
                    echo "-- Packages held: $PACKAGES_TO_HOLD"
                    echo "-- To upgrade in the future, run: sudo apt-mark unhold $PACKAGES_TO_HOLD"
                else
                    echo "WARN: Failed to hold packages, they may be upgraded by 'apt upgrade'"
                fi
            fi
        fi

        # Create update script for easy future updates
        UPDATE_SCRIPT="/usr/local/bin/update-cvedia"
        echo "-- Creating update script at ${UPDATE_SCRIPT}..."

        # Determine the protocol to use in the update script
        UPDATE_URL="https://get.cvedia.com"
        if [ "$PROTO" = "http" ]; then
            # If we're using HTTP, the update script should also use HTTP
            UPDATE_URL="http://get.cvedia.com"
        fi

        cat > "${UPDATE_SCRIPT}" << EOF
#!/bin/bash
# CVEDIA-RT Update Script @ $(date)
# Auto-generated by get.cvedia.com
# Protocol: ${PROTO}

if [[ \$EUID -ne 0 ]]; then
    echo "Please run this script as root or with sudo:"
    echo "sudo \$0"
    exit 1
fi
export THIN_CLIENT=\${THIN_CLIENT-${THIN_CLIENT}}
export INSTALL_NX=\${INSTALL_NX-${INSTALL_NX}}
echo "Updating CVEDIA-RT..."
curl -fsSL ${UPDATE_URL}?${RANDOM} | bash
EOF

        chmod +x "${UPDATE_SCRIPT}"
        if [ -x "${UPDATE_SCRIPT}" ]; then
            echo "-- Update script created successfully"
        else
            echo "WARN: Failed to create update script"
        fi

        if [ "$SKIP_INSTRUCTIONS" = "0" ] ; then
            echo "CVEDIA-RT was installed! You can update it using: sudo update-cvedia"
            echo "- To revert these change delete $APT_LST_FN"

            if [ "$INSTALL_COCKPIT" = "1" ] ; then
                echo "- Cockpit was also installed, you an access it on port 9090"
            fi
            
            if [ "$THIN_CLIENT" = "1" ] ; then
                echo "- CVEDIA-RT is running in thin client mode"
            else
                if [ -n "$INSTALL_NX" ] ; then
                    echo "- NX VMS plugin $INSTALL_NX was also installed"
                else
                    echo "- To install CVEDIA-RT VMS plugin, run: $(basename $APT_CMD) install cvedia-rt-nxplugin-thin"
                fi
            fi

            echo
            echo "Check https://docs.cvedia.com for more information."
        fi

    fi
else
    echo "APT not found, aborting!"
    echo "CVEDIA repositories are compatible with APT-based systems only."
    exit 1
fi

exit 0
}
