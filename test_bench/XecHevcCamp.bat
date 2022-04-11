
@rem ================================================================================================================================================================
@rem CampFire 3840x2160 10 bpp 30 fps HM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 30 --limit 300 --base_encoder baseyuv_hevc --qp 19 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 361 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30fps_10bit_420.yuv -o Camp_hevc_qp19_9999_0723_set22_enc  > test_camp19_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Camp_hevc_qp19_9999_0723_set22_enc.lvc -o Camp_hevc_qp19_9999_0723_set22_dec > test_camp19_hevc_dec.txt 2>&1
fc /b Camp_hevc_qp19_9999_0723_set22_enc.yuv Camp_hevc_qp19_9999_0723_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30fps_10bit_420.yuv --proc Camp_hevc_qp19_9999_0723_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p10 --metric PSNR >  __test_camp_HM_19

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 30 --limit 300 --base_encoder baseyuv_hevc --qp 21 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 553 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30fps_10bit_420.yuv -o Camp_hevc_qp21_9999_1107_set22_enc  > test_camp21_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Camp_hevc_qp21_9999_1107_set22_enc.lvc -o Camp_hevc_qp21_9999_1107_set22_dec > test_camp21_hevc_dec.txt 2>&1
fc /b Camp_hevc_qp21_9999_1107_set22_enc.yuv Camp_hevc_qp21_9999_1107_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30fps_10bit_420.yuv --proc Camp_hevc_qp21_9999_1107_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p10 --metric PSNR >  __test_camp_HM_21

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 30 --limit 300 --base_encoder baseyuv_hevc --qp 24 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 863 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30fps_10bit_420.yuv -o Camp_hevc_qp24_9999_1726_set22_enc  > test_camp24_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Camp_hevc_qp24_9999_1726_set22_enc.lvc -o Camp_hevc_qp24_9999_1726_set22_dec > test_camp24_hevc_dec.txt 2>&1
fc /b Camp_hevc_qp24_9999_1726_set22_enc.yuv Camp_hevc_qp24_9999_1726_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30fps_10bit_420.yuv --proc Camp_hevc_qp24_9999_1726_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p10 --metric PSNR >  __test_camp_HM_24

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 30 --limit 300 --base_encoder baseyuv_hevc --qp 28 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1100 --temporal_cq_sw_multiplier 500 -i orig\Campfire_3840x2160_30fps_10bit_420.yuv -o Camp_hevc_qp28_9999_2200_set22_enc  > test_camp28_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Camp_hevc_qp28_9999_2200_set22_enc.lvc -o Camp_hevc_qp28_9999_2200_set22_dec > test_camp28_hevc_dec.txt 2>&1
fc /b Camp_hevc_qp28_9999_2200_set22_enc.yuv Camp_hevc_qp28_9999_2200_set22_dec.yuv
QualityMetric --orig orig\Campfire_3840x2160_30fps_10bit_420.yuv --proc Camp_hevc_qp28_9999_2200_set22_enc.yuv --width 3840 --height 2160 --frames 300 --format yuv420p10 --metric PSNR >  __test_camp_HM_28

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

