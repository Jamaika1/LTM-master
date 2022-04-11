#!/bin/bash
#
# Gather bitstreams, parameters and commands
#
set -e

t=$1

mkdir -p ${t}bitstreams

cp test_${t}*/test_*.lvc ${t}bitstreams
cp test_${t}*/test_*recon.md5 ${t}bitstreams
cp test_${t}*/test_*encode.sh ${t}bitstreams
cp test_${t}*/test_*encode.bat ${t}bitstreams
cp test_${t}*/test_*.json ${t}bitstreams
