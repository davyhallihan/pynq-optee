#!/bin/bash
#  Copyright (C)2025 Zakaria Madaoui
XSCT=$1

if [ "$1" == "" ]; then 
    echo "Usage ./generate_dts <path_to_xsct_tool>"
    echo "WARNING: Defaulting to use xsct without absolute path"
    XSCT=xsct
fi

git clone --branch master https://github.com/Xilinx/device-tree-xlnx.git /tmp/device-tree-xlnx
$XSCT generate_dts.tcl
