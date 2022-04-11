
@rem ================================================================================================================================================================
@rem BasketballDrive 1920x1080 08 bpp 50 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 22 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1326 --temporal_cq_sw_multiplier 500 -i orig\BasketballDrive_1920x1080_50_499.yuv -o Bask_avc_qp22_9999_2652_set22_enc  > test_bask22_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Bask_avc_qp22_9999_2652_set22_enc.lvc -o Bask_avc_qp22_9999_2652_set22_dec > test_bask22_avc_dec.txt 2>&1 fc /b Bask_avc_qp22_9999_2652_set22_enc.yuv Bask_avc_qp22_9999_2652_set22_dec.yuv
QualityMetric --orig orig\BasketballDrive_1920x1080_50_499.yuv --proc Bask_avc_qp22_9999_2652_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_bask_JM_22

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 23 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1386 --temporal_cq_sw_multiplier 500 -i orig\BasketballDrive_1920x1080_50_499.yuv -o Bask_avc_qp23_9999_2772_set22_enc  > test_bask23_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Bask_avc_qp23_9999_2772_set22_enc.lvc -o Bask_avc_qp23_9999_2772_set22_dec > test_bask23_avc_dec.txt 2>&1
fc /b Bask_avc_qp23_9999_2772_set22_enc.yuv Bask_avc_qp23_9999_2772_set22_dec.yuv
QualityMetric --orig orig\BasketballDrive_1920x1080_50_499.yuv --proc Bask_avc_qp23_9999_2772_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_bask_JM_23

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 26 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1888 --temporal_cq_sw_multiplier 500 -i orig\BasketballDrive_1920x1080_50_499.yuv -o Bask_avc_qp26_9999_3777_set22_enc  > test_bask26_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Bask_avc_qp26_9999_3777_set22_enc.lvc -o Bask_avc_qp26_9999_3777_set22_dec > test_bask26_avc_dec.txt 2>&1
fc /b Bask_avc_qp26_9999_3777_set22_enc.yuv Bask_avc_qp26_9999_3777_set22_dec.yuv
QualityMetric --orig orig\BasketballDrive_1920x1080_50_499.yuv --proc Bask_avc_qp26_9999_3777_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_bask_JM_26

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 50 --limit 500 --base_encoder baseyuv_avc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\BasketballDrive_1920x1080_50_499.yuv -o Bask_avc_qp30_9999_9999_set22_enc  > test_bask30_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Bask_avc_qp30_9999_9999_set22_enc.lvc -o Bask_avc_qp30_9999_9999_set22_dec > test_bask30_avc_dec.txt 2>&1
fc /b Bask_avc_qp30_9999_9999_set22_enc.yuv Bask_avc_qp30_9999_9999_set22_dec.yuv
QualityMetric --orig orig\BasketballDrive_1920x1080_50_499.yuv --proc Bask_avc_qp30_9999_9999_set22_enc.yuv --width 1920 --height 1080 --frames 500 --format yuv420p08 --metric PSNR >  __test_bask_JM_30

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================

