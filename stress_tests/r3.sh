#!/bin/bash
#
./stresstest.py  --progress=spinner --label="ParkRunning3_" --test_file=tests.json --parameters='
{
  "input_file":"ParkRunning3_63frames_3840x2160_50fps_10bit_420.yuv",
  "base":"ParkRunning3_63frames_base_hevc_qp19_1920x1080.bin",
  "base_recon":"ParkRunning3_63frames_base_hevc_qp19_1920x1080.yuv",
  "format":"yuv420p10",
  "base_encoder":"hevc"
}
' $*
