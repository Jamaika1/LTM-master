#!/bin/bash
echo "-- Symbols"
for i in symbols-P0Q0L* ; do printf "%32s : " $i ; dd if=$i bs=64800 count=1 status=none | md5sum ; done
echo ""
for i in symbols-P0Q1L* ; do printf "%32s : " $i ; dd if=$i bs=259200 count=1 status=none | md5sum ; done
echo "-- Coeffs"
for i in coeffs-P0Q0L* ; do printf "%32s : " $i ; dd if=$i bs=64800 count=1 status=none | md5sum ; done
echo ""
for i in coeffs-P0Q1L* ; do printf "%32s : " $i ; dd if=$i bs=259200 count=1 status=none | md5sum ; done
echo "-- Residuals"
for i in residuals-P0Q0L* ; do printf "%32s : " $i ; dd if=$i bs=1036800 count=1 status=none | md5sum ; done
echo ""
for i in residuals-P0Q1L* ; do printf "%32s : " $i ; dd if=$i bs=4147200 count=1 status=none | md5sum ; done
echo "-- Output"
for i in output_1920x1080_p420.yuv ; do printf "%32s : " $i ; dd if=$i bs=3110400 count=1 status=none | md5sum ; done
