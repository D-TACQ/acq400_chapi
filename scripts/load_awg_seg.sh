#!/bin/bash

PATH=./acq400_chapi/:$PATH

# Usage ./send_segs.sh acq1102_010 id32x1-64-193600-*
uut="$1"

shift
if [[ "$*" == *[*?]* ]]; then
  files=($(ls $@ | sort))
else
  files=($@)
fi

export ${ACQ400_LAS_MODE:-ARP}
export ${ACQ400_LAS_SWITCH_SEG:-1000}

if [ ${#files[@]} -gt 0 ]; then
  cmd="acq400_load_awg_seg ${uut}"
  [ -n "${files[0]}" ] && cmd="$cmd A=${files[0]}"
  [ -n "${files[1]}" ] && cmd="$cmd B=${files[1]}"
  [ -n "${files[2]}" ] && cmd="$cmd C=${files[2]}"
  [ -n "${files[3]}" ] && cmd="$cmd D=${files[3]}"
  [ -n "${files[4]}" ] && cmd="$cmd E=${files[4]}"
  echo $cmd
  eval $cmd
fi

