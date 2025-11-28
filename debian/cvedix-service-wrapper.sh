#!/bin/bash
#
# CVEDIX AI Runtime Service Wrapper
# This script wraps the CVEDIX AI Runtime sample applications to run as a service
#

set -e

# Configuration file location
CONFIG_FILE="/etc/cvedix/service.conf"
DEFAULT_SAMPLE="1-1-1_sample"
DEFAULT_CONFIG_DIR="/opt/cvedix/config"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

# Load configuration if exists
if [ -f "$CONFIG_FILE" ]; then
    log "Loading configuration from $CONFIG_FILE"
    source "$CONFIG_FILE"
fi

# Get sample name from config or use default
SAMPLE_NAME="${CVEDIX_SAMPLE:-$DEFAULT_SAMPLE}"
SAMPLE_BIN="/usr/bin/${SAMPLE_NAME}"

# Check if sample binary exists
if [ ! -f "$SAMPLE_BIN" ]; then
    log "ERROR: Sample binary not found: $SAMPLE_BIN"
    log "Available samples:"
    ls -1 /usr/bin/*_sample 2>/dev/null | xargs -n1 basename || true
    exit 1
fi

# Change to working directory
cd "${CVEDIX_WORK_DIR:-/opt/cvedix}" || {
    log "ERROR: Cannot change to working directory: ${CVEDIX_WORK_DIR:-/opt/cvedix}"
    exit 1
}

# Check for cvedix_data directory
if [ ! -d "cvedix_data" ]; then
    log "WARNING: cvedix_data directory not found in working directory"
    log "Working directory: $(pwd)"
    log "Please ensure cvedix_data is available for the service to run properly"
fi

# Set up environment
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/usr/lib:/opt/cvedix/lib"
export GST_PLUGIN_PATH="${GST_PLUGIN_PATH}:/usr/lib/x86_64-linux-gnu/gstreamer-1.0:/usr/lib/aarch64-linux-gnu/gstreamer-1.0"

# Log startup
log "Starting CVEDIX AI Runtime Service"
log "Sample: $SAMPLE_NAME"
log "Working directory: $(pwd)"
log "Binary: $SAMPLE_BIN"

# Run the sample
exec "$SAMPLE_BIN" "$@"

