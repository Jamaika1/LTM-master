#!/bin/bash
#
./stresstest.py --progress=spinner --label="CanYouReadThis_" --test_file=tests.json --parameters='
{
	"input_file":"CanYouReadThis_63frames_1920x1080_420.yuv",
	"base":"CanYouReadThis_63frame_base_avc_qp19_960x540.bin",
	"base_recon":"CanYouReadThis_63frame_base_avc_qp19_960x540.yuv",
	"format":"yuv420p",
	"base_encoder":"avc"
}
' $*
