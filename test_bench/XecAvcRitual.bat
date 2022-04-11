
@rem ================================================================================================================================================================
@rem RitualDance 1920x1080 08 bpp 50 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 21 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 765 --temporal_cq_sw_multiplier 500 -i orig\RitualDance_1920x1080_60.yuv -o Ritu_avc_qp21_9999_1530_set22_enc  > test_ritu21_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Ritu_avc_qp21_9999_1530_set22_enc.lvc -o Ritu_avc_qp21_9999_1530_set22_dec > test_ritu21_avc_dec.txt 2>&1
fc /b Ritu_avc_qp21_9999_1530_set22_enc.yuv Ritu_avc_qp21_9999_1530_set22_dec.yuv
QualityMetric --orig orig\RitualDance_1920x1080_60.yuv --proc Ritu_avc_qp21_9999_1530_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_ritu_JM_21

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 24 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1014 --temporal_cq_sw_multiplier 500 -i orig\RitualDance_1920x1080_60.yuv -o Ritu_avc_qp24_9999_2029_set22_enc  > test_ritu24_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Ritu_avc_qp24_9999_2029_set22_enc.lvc -o Ritu_avc_qp24_9999_2029_set22_dec > test_ritu24_avc_dec.txt 2>&1
fc /b Ritu_avc_qp24_9999_2029_set22_enc.yuv Ritu_avc_qp24_9999_2029_set22_dec.yuv
QualityMetric --orig orig\RitualDance_1920x1080_60.yuv --proc Ritu_avc_qp24_9999_2029_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_ritu_JM_24

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 27 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1236 --temporal_cq_sw_multiplier 500 -i orig\RitualDance_1920x1080_60.yuv -o Ritu_avc_qp27_9999_2473_set22_enc  > test_ritu27_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Ritu_avc_qp27_9999_2473_set22_enc.lvc -o Ritu_avc_qp27_9999_2473_set22_dec > test_ritu27_avc_dec.txt 2>&1
fc /b Ritu_avc_qp27_9999_2473_set22_enc.yuv Ritu_avc_qp27_9999_2473_set22_dec.yuv
QualityMetric --orig orig\RitualDance_1920x1080_60.yuv --proc Ritu_avc_qp27_9999_2473_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_ritu_JM_27

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1437 --temporal_cq_sw_multiplier 500 -i orig\RitualDance_1920x1080_60.yuv -o Ritu_avc_qp30_9999_2875_set22_enc  > test_ritu30_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Ritu_avc_qp30_9999_2875_set22_enc.lvc -o Ritu_avc_qp30_9999_2875_set22_dec > test_ritu30_avc_dec.txt 2>&1
fc /b Ritu_avc_qp30_9999_2875_set22_enc.yuv Ritu_avc_qp30_9999_2875_set22_dec.yuv
QualityMetric --orig orig\RitualDance_1920x1080_60.yuv --proc Ritu_avc_qp30_9999_2875_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_ritu_JM_30

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

