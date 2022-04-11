#!/bin/bash
#
ENCODER=./ModelEncoder

# The subimissions data
MEDIA=/home/sam/work/medusa/bases

${ENCODER} \
	--width=3840 --height=2160 --fps=30 --format=yuv420p10 \
	--qp=26 \
	--parameters=tests/config_TM.json \
	--encapsulation=nal \
	--base_encoder=evc \
	--cq_step_width_loq_1=32767 \
	--cq_step_width_loq_0=10000 \
	--limit=10 \
	--dump_surfaces=true \
	\
	--input_file=${MEDIA}/EVC/Robo_3840x2160_30fps_10bit.yuv \
	--base=${MEDIA}/EVC/Robo_1920x1080_30fps_10bit_qp30.evc \
	--base_recon=${MEDIA}/EVC/Robo_1920x1080_30fps_10bit_qp30.yuv \
	--output_recon=Robo_recon_3840x2160_10bit.yuv \
	--output_file=Robo_3840x2160.lvc
