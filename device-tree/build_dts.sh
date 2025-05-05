#!/bin/bash
#  Copyright (C)2025 Zakaria Madaoui

DTC=$1

if [ "$1" == "" ]; then 
    echo "Usage ./build_dts.sh <path_to_dtc_tool>"
    echo "WARNING: Defaulting to use dtc without absolute path"
    DTC=dtc
fi

# Preprocessing
echo "Preprocessing ...."
gcc -I ./dts -E -nostdinc -I include -undef -D__DTS__ -x assembler-with-cpp -o dts/system.dts dts/system-top.dts

echo "Compiling DTS ...."
$DTC -I dts -O dtb -o ../artifacts/system.dtb dts/system.dts

if [ $? -eq 0 ]; then 
    echo "DTB compiled and stored at $(realpath ../artifacts/system.dtb )"
fi 