#!/bin/bash

# Usage:
#       ./load_awg_seg.sh acq1102_010 pattern-1.dat pattern-2.dat
#       ./load_awg_seg.sh acq1102_010 pattern-*.dat
#       ACQ400_LAS_SWITCH_SEG=1000 NFILES=2 ./load_awg_seg.sh acq1102_010 pattern-*.dat

uut="$1"
if ! ping -c 1 "$1" &> /dev/null; then
  echo "Unable to connect to '$1'"
  exit 1
fi

shift
files=( "$@" )
NFILES=${NFILES:-26}
LETTERS=(A B C D E F G H I J K L M N O P Q R S T U V W X Y Z)

export ACQ400_LAS_MODE=${ACQ400_LAS_MODE:-ARP}
export ACQ400_LAS_SWITCH_SEG=${ACQ400_LAS_SWITCH_SEG:-1000}

ROOT_DIR=$(dirname $(realpath "$(dirname "$0")"))
cmd="${ROOT_DIR}/acq400_chapi/acq400_load_awg_seg ${uut}"

for file in "${files[@]}"; do
  (( count >= NFILES )) && break
  (( count >= ${#LETTERS[@]} )) && break
  cmd="$cmd ${LETTERS[count]}=$file"
  ((count++))
done

echo $cmd
$cmd

