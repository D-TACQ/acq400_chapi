#!/bin/bash

# Overview:
# Play waveforms to AO UUT and stream from AI UUT
#
# Setup:
# 0.5Hz trigger into AO UUT
# HDMI Connection from AO UUT to AI UUT
# AO module connected to AI module
#
# Examples:
# AO422
# ./scripts/awg_rtm_stream.sh --ai_uut=acq1102_010 --ao_uut=acq1001_315 --source=ao422_patterns --ai_clk=10K --ao_clk=1M --burstlen=1000000 --translen=10000
# AO424
# ./scripts/awg_rtm_stream.sh --ai_uut=acq1001_070 --ao_uut=acq1001_084 --source=ao424_patterns --ai_clk=10K --ao_clk=500K --burstlen=500000 --translen=10000
#
# TODO: 
#   - handle ai+ao site args

set -e
set -u

PLAY_PID=""
TRIGGER_PID=""

# Argparse
AI_UUT=acq1102_010
AI_CLK=10K
AI_SITE=1
TRANSLEN=10000
MASK='AUTO'

AO_UUT=acq1001_315
AO_CLK=1000000
AO_SITE=1
BURSTLEN=1000000

SOURCE='awg_waveforms'

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
    --ai_uut=DEVICE      AI UUT device
    --ao_uut=DEVICE      AO UUT device
    --translen=N         AI UUT rtm_translen (default: 10000)
    --burstlen=N         AO UUT burstlen (default: 1000000)
    --ai_clk=CLK         AI clock (default: 10K)
    --ai_site=N          AI site (default: 1)
    --ao_clk=CLK         AO clock (default: 1000000)
    --ao_site=N          AO site (default: 1)
    --mask=MASK          Stream subset mask (default: AUTO)
    --source=DIR      Waveforms source directory (default: awg_waveforms)
EOF
}

for arg in "$@"; do
    case $arg in
        # AI args
        --ai_uut=*)
            AI_UUT="${arg#*=}"
            ;;
        --ai_clk=*)
            AI_CLK="${arg#*=}"
            ;;
        --ai_site=*)
            AI_SITE="${arg#*=}"
            ;;
        --translen=*)
            TRANSLEN="${arg#*=}"
            ;;
        --mask=*)
            MASK="${arg#*=}"
            ;;
        # AO args
        --ao_uut=*)
            AO_UUT="${arg#*=}"
            ;;
        --ao_clk=*)
            AO_CLK="${arg#*=}"
            ;;
        --ao_site=*)
            AO_SITE="${arg#*=}"
            ;;
        --burstlen=*)
            BURSTLEN="${arg#*=}"
            ;;
        --source=*)
            SOURCE="${arg#*=}"
            ;;
        # Misc
        *)
            usage
            exit 1
            ;;
    esac
done

# Functions

cleanup() {
    # Kills subprocess at end
    if [ -n "$PLAY_PID" ]; then
        kill $PLAY_PID 2>/dev/null
        wait $PLAY_PID 2>/dev/null
    fi
    if [ -n "$TRIGGER_PID" ]; then
        kill $TRIGGER_PID 2>/dev/null
        wait $TRIGGER_PID 2>/dev/null
    fi
}


wait_for_ready() {
    # Wait for UUTs to be ready
    $AO_UUT=$1
    $AI_UUT=$2
    ./acq400_chapi/acq400_wait_for_ready $AO_UUT
    ./acq400_chapi/acq400_wait_for_ready $AI_UUT
}

play_waveforms() {
    # Continuously streams waveforms to AO
    UUT=$1
    DIR=$2
    if [ -z "$(ls -A $DIR 2>/dev/null)" ]; then
        echo "ERROR: waveforms not found ($(pwd)/${DIR}/) " >&2
        exit 1
    fi
    echo "RUNNING CMD ./acq400_chapi/acq400_stream_awg $UUT repeat $DIR/*"
    (   
        ./acq400_chapi/acq400_stream_awg $UUT repeat $DIR/*
    ) & PLAY_PID=$!
    echo "Playing waveforms from ${DIR} on ${UUT} (PID: $PLAY_PID)"
}

set_trigger_in() {
    # sets trigger d0 source value
    UUT=$1
    VALUE=$2
    echo "Trigger d0 = ${VALUE}"
    send_cmd $UUT 0 "SIG:SRC:TRG:0=${VALUE}"
}

prime_trigger() {
    # Enable AO UUT trigger input on AI UUT arm
    AI_UUT=$1
    AO_UUT=$2
    (
        echo "Waiting for ${AI_UUT} to reach ARM state..."
        wait_for_state $AI_UUT "ARM"
        echo "Enable trigger IN"
        set_trigger_in $AO_UUT 'EXT'
    ) & TRIGGER_PID=$!
    echo "Trigger waiting (PID: $TRIGGER_PID)"
}

abort_uut() {
    # Abort AI UUT
    UUT=$1
    echo "Abort ${UUT}"
    send_cmd $UUT 0 "set_abort=1"
}

wait_for_state() {
    # Wait for UUT to reach state
    UUT=$1
    TARGET_STATE=$2
    
    while true; do
        STATE=$(echo "CONTINUOUS:STATE" | nc -N $UUT 4220)
        if echo "$STATE" | grep -q "$TARGET_STATE"; then
            echo "${UUT} reached ${TARGET_STATE}"
            break
        fi
        sleep 1
    done
}

send_cmd() {
    # Send command to UUT
    UUT=$1
    SITE_NUM=$2
    CMD=$3
    PORT=$((4220 + $SITE_NUM))
    SUPPRESS=${4:-false}
    if [ "$SUPPRESS" = "true" ]; then
        echo "$CMD" | nc -N $UUT $PORT > /dev/null 2>&1
    else
        echo "$CMD" | nc -N $UUT $PORT
    fi
}

config_ai() {
    # Configure AI UUT for capture
    UUT=$1
    CLK=$2
    TRANSLEN=$3
    MASK=$4
    echo "Config AI UUT"
    send_cmd $UUT 0 "sync_role master ${CLK} TRG=int" true
    send_cmd $UUT 0 "SIG:SRC:TRG:0=HDMI"
    send_cmd $UUT 1 "RTM_TRANSLEN=${TRANSLEN}"
    send_cmd $UUT 0 "stream_subset_mask=${MASK}"
    send_cmd $UUT 1 "rgm=3,0,1"
    send_cmd $UUT 1 "RTM_TRANSLEN"
}

config_ao() {
    UUT=$1
    CLK=$2
    BURSTLEN=$3
    echo "Config AO UUT"
    send_cmd $UUT 0 "sync_role master ${CLK}" true
    send_cmd $UUT 1 "rtm_translen=${BURSTLEN}"
    send_cmd $UUT 1 "trg=1,0,1"
    send_cmd $UUT 1 "awg_rtm=3"
}

calculate_mask() {
    if [ "$MASK" = "AUTO" ]; then
        AO_CHANS=$(echo "NCHAN" | nc -N $AO_UUT 4221 | tr -d '\n\r ')
        VALUE=$(( 2**$AO_CHANS - 1 ))
        MASK=$(printf "0x%x" $VALUE)
        echo "Mask auto set to ${MASK}"
    fi
}



# Starts here

trap cleanup EXIT INT TERM

wait_for_ready $AO_UUT $AI_UUT
set_trigger_in $AO_UUT 'NONE'
abort_uut $AI_UUT
wait_for_state $AI_UUT "IDLE"
calculate_mask
config_ai $AI_UUT $AI_CLK $TRANSLEN $MASK
config_ao $AO_UUT $AO_CLK $BURSTLEN
prime_trigger $AI_UUT $AO_UUT
play_waveforms $AO_UUT $SOURCE

DATFILE="awg_rtm_stream.${AO_UUT}.dat"
echo "RUNNING CMD./acq400_chapi/acq400_stream $AI_UUT 4210 | pv > $DATFILE"
./acq400_chapi/acq400_stream $AI_UUT 4210 | pv > $DATFILE
