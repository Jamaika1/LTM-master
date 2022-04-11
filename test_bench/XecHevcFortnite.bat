
@rem ================================================================================================================================================================
@rem Fortnite (Part1) 3840x2160 10 bpp 60 fps HM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 27 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1326 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv -o Fort_hevc_qp27_9999_2652_set22_enc  > test_fort27_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Fort_hevc_qp27_9999_2652_set22_enc.lvc -o Fort_hevc_qp27_9999_2652_set22_dec > test_fort27_hevc_dec.txt 2>&1
fc /b Fort_hevc_qp27_9999_2652_set22_enc.yuv Fort_hevc_qp27_9999_2652_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv --proc Fort_hevc_qp27_9999_2652_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_fort_HM_27

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1888 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv -o Fort_hevc_qp30_9999_3777_set22_enc  > test_fort30_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Fort_hevc_qp30_9999_3777_set22_enc.lvc -o Fort_hevc_qp30_9999_3777_set22_dec > test_fort30_hevc_dec.txt 2>&1
fc /b Fort_hevc_qp30_9999_3777_set22_enc.yuv Fort_hevc_qp30_9999_3777_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv --proc Fort_hevc_qp30_9999_3777_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_fort_HM_30

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 32 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv -o Fort_hevc_qp32_9999_9999_set22_enc  > test_fort32_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Fort_hevc_qp32_9999_9999_set22_enc.lvc -o Fort_hevc_qp32_9999_9999_set22_dec > test_fort32_hevc_dec.txt 2>&1
fc /b Fort_hevc_qp32_9999_9999_set22_enc.yuv Fort_hevc_qp32_9999_9999_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv --proc Fort_hevc_qp32_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_fort_HM_32

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 34 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv -o Fort_hevc_qp34_9999_9999_set22_enc  > test_fort34_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Fort_hevc_qp34_9999_9999_set22_enc.lvc -o Fort_hevc_qp34_9999_9999_set22_dec > test_fort34_hevc_dec.txt 2>&1
fc /b Fort_hevc_qp34_9999_9999_set22_enc.yuv Fort_hevc_qp34_9999_9999_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160_60fps_10bit_420.yuv --proc Fort_hevc_qp34_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_fort_HM_34

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

