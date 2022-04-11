
@rem ================================================================================================================================================================
@rem Cactus 1920x1080 08 bpp 50 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 24 --cq_step_width_loq_1 32767 --cq_step_width_loq_0  2410 --temporal_cq_sw_multiplier 500 -i orig\Cactus_1920x1080_50.yuv -o Cactus_avc_qp24_9999_2410_set22_enc  > test_cactus24_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Cactus_avc_qp24_9999_2410_set22_enc.lvc -o Cactus_avc_qp24_9999_2410_set22_dec > test_cactus24_avc_dec.txt 2>&1
fc /b Cactus_avc_qp24_9999_2410_set22_enc.yuv Cactus_avc_qp24_9999_2410_set22_dec.yuv
QualityMetric --orig orig\Cactus_1920x1080_50.yuv --proc Cactus_avc_qp24_9999_2410_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_cactus_JM_24

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 26 --cq_step_width_loq_1  32767 --cq_step_width_loq_0  3160 --temporal_cq_sw_multiplier 500 -i orig\Cactus_1920x1080_50.yuv -o Cactus_avc_qp26_9999_3160_set22_enc  > test_cactus26_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Cactus_avc_qp26_9999_3160_set22_enc.lvc -o Cactus_avc_qp26_9999_3160_set22_dec > test_cactus26_avc_dec.txt 2>&1
fc /b Cactus_avc_qp26_9999_3160_set22_enc.yuv Cactus_avc_qp26_9999_3160_set22_dec.yuv
QualityMetric --orig orig\Cactus_1920x1080_50.yuv --proc Cactus_avc_qp26_9999_3160_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_cactus_JM_26

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 29 --cq_step_width_loq_1  32767 --cq_step_width_loq_0 1888 --temporal_cq_sw_multiplier 500 -i orig\Cactus_1920x1080_50.yuv -o Cactus_avc_qp29_9999_3777_set22_enc  > test_cactus29_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Cactus_avc_qp29_9999_3777_set22_enc.lvc -o Cactus_avc_qp29_9999_3777_set22_dec > test_cactus29_avc_dec.txt 2>&1
fc /b Cactus_avc_qp29_9999_3777_set22_enc.yuv Cactus_avc_qp29_9999_3777_set22_dec.yuv
QualityMetric --orig orig\Cactus_1920x1080_50.yuv --proc Cactus_avc_qp29_9999_3777_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_cactus_JM_32

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 33 --cq_step_width_loq_1  32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\Cactus_1920x1080_50.yuv -o Cactus_avc_qp33_9999_9999_set22_enc  > test_cactus33_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Cactus_avc_qp33_9999_9999_set22_enc.lvc -o Cactus_avc_qp33_9999_9999_set22_dec > test_cactus33_avc_dec.txt 2>&1
fc /b Cactus_avc_qp33_9999_9999_set22_enc.yuv Cactus_avc_qp33_9999_9999_set22_dec.yuv
QualityMetric --orig orig\Cactus_1920x1080_50.yuv --proc Cactus_avc_qp33_9999_9999_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_cactus_JM_33

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================
