
@rem ================================================================================================================================================================
@rem Twitch_Eurotruck 1920x1080 08 bpp 60 fps JM base (PRED WITHOUT MAP)
@rem ================================================================================================================================================================

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 26 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1065 --temporal_cq_sw_multiplier 500 -i orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv -o Euro_avc_qp26_9999_2130_set22_enc  > test_euro26_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Euro_avc_qp26_9999_2130_set22_enc.lvc -o Euro_avc_qp26_9999_2130_set22_dec > test_euro26_avc_dec.txt 2>&1
fc /b Euro_avc_qp26_9999_2130_set22_enc.yuv Euro_avc_qp26_9999_2130_set22_dec.yuv
QualityMetric --orig orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv --proc Euro_avc_qp26_9999_2130_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_euro_JM_26

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 28 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1325 --temporal_cq_sw_multiplier 500 -i orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv -o Euro_avc_qp28_9999_2650_set22_enc  > test_euro28_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Euro_avc_qp28_9999_2650_set22_enc.lvc -o Euro_avc_qp28_9999_2650_set22_dec > test_euro28_avc_dec.txt 2>&1
fc /b Euro_avc_qp28_9999_2650_set22_enc.yuv Euro_avc_qp28_9999_2650_set22_dec.yuv
QualityMetric --orig orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv --proc Euro_avc_qp28_9999_2650_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_euro_JM_28

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 30 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 1500 --temporal_cq_sw_multiplier 500 -i orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv -o Euro_avc_qp30_9999_3000_set22_enc  > test_euro30_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Euro_avc_qp30_9999_3000_set22_enc.lvc -o Euro_avc_qp30_9999_3000_set22_dec > test_euro30_avc_dec.txt 2>&1
fc /b Euro_avc_qp30_9999_3000_set22_enc.yuv Euro_avc_qp30_9999_3000_set22_dec.yuv
QualityMetric --orig orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv --proc Euro_avc_qp30_9999_3000_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_euro_JM_30

ModelEncoder --parameters config.json -w 1920 -h 1080 -f yuv420p -r 60 --limit 600 --base_encoder baseyuv_avc --qp 32 --cq_step_width_loq_1 32767 --cq_step_width_loq_0 32767 --temporal_cq_sw_multiplier 500 -i orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv -o Euro_avc_qp32_9999_9999_set22_enc  > test_euro32_avc_enc.txt 2>&1
ModelDecoder -w 1920 -h 1080 -f yuv420p -b avc -i Euro_avc_qp32_9999_9999_set22_enc.lvc -o Euro_avc_qp32_9999_9999_set22_dec > test_euro32_avc_dec.txt 2>&1
fc /b Euro_avc_qp32_9999_9999_set22_enc.yuv Euro_avc_qp32_9999_9999_set22_dec.yuv
QualityMetric --orig orig\Twitch_EurotruckSimulator2_1920x1080_60_8bit_420.yuv --proc Euro_avc_qp32_9999_9999_set22_enc.yuv --width 1920 --height 1080 --frames 600 --format yuv420p08 --metric PSNR >  __test_euro_JM_32

@rem ================================================================================================================================================================
@rem ================================================================================================================================================================
