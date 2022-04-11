
@rem ================================================================================================================================================================
@rem PARKRUNNING 3840x2160 08 bpp 50 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 28 --cq_step_width_loq_1 32767 --cq_step_width_loq_0  2652 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50.yuv -o Park_avc_qp28_9999_2652_set22_enc  > test_park28_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Park_avc_qp28_9999_2652_set22_enc.lvc -o Park_avc_qp28_9999_2652_set22_dec > test_park28_avc_dec.txt 2>&1
fc /b Park_avc_qp28_9999_2652_set22_enc.yuv Park_avc_qp28_9999_2652_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50.yuv --proc Park_avc_qp28_9999_2652_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p08 --metric PSNR >  __test_park_JM_28

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0  2885 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50.yuv -o Park_avc_qp30_1020_2885_set22_enc  > test_park30_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Park_avc_qp30_1020_2885_set22_enc.lvc -o Park_avc_qp30_1020_2885_set22_dec > test_park30_avc_dec.txt 2>&1
fc /b Park_avc_qp30_1020_2885_set22_enc.yuv Park_avc_qp30_1020_2885_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50.yuv --proc Park_avc_qp30_1020_2885_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p08 --metric PSNR >  __test_park_JM_30

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 32 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50.yuv -o Park_avc_qp32_9999_9999_set22_enc  > test_park32_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Park_avc_qp32_9999_9999_set22_enc.lvc -o Park_avc_qp32_9999_9999_set22_dec > test_park32_avc_dec.txt 2>&1
fc /b Park_avc_qp32_9999_9999_set22_enc.yuv Park_avc_qp32_9999_9999_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50.yuv --proc Park_avc_qp32_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p08 --metric PSNR >  __test_park_JM_32

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 35 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50.yuv -o Park_avc_qp35_9999_9999_set22_enc  > test_park35_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Park_avc_qp35_9999_9999_set22_enc.lvc -o Park_avc_qp35_9999_9999_set22_dec > test_park35_avc_dec.txt 2>&1
fc /b Park_avc_qp35_9999_9999_set22_enc.yuv Park_avc_qp35_9999_9999_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50.yuv --proc Park_avc_qp35_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p08 --metric PSNR >  __test_park_JM_35

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

