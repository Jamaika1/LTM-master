#!/bin/bash
#
./stresstest.py --progress=spinner --encoder=../_build/ModelEncoder --decoder=../_build/ModelDecoder --label="Cactus_" --test_file=tests.json --parameters='
{
	"input_file":"Cactus_63frames_1920x1080_50fps_420.yuv",
	"base":"Cactus_63frames_base_avc_qp19_960x540_50fps.bin",
	"base_recon":"Cactus_63frames_base_avc_qp19_960x540_50fps.yuv",
	"format":"yuv420p",
	"base_encoder":"avc"
}
' $*
