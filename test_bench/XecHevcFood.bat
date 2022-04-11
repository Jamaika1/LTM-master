
@rem ================================================================================================================================================================
@rem FoodMarket4 3840x2160 10 bpp 60 fps HM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 23 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 935 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv -o Food_hevc_qp23_9999_1870_set22_enc  > test_food23_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Food_hevc_qp23_9999_1870_set22_enc.lvc -o Food_hevc_qp23_9999_1870_set22_dec > test_food23_hevc_dec.txt 2>&1
fc /b Food_hevc_qp23_9999_1870_set22_enc.yuv Food_hevc_qp23_9999_1870_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv --proc Food_hevc_qp23_9999_1870_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_food_HM_23

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 26 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1134 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv -o Food_hevc_qp26_9999_2268_set22_enc  > test_food26_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Food_hevc_qp26_9999_2268_set22_enc.lvc -o Food_hevc_qp26_9999_2268_set22_dec > test_food26_hevc_dec.txt 2>&1
fc /b Food_hevc_qp26_9999_2268_set22_enc.yuv Food_hevc_qp26_9999_2268_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv --proc Food_hevc_qp26_9999_2268_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_food_HM_26

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 28 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1287 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv -o Food_hevc_qp28_9999_2574_set22_enc  > test_food28_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Food_hevc_qp28_9999_2574_set22_enc.lvc -o Food_hevc_qp28_9999_2574_set22_dec > test_food28_hevc_dec.txt 2>&1
fc /b Food_hevc_qp28_9999_2574_set22_enc.yuv Food_hevc_qp28_9999_2574_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv --proc Food_hevc_qp28_9999_2574_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_food_HM_28

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p10 -r 60 --limit 600 --base_encoder baseyuv_hevc --qp 34 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv -o Food_hevc_qp34_9999_9999_set22_enc  > test_food34_hevc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p10 -b hevc -i Food_hevc_qp34_9999_9999_set22_enc.lvc -o Food_hevc_qp34_9999_9999_set22_dec > test_food34_hevc_dec.txt 2>&1
fc /b Food_hevc_qp34_9999_9999_set22_enc.yuv Food_hevc_qp34_9999_9999_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60fps_10bit_420_600.yuv --proc Food_hevc_qp34_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p10 --metric PSNR >  __test_food_HM_34

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

