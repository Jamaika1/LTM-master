
@rem ================================================================================================================================================================
@rem FoodMarket4 3840x2160 08 bpp 60 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 24 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 993 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60_600.yuv -o Food_avc_qp24_9999_1986_set22_enc  > test_food24_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Food_avc_qp24_9999_1986_set22_enc.lvc -o Food_avc_qp24_9999_1986_set22_dec > test_food24_avc_dec.txt 2>&1
fc /b Food_avc_qp24_9999_1986_set22_enc.yuv Food_avc_qp24_9999_1986_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60_600.yuv --proc Food_avc_qp24_9999_1986_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_food_JM_24

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 28 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1442 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60_600.yuv -o Food_avc_qp28_9999_2885_set22_enc  > test_food28_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Food_avc_qp28_9999_2885_set22_enc.lvc -o Food_avc_qp28_9999_2885_set22_dec > test_food28_avc_dec.txt 2>&1
fc /b Food_avc_qp28_9999_2885_set22_enc.yuv Food_avc_qp28_9999_2885_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60_600.yuv --proc Food_avc_qp28_9999_2885_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_food_JM_28

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1635 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60_600.yuv -o Food_avc_qp30_9999_3270_set22_enc  > test_food30_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Food_avc_qp30_9999_3270_set22_enc.lvc -o Food_avc_qp30_9999_3270_set22_dec > test_food30_avc_dec.txt 2>&1
fc /b Food_avc_qp30_9999_3270_set22_enc.yuv Food_avc_qp30_9999_3270_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60_600.yuv --proc Food_avc_qp30_9999_3270_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_food_JM_30

ModelEncoder --parameters config.json -w 3840 -h 2160 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 32 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\FoodMarket4_3840x2160_60_600.yuv -o Food_avc_qp32_9999_9999_set22_enc  > test_food32_avc_enc.txt 2>&1
ModelDecoder -w 3840 -h 2160 -f yuv420p -b avc -i Food_avc_qp32_9999_9999_set22_enc.lvc -o Food_avc_qp32_9999_9999_set22_dec > test_food32_avc_dec.txt 2>&1
fc /b Food_avc_qp32_9999_9999_set22_enc.yuv Food_avc_qp32_9999_9999_set22_dec.yuv
QualityMetric --orig orig\FoodMarket4_3840x2160_60_600.yuv --proc Food_avc_qp32_9999_9999_set22_enc.yuv --width 3840 --height 2160 --frames 600 --format yuv420p08 --metric PSNR >  __test_food_JM_32

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

