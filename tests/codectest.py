#!/usr/bin/env python3
#
# codectest.py [--encoder=<encoder>] [--decoder=<decoder>] [--test_file=<tests-csv>] [--label=<label>] [--input_dir=<dir>] [--progress=<bool>]
#
import csv
import hashlib
import os.path
import re
import subprocess
import sys

# Columns in CSV that are consumed by test, not by codec
TEST_PARAMETERS=[ 'checksum' ]

INPUTS=['input_file', 'base', 'base_recon']
def make_opt(d, k, v):
	"""Convert key,value into a command line parameter, prefixing any inputs with input base dir."""
	if k in INPUTS and not os.path.isabs(v):
		p = os.path.join(d, v)
	else:
		p = v
	return f"--{k}={p}"

def md5sum(filename, blocksize=65536):
	"""
	Compute MD5 checksum of named file.
	"""
	hash = hashlib.md5()
	with open(filename, "rb") as f:
		for block in iter(lambda: f.read(blocksize), b""):
			hash.update(block)
	return hash.hexdigest()

IMAGE_NAME_PARAMS = [
	("width", re.compile("([0-9]+)x[0-9]+")),
	("height", re.compile("[0-9]+x([0-9]+)")),
	("fps", re.compile("([0-9]+)(fps|hz)")),
	("bit_depth", re.compile("([0-9]+)(bits?|bpp)")),
	("format", re.compile("^(420p|422p|yuv|yuyv|y|)$"))
]

def parse_filename(name):
	"""
	Extract vooya-ish image metadata from filename in form suitable for use as encoder parameters.
	"""
	params = {}
	for p in name.split("_"):
		for n,r in IMAGE_NAME_PARAMS:
			m = r.match(p)
			if m:
				params[n] = m.group(1)

	if 'bit_depth' in params:
		params['format'] += params.pop('bit_depth')

	if 'format' in params and params['format'].startswith('4'):
		params['format'] = 'yuv' + params['format']

	return params

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
				if show_progress:
					progress_indicator = "|/-\\|/-\\"[n % 8]
					sys.stderr.write(f"    {title}: {progress_indicator}      \r")
				print(l.strip(), file=log)

			if p.wait() != 0:
				sys.stderr.write(f"    {title}: FAILED\n")
				return False

	sys.stderr.write(f"    {title}: OK\n")
	return True

def run_testmodel_encode_decode(encoder, decoder, input_dir, params, label, show_progress):
	"""
	Run an encode followed by decode of given file (using given params)
	Return true is recon & decoded yuv match required checksum.
	"""
	basename = os.path.splitext(os.path.basename(params['input_file']))[0]
	output = f"test_{label}"
	args = list(map(lambda i: make_opt(input_dir, i[0], i[1]),
					filter(lambda i: i[0] not in TEST_PARAMETERS and i[1],
						   params.items())))

	sys.stderr.write(f"Encode/Decode Test: {label} {basename} "
					 f"SW1:{params['cq_step_width_loq_1']} "
					 f"SW2:{params['cq_step_width_loq_0']} "
					 f"QP:{params['qp']}\n")

	# Encode
	encoder_ok = run_subprocess(f"YUV Encode", [encoder, "--parameters=config_TM.json",
												"--output_file="+output+".lvc",
												"--output_recon="+output+".yuv" ] + args,
								f"test_{label}_encoder.log", show_progress)

	# Encode recon checksum
	encoded_checksum = md5sum(output+".yuv")

	# Decode
	decoder_ok = run_subprocess(f"YUV Decode", [decoder, "--format=yuv420p",
												"--base_encoder="+params['base_encoder'],
												"--encapsulation="+params['encapsulation'],
												"--input_file="+output+".lvc",
												"--output_file="+output+"_decoded.yuv" ],
								f"test_{label}_decoder.log", show_progress)

	# Decode checksum
	decoded_checksum = md5sum(output+"_decoded.yuv")

	check_encode = (not params['checksum']) or params['checksum'] == encoded_checksum
	check_match = encoded_checksum == decoded_checksum

	print("Encoded checksum: ", check_encode and "OK" or "FAILED", file=sys.stderr)
	print("Encoded vs decoded checksum: ", check_match and "OK" or "FAILED", file=sys.stderr)
	return encoder_ok and decoder_ok and check_encode and check_match

def run_testmodel_decode(decoder, input_dir, params, label, show_progress):
	"""
	Run an decode of prepared ES file.
	Return true is recon & decoded yuv match required checksum.
	"""

	basename = os.path.splitext(os.path.basename(params['input_file']))[0]
	output = f"test_{label}"
	args = list(map(lambda i: make_opt(input_dir, i[0], i[1]),
					filter(lambda i: i[0] not in TEST_PARAMETERS and i[1],
						   params.items())))

	sys.stderr.write(f"Decode Test: {label} {basename}\n")

	# Decode
	decoder_ok = run_subprocess(f"ES Decode", [decoder,
											   "--output_file="+output+"_decoded.yuv" ] + args,
								f"test_{label}_decoder.log", show_progress)

	# Decode checksum
	decoded_checksum = md5sum(output+"_decoded.yuv")

	check_decode = (not params['checksum']) or params['checksum'] == decoded_checksum
	print("Decoded checksum: ", check_decode and "OK" or "FAILED", file=sys.stderr)

	return decoder_ok and check_decode

def run_tests(label, encoder, decoder, input_dir, configs, show_progress):
	"""Read a .CSV file and run test described by each line."""

	fails = 0
	for n,test_params in enumerate(csv.DictReader(configs)):
		base,ext = os.path.splitext(test_params['input_file'])

		# Extract implicit parameter names from input_file
		params = { **parse_filename(base), **test_params}

		if ext == '.yuv':
			if not run_testmodel_encode_decode(encoder, decoder, input_dir, params, f"{label}{n}", show_progress):
				fails += 1
		elif  ext in ['.es', '.lvc']:
			if not run_testmodel_decode(decoder, input_dir, params, f"{label}{n}", show_progress):
				fails += 1
		else:
			print(f"Unrecognised intput: {inputext}", file=sys.stderr)
			fails += 1

	if fails != 0:
		print("Tests failed", file=sys.stderr)

	return fails

def platform_exe(path, name):
	"""Given a directory and an executable file name, return platform specifix path."""
	exesuffix = {"win32":".exe","linux":""}
	return os.path.join(path, name+exesuffix.get(sys.platform,""))

if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("--label", help="label for test outputs", default="")
	parser.add_argument("--encoder", help="Test Model encoder", default=platform_exe("..","ModelEncoder"))
	parser.add_argument("--decoder", help="Test Model decoder", default=platform_exe("..","ModelDecoder"))
	parser.add_argument("--test_file", help="CSV file of tests", default="tests.csv")
	parser.add_argument("--input_dir", help="Base for test inputs", default="data")
	parser.add_argument("--progress", help="Display progresss spinner while coding", default="true")
	args = parser.parse_args()

	with open(args.test_file) as f:
		sys.exit(run_tests(args.label, args.encoder, args.decoder, args.input_dir, f, args.progress=="true"))
