
@rem ================================================================================================================================================================
@rem PARKRUNNING 3840x2160 10 bpp 50 fps HM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 50 --limit 500 --base_encoder baseyuv_hevc --qp 27 --cq_step_width_loq_1  32767 --cq_step_width_loq_0  2450 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv -o Park_hevc_qp27_9999_2450_set22_enc  > test_park27_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Park_hevc_qp27_9999_2450_set22_enc.lvc -o Park_hevc_qp27_9999_2450_set22_dec > test_park27_hevc_dec.txt 2>&1
fc /b Park_hevc_qp27_9999_2450_set22_enc.yuv Park_hevc_qp27_9999_2450_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv --proc Park_hevc_qp27_9999_2450_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p10 --metric PSNR >  __test_park_HM_27

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 50 --limit 500 --base_encoder baseyuv_hevc --qp 29 --cq_step_width_loq_1  32767 --cq_step_width_loq_0  2650 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv -o Park_hevc_qp29_9999_2650_set22_enc  > test_park29_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Park_hevc_qp29_9999_2650_set22_enc.lvc -o Park_hevc_qp29_9999_2650_set22_dec > test_park29_hevc_dec.txt 2>&1
fc /b Park_hevc_qp29_9999_2650_set22_enc.yuv Park_hevc_qp29_9999_2650_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv --proc Park_hevc_qp29_9999_2650_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p10 --metric PSNR >  __test_park_HM_29

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 50 --limit 500 --base_encoder baseyuv_hevc --qp 31 --cq_step_width_loq_1  32767 --cq_step_width_loq_0 1435 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv -o Park_hevc_qp31_9999_2870_set22_enc  > test_park31_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Park_hevc_qp31_9999_2870_set22_enc.lvc -o Park_hevc_qp31_9999_2870_set22_dec > test_park31_hevc_dec.txt 2>&1
fc /b Park_hevc_qp31_9999_2870_set22_enc.yuv Park_hevc_qp31_9999_2870_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv --proc Park_hevc_qp31_9999_2870_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p10 --metric PSNR >  __test_park_HM_31

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 50 --limit 500 --base_encoder baseyuv_hevc --qp 33 --cq_step_width_loq_1  32767 --cq_step_width_loq_0 1750 --temporal_cq_sw_multiplier 500 -i orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv -o Park_hevc_qp33_9999_3500_set22_enc  > test_park33_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Park_hevc_qp33_9999_3500_set22_enc.lvc -o Park_hevc_qp33_9999_3500_set22_dec > test_park33_hevc_dec.txt 2>&1
fc /b Park_hevc_qp33_9999_3500_set22_enc.yuv Park_hevc_qp33_9999_3500_set22_dec.yuv
QualityMetric --orig orig\ParkRunning3_3840x2160_50fps_10bit_420.yuv --proc Park_hevc_qp33_9999_3500_set22_enc.yuv --width 3840 --height 2160 --frames 500 --format yuv420p10 --metric PSNR >  __test_park_HM_33


@rem ================================================================================================================================================================
@rem ================================================================================================================================================================
