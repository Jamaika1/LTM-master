# LCEVC Conformance Test - Cross Checking Tools

This instruction gives a brief overview on how to use the provided (and recommended) tools to perform the cross-checking of the LCEVC conformance test bitstreams.


## Test Scripts

The *cross_checking_tools* folder on the MPEG FTP server contains the following content:
* stresstest.py - The python script to perform the tests with
* tests.json - A json file which contains the settings for the individual tests
* main_profile.json - A json file in which parameters are set which are shared between all tests in the test-set
* input_yuvs - Folder with the YUV input files


## Test Set-Up

The test-set, which contains 62 individual tests, is tested on 3 different sequences. An overview of the test-set can be found in the Excel spreadsheet, located on the MPEG FTP server. In order to perform the test, the latest version of the LCEVC Test Model (LTM) shall be used (LTM v.5.3 at the time of writing).
| Sequence | Resolution | Frame rate | Bit depth | Base Codec |
| ------------ | ------------- | ------------ | ------------- | ------------- |
| Cactus | 1920x1080 | 50 fps | 8 | H.264/AVC (JM v.19.0) |
| CanYouReadThis | 1920x1080 | 50 fps | 8 | H.264/AVC (JM v.19.0) |
| ParkRunning3 | 3840x2160 | 50 fps | 10 | H.265/HEVC (HM v.16.20) |


## Performing a Test

Most of the tests can be run automatically without any additional input. The python script stresstest.py can be used to perform the encodes and decodes. Additionally, the reconstruction yuv file of the encoder and the yuv of the decoder are checked whether they are matching.
In case, multiple cores are available to perform the tests, the following command lines can be appended by the argument
```
--parallel=10
```
where the number of parallel jobs can be specified.

### H.264/AVC Base Codec
The following command line gives an example for running the python script on the Cactus sequence:
```
python stresstest.py 
--progress=spinner 
--encoder=lcevc_test_model/ModelEncoder 
--decoder=lcevc_test_model/ModelDecoder 
--label="Cactus_" 
--test_file=tests.json 
--parameters='{\"input_file\":\"input_yuvs/Cactus_49frames_1920x1080_50fps_420.yuv\",\"format\":\"yuv420p\",\"base_encoder\":\"avc\"}' 
--sets="stress"
```

### H.265/HEVC Base Codec
The following command line gives an example for running the python script on the ParkRunning3 sequence:
```
python stresstest.py 
--progress=spinner 
--encoder=lcevc_test_model/ModelEncoder 
--decoder=lcevc_test_model/ModelDecoder 
--label="ParkRunning3_" 
--test_file=tests.json 
--parameters='{\"input_file\":\"input_yuvs/ParkRunning3_63frames_3840x2160_50fps_10bit_420.yuv\",\"format\":\"yuv420p10\",\"base_encoder\":\"hevc\"}' 
--sets="stress,stress_10bit"
```

### Special Tests
Some tests cannot be performed automatically and need manual input.

* For Cactus & CanYouReadThis, the tests "RBD-1" and "RBD-2" require as an input the 10 bit yuv file (provided in the folder inputs_yuv). Additionally, the format needs to be changed to "yuv420p10". These tests can be run by specifying
```
--sets="stress_10bit"
```
on the command line of the python script.

* The tests "RBD-3 and RBD-4" require as an input the 12 bit yuv file (provided in the folder inputs_yuv). Additionally, the format needs to be changed to "yuv420p12". These tests can be run by specifying
```
--sets="stress_12bit"
```
on the command line of the python script.

* The test "CST-1" require as an input a monochrome yuv file (4:0:0) (provided in the folder inputs_yuv). Additionally, the format needs to be changed to "y" for Cactus & CanYouReadThis and to "y10" for ParkRunning3. The test can be run by specifying
```
--sets="stress_mono"
```
on the command line of the python script.

* For the tests "HDUD-1" through "HDUD-4", the user data must be extracted. This can be done by compiling the LCEVC Test Model (LTM) with "USER_DATA_EXTRACTION" (located in src/Config.hpp) turned on. The user data from the encoder and decoder will be written into a binary file. These files can be cross-checked with the files provided on the MPEG FTP server.


## Providing the Results

In order to track the cross-checking results, the provided Excel spreadsheet on the MPEG FTP server can be used. The corresponding columns for the cross-checking can be updated and the modified file can afterwards be uploaded to the FTP server.
