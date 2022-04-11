import glob
import filecmp
import json
import hashlib
import os
import os.path
import subprocess
import sys

def platform_exe(path, name):
	"""Given a directory and an executable file name, return platform specifix path."""
	exesuffix = {"win32":".exe","linux":""}
	return os.path.join(path, name+exesuffix.get(sys.platform,""))

def run_subprocess(title, args, logfile, show_progress):
	"""Run a comamnd as a subprocess - report progress and capture output (both stderr and stdout) to a log file."""

	with open(logfile, "wt") as log:
		sys.stderr.write(f"    {title}: Start\r")
		if show_progress:
			sys.stderr.write("\r")
		else:
			sys.stderr.write("\n")

		print(" ".join(args), file=log)

		with subprocess.Popen(args, bufsize=1, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
							  universal_newlines=True) as p:
			for n,l in enumerate(p.stdout):
				# Progress
				if show_progress == "spinner":
					progress_indicator = "|/-\\|/-\\"[n % 8]
					sys.stderr.write(f"    {title}: {progress_indicator}      \r")
				elif show_progress == "verbose":
					sys.stderr.write(l)
				# Log file
				print(l.strip(), file=log)

			if p.wait() != 0:
				sys.stderr.write(f"    {title}: FAILED\n")
				return False

	sys.stderr.write(f"    {title}: OK\n")
	return True

def hash_md5(filename, blocksize=65536):
	"""
	Compute md5 hash of named file.
	"""
	hash = hashlib.md5()
	with open(filename, "rb") as f:
		for block in iter(lambda: f.read(blocksize), b""):
			hash.update(block)
	return hash.hexdigest()

def run_decode(decoder, input_dir, base):

    os.chdir(input_dir)

    for filename in glob.glob("**/*.bit"):
        name = filename[:-4]
        if base == 'auto':
            if os.path.isfile(f"{name}.cfg"):
                with open(f"{name}.cfg") as json_file:
                    config = json.load(json_file)
                    base_run = config['base_encoder']
                    yuv_format = config['format']
                    base_external = False
                    if not yuv_format.startswith("yuv"):
                        base_external = True
                    elif yuv_format.endswith("12"):
                        base_external = True
                    elif yuv_format.endswith("14"):
                        base_external = True
            else:
                if 'Cactus' in name or 'CanYouReadThis' in name:
                    base_run = "avc"
                    base_external = False
                elif 'ParkRunning3' in name:
                    base_run = "hevc"
                    if '12bit' in name or '14bit' in name:
                        base_external = True
                    else:
                        base_external = False
                else:
                    NameError
        else:
            base_run = base
            base_external = False


        decode_arguments = [
            os.path.abspath(decoder),
            f"--base_encoder={base_run}",
            f"--encapsulation=nal",
            f"--input_file={filename}",
            f"--output_file={name}.yuv"
        ]

        if base_external:
            decode_arguments.append("--base_external=true")

        # Decode
        decoder_ok = run_subprocess(f"  Decode {name}", decode_arguments,
                                    f"{name}_decoder.log", show_progress='spinner')

        # Decode checksum
        if decoder_ok:
            decoded_checksum = hash_md5(f"{name}.yuv")
            if os.path.isfile(f"{name}.yuv.md5"):
                f = open(f"{name}.yuv.md5", "r")
                stored_checksum = f.readline()[0:32]
                check_checksum = stored_checksum == decoded_checksum
                print(f"Encoded vs stored checksum {name}: ", check_checksum and "OK" or "FAILED")
                if not check_checksum:
                    print(f"Decoded MD5: {decoded_checksum}")
                    print(f"Stored MD5:  {stored_checksum}")
            else:
                print(f"MD5 {name}.yuv  --  {decoded_checksum}")

            # Check Userdata Files
            if os.path.isfile(f"{name}_userdata.bin"):
                if os.path.isfile("userdata_dec.bin"):
                    userdata = filecmp.cmp(f"{name}_userdata.bin", "userdata_dec.bin")
                    os.remove("userdata_dec.bin")
                else:
                    userdata = False    
                print(f"Userdata Comparison {name}: ", userdata and "OK" or "FAILED")

            # Remove temporary files
            try:
                os.remove(f"{name}.yuv")
                # os.remove(f"{name}_decoder.log")
            except FileNotFoundError:
                pass
        else:
            print(f"decode FAILED: {name}")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--decoder", help="Test Model decoder",
                        default=platform_exe("..", "ModelDecoder"))
    parser.add_argument(
        "--input_dir", help="Input Directory", default="inputs")
    parser.add_argument(
        "--base", help="Base Encoder (auto/avc/hevc/evc/vvc)", default="auto")

    args = parser.parse_args()
    run_decode(os.path.abspath(args.decoder), args.input_dir, args.base)
