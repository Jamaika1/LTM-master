LCEVC Test Model
================

## Compilation

To build the LCEVC test model, first download the source from the repository (this will also download the JM and HM test model sources from their respective repositories).

```
git clone --recurse-submodules http://mpegx.int-evry.fr/software/MPEG/LCEVC/LTM.git
```

The default build mechanism is CMake, version 3.10 or later, examples of building for:

* Linux:

```
cd LTM
mkdir _build_linux
cd _build_linux
cmake ..
cmake --build .
```

* Windows 64 bit with Visual Studio 2017:

```
cd LTM
mkdir _build_win64
cd _build_win64
cmake -G “Visual Studio 15 2017 Win64” ..
cmake --build .
```

* Windows 64 bit with Visual Studio 2019 (using CMake 3.14 or later):

```
cd LTM
mkdir _build_win64
cd _build_win64
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build .
```


## Decoder

This will take an elementary stream and decode it to a raw YUV420 file.

The underlying codec should be given on the command line.

### Usage:
```
  LCEVC Decoder

  ModelDecoder.exe [OPTION...]
  Use equal sign to set boolean values (example: --dump_surfaces=true)

  -i, --input_file arg     Input elementary stream filename (default: input.lvc)
  -o, --output_file arg    Output filename for decoded YUV data (default: output.yuv)
  -b, --base arg           Base codec (avc, hevc, evc, vvc, or yuv) (default: avc)
      --base_encoder arg   Base codec (same as --base) (default: avc)
      --base_external      Use an external base codec executable (select for decoding of monochrome output)
  -y, --base_yuv arg       Prepared YUV data for base decode (default: )
      --input_yuv arg      Original YUV data for PSNR computation (default: )
  -l, --limit arg          Number of frames to decode (default: 1000000)
      --dump_surfaces      Dump intermediate surfaces to yuv files
      --encapsulation arg  Wrap enhancement as SEI or NAL (default: nal)
      --dithering_switch   Disable decoder dithering independent of configuration in bitstream (default: true)
      --dithering_fixed    Use a fixed seed for dithering
      --report             Calculate PSNR and checksums
      --keep_base          Keep the base + enhancement bitstreams and base decoded yuv file
      --apply_enhancement  Apply LCEVC enhancement data (residuals) on output YUV (default: true)
      --version            Show version
      --help               Show help
```

### Examples:

Decode an 8 bit HD stream, based on AVC:

```
c:> ModelDecoder --base_encoder=avc --input_file=Cact_1920x1080_50fps_08bpp.lvc --output_file=Cact_1920x1080_50fps_08bpp_dec.yuv
```

Decode a 10 bit HD stream, based on AVC:

```
c:> ModelDecoder --base_encoder=avc --input_file=Cact_1920x1080_50fps_10bpp.lvc --output_file=Cact_1920x1080_50fps_10bpp_dec.yuv
```

Decode an 8 bit UHD stream, based on HEVC:

```
c:> ModelDecoder --base_encoder=hevc --input_file=Park_3840x2160_50fps_08bpp.lvc --output_file=Park_3840x2160_50fps_08bpp_dec.yuv
```

Decode a 10 bit UHD stream, based on HEVC:

```
c:> ModelDecoder --base_encoder=hevc --input_file=Park_3840x2160_50fps_10bpp.lvc --output_file=Park_3840x2160_50fps_10bpp_dec.yuv
```

Please note that the base decoder (AVC, HEVC, EVC or VVC) is called by the LTM Decoder to reconstruct the base decoded YUV sequence.<br>
Thus, it is essential to have the `external_codecs` folder copied in the working directory.

## Encoder

This will take:
  - A source video in raw YUV 420 format, either 8 bpp or 10 bpp, 
  - A base format elementary stream, either AVC, HEVC, EVC or VVC, 
  - The reconstructed base video YUV, 
  - A description of video format, 
  - A choice of the base format, 
  - Optionally, a configuration file for the encoder, e.g. config.json 

Please note that the base encoder or decoder (AVC, HEVC, EVC or VVC) may be called by the LTM Encoder to construct the base encoded bitstream as well as the corresponding YUV sequence.<br>
Thus, it is essential to have the `external_codecs` folder copied in the working directory.

In case the option `--parameter_config=default` is used, some of the parameter values will be derived from the input step width of enhancement sub-layer 2. To disable this feature, please use `--parameter_config=conformance`.

There are three possible modes of operation of the LTM Encoder, depending on the input sequence provided:
  - using as input an original YUV sequence, the Encoder will first invoke the "external base encoder (AVC, HEVC, EVC or VVC)" to produce the base bitstream and the corresponding YUV reconstruction.
then, the LTM Encoder will use such base bitstream plus YUV sequence for its operation;
  - using as input an already encoded base, without the corresponding YUV, the Encoder will first invoke the "external base decoder (AVC, HEVC, EVC or VVC)" to produce the corresponding YUV reconstruction.
then, the LTM Encoder will use such base bitstream plus YUV sequence for its operation;
  - using as input an already encoded AVC, HEVC, EVC or VVC base, with the corresponding YUV, 
the LTM Encoder will directly use such base bitstream plus YUV sequence for its operation.

### Usage:
```
  LCEVC Encoder

  ModelEncoder.exe [OPTION...]
  Use equal sign to set boolean values (example: --dump_surfaces=true)

  -i, --input_file arg                   Input filename for raw YUV video frames (default: source.yuv)
  -o, --output_file arg                  Output filename for elementary stream (default: output.lvc)
  -w, --width arg                        Image width (default: 1920)
  -h, --height arg                       Image height (default: 1080)
  -r, --fps arg                          Frame rate (default: 50)
  -l, --limit arg                        Limit number of frames to encode
  -f, --format arg                       Picture format (yuv420p, yuv420p10, yuv420p12, yuv420p14, yuv422p, yuv422p10, yuv422p12, yuv422p14, yuv444p, yuv444p10, yuv444p12, yuv444p14, y, y10, y12, or y14) (default: yuv420p)
  -p, --parameters arg                   JSON parameters for encoder (path to .json file) (default: {})
      --parameter_config arg             Configuration with default parameter values to use (default or conformance) (default: default)
      --dump_surfaces                    Dump intermediate surfaces to yuv files
      --downsample_only                  Downsample input and write to output.
      --upsample_only                    Upsample input and write to output.
      --output_recon arg                 Output filename for encoder yuv reconstruction (must be specified for output)
      --encapsulation arg                Code enhancement as SEI or NAL (default: nal)
      --version                          Show version
      --help                             Show this help
```

Other options are provided to define parameters for the different coding tools.
Please check the usage list displayed by typing:

```
c:> ModelEncoder --help
```

### Examples:

Encode an 8 bit HD stream, based on AVC:

```
c:> ModelEncoder --width=1920 --height=1080 --format=yuv420p --input_file=Cact_1920x1080_50fps_08bpp.yuv --output_file=Cact_1920x1080_50fps_QP24_1000.lvc --output_recon=Cact_1920x1080_50fps_QP24_1000_enc.yuv --base_encoder=avc --encapsulation=nal --base=Cact_0960x0540_50fps_QP24.avc --base_recon=Cact_0960x0540_50fps_QP24_avc.yuv --limit=500 --cq_step_width_loq_1=32767 --cq_step_width_loq_0=1000 
```

Encode a 10 bit HD stream, based on AVC:

```
c:> ModelEncoder --width=1920 --height=1080 --format=yuv420p10 --input_file=Cact_1920x1080_50fps_10bpp.yuv --output_file=Cact_1920x1080_50fps_QP24_1000.lvc --output_recon=Cact_1920x1080_50fps_QP24_1000_enc.yuv --base_encoder=avc --encapsulation=nal --base=Cact_0960x0540_50fps_QP24.avc --base_recon=Cact_0960x0540_50fps_QP24_avc.yuv --limit=500 --cq_step_width_loq_1=32767 --cq_step_width_loq_0=1000 
```

Encode an 8 bit UHD stream, based on HEVC:

```
c:> ModelEncoder --width=3840 --height=2160 --format=yuv420p --input_file=Park_3840x2160_50fps_08bpp.yuv --output_file=Park_3840x2160_50fps_QP24_1000.lvc --output_recon=Park_3840x2160_50fps_QP24_1000_enc.yuv --base_encoder=hevc --encapsulation=nal --base=Park_1920x1080_50fps_QP24.avc --base_recon=Park_1920x1080_50fps_QP24_avc.yuv --limit=500 --cq_step_width_loq_1=32767 --cq_step_width_loq_0=1000 
```

Encode a 10 bit UHD stream, based on HEVC:

```
c:> ModelEncoder --width=3840 --height=2160 --format=yuv420p10 --input_file=Park_3840x2160_50fps_10bpp.yuv --output_file=Park_3840x2160_50fps_QP24_1000.lvc --output_recon=Park_3840x2160_50fps_QP24_1000_enc.yuv --base_encoder=hevc --encapsulation=nal --base=Park_1920x1080_50fps_QP24.avc --base_recon=Park_1920x1080_50fps_QP24_avc.yuv --limit=500 --cq_step_width_loq_1=32767 --cq_step_width_loq_0=1000 
```
