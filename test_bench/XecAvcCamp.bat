
@rem ================================================================================================================================================================
@rem CampFire 3840x2160 08 bpp 30 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 30 --limit 300 --base_encoder baseyuv_avc --qp 21 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 490 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30.yuv -o Camp_avc_qp21_9999_0980_set22_enc  > test_camp21_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Camp_avc_qp21_9999_0980_set22_enc.lvc -o Camp_avc_qp21_9999_0980_set22_dec > test_camp21_avc_dec.txt 2>&1
fc /b Camp_avc_qp21_9999_0980_set22_enc.yuv Camp_avc_qp21_9999_0980_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30.yuv --proc Camp_avc_qp21_9999_0980_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p08 --metric PSNR >  __test_camp_JM_21

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 30 --limit 300 --base_encoder baseyuv_avc --qp 23 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 740 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30.yuv -o Camp_avc_qp23_9999_1480_set22_enc  > test_camp23_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Camp_avc_qp23_9999_1480_set22_enc.lvc -o Camp_avc_qp23_9999_1480_set22_dec > test_camp23_avc_dec.txt 2>&1
fc /b Camp_avc_qp23_9999_1480_set22_enc.yuv Camp_avc_qp23_9999_1480_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30.yuv --proc Camp_avc_qp23_9999_1480_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p08 --metric PSNR >  __test_camp_JM_23

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 30 --limit 300 --base_encoder baseyuv_avc --qp 27 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1185 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30.yuv -o Camp_avc_qp27_9999_2370_set22_enc  > test_camp27_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Camp_avc_qp27_9999_2370_set22_enc.lvc -o Camp_avc_qp27_9999_2370_set22_dec > test_camp27_avc_dec.txt 2>&1
fc /b Camp_avc_qp27_9999_2370_set22_enc.yuv Camp_avc_qp27_9999_2370_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30.yuv --proc Camp_avc_qp27_9999_2370_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p08 --metric PSNR >  __test_camp_JM_27

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 30 --limit 300 --base_encoder baseyuv_avc --qp 29 --cq_step_width_loq_1 1200 --cq_step_width_loq_0 1320 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30.yuv -o Camp_avc_qp29_1200_2640_set22_enc  > test_camp29_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Camp_avc_qp29_1200_2640_set22_enc.lvc -o Camp_avc_qp29_1200_2640_set22_dec > test_camp29_avc_dec.txt 2>&1
fc /b Camp_avc_qp29_1200_2640_set22_enc.yuv Camp_avc_qp29_1200_2640_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30.yuv --proc Camp_avc_qp29_1200_2640_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p08 --metric PSNR >  __test_camp_JM_29

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

