
@rem ================================================================================================================================================================
@rem Fortnite (Part1) 3840x2160 08 bpp 60 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 26 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1254 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160p60.yuv -o Fort_avc_qp26_9999_2508_set22_enc  > test_fort26_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Fort_avc_qp26_9999_2508_set22_enc.lvc -o Fort_avc_qp26_9999_2508_set22_dec > test_fort26_avc_dec.txt 2>&1
fc /b Fort_avc_qp26_9999_2508_set22_enc.yuv Fort_avc_qp26_9999_2508_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160p60.yuv --proc Fort_avc_qp26_9999_2508_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_fort_JM_26

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 29 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1382 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160p60.yuv -o Fort_avc_qp29_9999_2765_set22_enc  > test_fort29_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Fort_avc_qp29_9999_2765_set22_enc.lvc -o Fort_avc_qp29_9999_2765_set22_dec > test_fort29_avc_dec.txt 2>&1
fc /b Fort_avc_qp29_9999_2765_set22_enc.yuv Fort_avc_qp29_9999_2765_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160p60.yuv --proc Fort_avc_qp29_9999_2765_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_fort_JM_29

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 32 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1879 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160p60.yuv -o Fort_avc_qp32_9999_3758_set22_enc  > test_fort32_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Fort_avc_qp32_9999_3758_set22_enc.lvc -o Fort_avc_qp32_9999_3758_set22_dec > test_fort32_avc_dec.txt 2>&1
fc /b Fort_avc_qp32_9999_3758_set22_enc.yuv Fort_avc_qp32_9999_3758_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160p60.yuv --proc Fort_avc_qp32_9999_3758_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_fort_JM_32

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 34 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\Fortnite_Part1_3840x2160p60.yuv -o Fort_avc_qp34_9999_9999_set22_enc  > test_fort34_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Fort_avc_qp34_9999_9999_set22_enc.lvc -o Fort_avc_qp34_9999_9999_set22_dec > test_fort34_avc_dec.txt 2>&1
fc /b Fort_avc_qp34_9999_9999_set22_enc.yuv Fort_avc_qp34_9999_9999_set22_dec.yuv
QualityMetric --orig orig\Fortnite_Part1_3840x2160p60.yuv --proc Fort_avc_qp34_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_fort_JM_34

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

