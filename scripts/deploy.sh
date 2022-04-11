#!/bin/bash
#
deploy_dest="serifos:/mnt/network/padfs020/Data/Projects/LCEVCTestModel"
version=$(git describe --dirty --always --long --tags)
base_name=lcevc_test_model_${version}


install_dir=/tmp/${base_name}

## Linux
#
install -d -m 755 "${install_dir}/linux"
install -d -m 755 "${install_dir}/linux/external_codecs"
install -d -m 755 "${install_dir}/linux/external_codecs/JM"
install -d -m 755 "${install_dir}/linux/external_codecs/HM"

install -D -m 755 ModelDecoder "${install_dir}/linux"
install -D -m 755 ModelEncoder "${install_dir}/linux"

install -D -m 755 external_codecs/JM/ldecod "${install_dir}/linux/external_codecs/JM"
install -D -m 755 external_codecs/JM/lencod "${install_dir}/linux/external_codecs/JM"
install -D -m 755 external_codecs/JM/*.cfg "${install_dir}/linux/external_codecs/JM"

install -D -m 755 external_codecs/HM/TAppDecoder "${install_dir}/linux/external_codecs/HM"
install -D -m 755 external_codecs/HM/TAppEncoder "${install_dir}/linux/external_codecs/HM"
install -D -m 755 external_codecs/HM/*.cfg "${install_dir}/linux/external_codecs/HM"

## Windows
#
install -d -m 755 "${install_dir}/windows"
install -d -m 755 "${install_dir}/windows/external_codecs"

install -D -m 755 ModelDecoder.exe  "${install_dir}/windows"
install -D -m 755 ModelEncoder.exe  "${install_dir}/windows"

install -D -m 755 external_codecs/JM/ldecod.exe "${install_dir}/windows/external_codecs/JM"
install -D -m 755 external_codecs/JM/lencod.exe "${install_dir}/windows/external_codecs/JM"
install -D -m 755 external_codecs/JM/*.cfg "${install_dir}/windows/external_codecs/JM"

install -D -m 755 external_codecs/HM/TAppDecoder.exe "${install_dir}/windows/external_codecs/HM"
install -D -m 755 external_codecs/HM/TAppEncoder.exe "${install_dir}/windows/external_codecs/HM"
install -D -m 755 external_codecs/HM/*.cfg "${install_dir}/windows/external_codecs/HM"

make README.html
install -D -m 644 README.html "${install_dir}"

# Gather into ZIP file to preserve +x and copy to deployment dir.
( cd "${install_dir}"/.. && zip -r "${base_name}.zip" "${base_name}" && scp "${base_name}.zip" "${deploy_dest}")

echo "-- ${version} \\\\padfs020\\Data\\Projects\\LCEVCTestModel\\${base_name}.zip"
