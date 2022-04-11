#!/usr/bin/env python3.7

import filecmp
import json
import csv
import xxhash,hashlib
import os, os.path
import ntpath
from pathlib import Path
import re
import shutil
import subprocess
import sys
import concurrent.futures

def platform_exe(path, name):
	"""Given a directory and an executable file name, return platform specifix path."""
	exesuffix = {"win32":".exe","linux":""}
	return os.path.join(path, name+exesuffix.get(sys.platform,""))


def prefix_path(d, f):
	if not os.path.isabs(v):
		return os.path.join(d, f)
	else:
		return f

def hash(filename, blocksize=65536):
	"""
	Compute hash of named file.
	"""
	hash = xxhash.xxh3_64()
	with open(filename, "rb") as f:
		for block in iter(lambda: f.read(blocksize), b""):
			hash.update(block)
	return hash.hexdigest()

def hash_md5(filename, blocksize=65536):
	"""
	Compute md5 hash of named file.
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

def parse_vooya_filename(name):
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
		if 'format' not in params:
			params['format'] = "420p"
		params['format'] += params.pop('bit_depth')

	if 'format' in params and params['format'].startswith('4'):
		params['format'] = 'yuv' + params['format']

	if 'width' in params:
		params['width'] = int(params['width'])
	if 'height' in params:
		params['height'] = int(params['height'])
	if 'fps' in params:
		params['fps'] = int(params['fps'])

	return params

def create_opl_file(width, height, output):
	data = []
	log = open(f"{output}_encoder.log", "r")
	poc = 0
	for line in log:
		if line[:6] == "[MD5Y ":
			frame_data = [poc, width, height]
			frame_data.append(line[6:38])
			frame_data.append(line[46:78])
			frame_data.append(line[86:118])
			data.append(frame_data)
			poc += 1

	with open(f"{output}.opl", 'w', newline='') as csvfile:
		writer = csv.writer(csvfile)
		writer.writerow(["PicOrderCntVal", "pic_width_max", "pic_height_max", "md5_y", "md5_u", "md5_v"])
		writer.writerows(data)

def create_txt_file(width, height, output, description):
	sample_rate = width * height * 50
	if sample_rate <= 29410000:
		level = 1
	elif sample_rate <= 124560000:
		level = 2
	elif sample_rate <= 527650000:
		level = 3
	else:
		level = 4
	lines = [
		f"Bitstream file name: {output}.bit\n",
		f"Explanation of bitstream features: {description}\n",
		f"Profile: Main\n",
		f"Level: {level}.1\n",
		f"Max picture width: {width}\n",
		f"Max picture height: {height}\n",
		f"Picture rate: 50\n",
		f"LTM release version number used to generate the bitstream: LTM 5.4\n",
		f"Contact name and email: Simone Ferrara (simone.ferrara@v-nova.com)\n",
	]
	f = open(f"{output}.txt", 'w')
	f.writelines(lines)
	f.close()

def run_subprocess(title, args, logfile, scriptfile, show_progress):
	"""Run a comamnd as a subprocess - report progress and capture output (both stderr and stdout) to a log file."""

	with open(logfile, "wt") as log:
		sys.stderr.write(f"    {title}: Start\r")
		if show_progress and show_progress.lower() != "none":
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

	sys.stderr.write(f"    {title}: OK    \n")

	# Write scripts
	# if scriptfile:
	# 	with open(scriptfile + ".sh", "wt") as f:
	# 		f.write("#!/bin/bash\n")
	# 		f.write("${LTM_PATH}" + os.path.basename(args[0]) + " " + " ".join(args[1:]))
	# 		f.write("\n")

	# 	with open(scriptfile + ".bat", "wt") as f:
	# 		f.write("@echo off\n")
	# 		f.write("%LTM_PATH%" + os.path.basename(args[0]) + ".exe " + " ".join(args[1:]))
	# 		f.write("\n")

	return True

def test_encode_decode(number, encoder, decoder, input_dir, base_dir, test, default_parameters, 
                       show_progress, decode_only, sequence, harness, validator):
	"""
	Run an encode followed by decode of given file (using given params)
	Return true if recon & decoded yuv match required checksum.
	"""

	## Merge per test params and defaults
	parameters = {**test['parameters'], **default_parameters}

	# Are any parameters read from file?
	if 'include-parameters' in test:
		with open(test['include-parameters']) as f:
			include_params = json.load(f)
			parameters = { **include_params, **parameters }

	# Extract any parameters from input name
	input_file_params = parse_vooya_filename(parameters['input_file'])
	parameters = { **input_file_params, **parameters }

	# Supress base if this test needs to generate its own
	if test.get('generate_base', False):
		if 'base' in parameters:
			parameters.pop('base')
		if 'base_recon' in parameters:
			parameters.pop('base_recon')

	# Add input dirs
	if 'input_file' in parameters and not os.path.isabs(parameters['input_file']):
		parameters['input_file'] = os.path.abspath(os.path.join(input_dir, parameters['input_file']))

	if 'base' in parameters and parameters['base'] != "" and not os.path.isabs(parameters['base']):
		parameters['base'] = os.path.abspath(os.path.join(base_dir, parameters['base']))

	if 'base_recon' in parameters and parameters['base_recon'] != "" and not os.path.isabs(parameters['base_recon']):
		parameters['base_recon'] = os.path.abspath(os.path.join(base_dir, parameters['base_recon']))

	# Figure input name and output name
	category = f"{test['name']}_v-nova_v{test['version']:02d}"
	output = f"{sequence}_{test['name']}_v-nova_v{test['version']:02d}"

	# Change to a subdirectory for each test
	# Avoids any issues with parallel runs clashing over temp files.
	#
	current_dir = os.getcwd()
	if not os.path.exists(category):
		os.mkdir(category)
	if not os.path.isdir(category):
		print(f"Cannot make test directory:{category}", file=sys.stderr)
		return False

	os.chdir(category)

	# Convert paths back to relative
	# (All a bit of a dance - but means everything survives interesting filesystem layouts and is readable)
	#
	if 'input_file' in parameters:
		parameters['input_file'] = os.path.relpath(parameters['input_file'])

	if 'base' in parameters:
		parameters['base'] = os.path.relpath(parameters['base'])

	if 'base_recon' in parameters:
		parameters['base_recon'] = os.path.relpath(parameters['base_recon'])

	# Create the parameter file
	with open(output + ".cfg", "wt") as f:
		json.dump(parameters, f, indent=4)

	sys.stderr.write(f"Encode/Decode {number}: {output} : {test['description']}\n")

	#
	encode_arguments = [
		os.path.relpath(encoder),
		f"--width={parameters['width']}",
		f"--height={parameters['height']}",
		f"--format={parameters['format']}",
		f"--base_encoder={parameters['base_encoder']}",
		f"--encapsulation={parameters['encapsulation']}",
		f"--parameters={output}.cfg",
		f"--output_file={output}.bit",
		f"--output_recon={output}_recon.yuv",
		f"--keep_base=false",
		f"--parameter_config=conformance",
		f"--qp={parameters['qp']}" ]

	if 'limit' in test:
		encode_arguments.append(f"--limit={test['limit']}")

	#
	decode_arguments = [
		os.path.relpath(decoder),
		f"--base_encoder={parameters['base_encoder']}",
		f"--encapsulation={parameters['encapsulation']}",
		f"--input_file={output}.bit",
		f"--output_file={output}_decoded.yuv" ]

	# Use external base decoder for monochrome input or 12/14 bpp
	if 'format' in parameters:
		if not parameters['format'].startswith('yuv'):
			decode_arguments.append("--base_external=true")
		elif parameters['format'].endswith('12') or parameters['format'].endswith('14'):
			decode_arguments.append("--base_external=true")

	## SDK Decoding
	if parameters["base_encoder"] == "avc":
		base_type = "h264"
	else:
		base_type = parameters["base_encoder"]

	validator_arguments = [
		os.path.relpath(validator),
		f"-i {output}.bit",
		f"-t nal",
		f"-o {output}_sdk.bit",
		f"-u bin",
		f"-b {base_type}",
	]

	tmp_dir = f"tmp_{sequence}"
	Path(tmp_dir).mkdir(parents=True, exist_ok=True)

	harness_arguments = [
		os.path.relpath(harness),
		f"-p {output}_sdk.bit",
		f"-b {parameters['base_recon']}", 
		f"-o {tmp_dir}/", 
		f"--format-filenames 0",
		f"--pipeline-mode 1"
	]

	# Encode
	if decode_only:
		encoder_ok = True
	else:
		encoder_ok = run_subprocess(f"  Encode {number} {output}", encode_arguments,
									f"{output}_encoder.log", f"{output}_encode", show_progress)

	# Encode recon checksums
	if os.path.exists(f"{output}_recon.yuv"):
		encoded_checksum_xxhash = hash(f"{output}_recon.yuv")
		encoded_checksum_md5 = hash_md5(f"{output}_recon.yuv")
	else:
		encoded_checksum_xxhash = "No encoded output (xxhash)"
		encoded_checksum_md5 = "No encoded output"

	# Encode bitstream checksums
	if encoder_ok:
		encoded_bitstream_checksum_md5 = hash_md5(f"{output}.bit")
	else:
		encoded_bitstream_checksum_md5 = "No bitstream output"

	# Decode & Validator
	if encoder_ok:
		decoder_ok = run_subprocess(f"  Decode {number} {output}", decode_arguments,
									f"{output}_decoder.log", f"{output}_decode", show_progress)
		validator_ok = run_subprocess(f"  Validator {number} {output}", validator_arguments,
									f"{output}_validator.log", f"{output}_validator", show_progress)
		harness_ok = run_subprocess(f"  Harness {number} {output}", harness_arguments,
									f"{output}_harness.log", f"{output}_harness", show_progress)
	else:
		decoder_ok = False
		validator_ok = False
		harness_ok = False

	# Decode checksum
	if os.path.exists(f"{output}_decoded.yuv"):
		decoded_checksum = hash(f"{output}_decoded.yuv")
	else:
		decoded_checksum = "No decoded output"

	# SDK checksum
	if os.path.exists(f"{tmp_dir}/conformance_window.yuv"):
		sdk_checksum = hash(f"{tmp_dir}/conformance_window.yuv")
	elif os.path.exists(f"{tmp_dir}/high.y"):
		sdk_checksum = hash(f"{tmp_dir}/high.y")
	elif os.path.exists(f"{tmp_dir}/high.yuv"):
		sdk_checksum = hash(f"{tmp_dir}/high.yuv")
	else:
		sdk_checksum = "No SDK output"

	# Record bitstream md5 checksum
	with open(output + ".md5", "wt") as f:
		f.write(encoded_bitstream_checksum_md5 + "\n")

	# Record recon md5 checksum
	with open(output + ".yuv.md5", "wt") as f:
		f.write(encoded_checksum_md5 + "\n")

	# Record checksum status
	with open(output + "_status.txt", "wt") as f:
		f.write(f"LTM: encode:{encoded_checksum_xxhash} decode:{decoded_checksum} ok:{encoded_checksum_xxhash==decoded_checksum}\n")
		f.write(f"SDK: encode:{encoded_checksum_xxhash} decode:{sdk_checksum} ok:{encoded_checksum_xxhash==sdk_checksum}\n")

	# Store checksum for json file
	test['checksum'] = encoded_checksum_xxhash
	test['base_recon'] = ntpath.basename(parameters['base_recon'])
	test['sequence'] = sequence

	# Check LTM & SDK match
	check_match = encoded_checksum_xxhash == decoded_checksum
	print(f"LTM: Encoded vs decoded checksum {number} {output}: ", check_match and "OK" or "FAILED", file=sys.stderr)
	check_match_sdk = encoded_checksum_xxhash == sdk_checksum
	print(f"SDK: Encoded vs decoded checksum {number} {output}: ", check_match_sdk and "OK" or "FAILED", file=sys.stderr)

	# Remove yuv outputs
	if os.path.exists(f"{output}_recon.yuv"):
		os.remove(output + "_recon.yuv")
	if os.path.exists(f"{output}_decoded.yuv"):
		os.remove(output + "_decoded.yuv")
	
	try:
		shutil.rmtree(tmp_dir)
	except OSError as e:
		print("Error deleting harness folder: %s - %s." % (e.filename, e.strerror))

	# Create .opl and .txt files as specified in MPEG Conformance Test
	if check_match:
		create_opl_file(parameters['width'], parameters['height'], output)
		create_txt_file(parameters['width'], parameters['height'], output, test['description'])

	# Check Userdata
	if os.path.isfile("userdata_enc.bin"):
		if os.path.isfile("userdata_dec.bin"):
			userdata_ok = filecmp.cmp("userdata_enc.bin", "userdata_dec.bin")
			os.rename("userdata_enc.bin", f"{output}_userdata.bin")
			os.remove("userdata_dec.bin")
		else:
			userdata_ok = False
			
		print(f"LTM: Userdata {number} {output}: ", userdata_ok and "OK" or "FAILED", file=sys.stderr)
	else:
		userdata_ok = True

	os.chdir(current_dir)

	return (encoder_ok and decoder_ok and check_match and check_match_sdk and harness_ok and userdata_ok)

def run_test(test_job):
	"""Wrapper for running a parallel test instance - unpacks arguments and forwards to real function."""
	print(f"-- Parallel Test {test_job['number']} {test_job['sequence']} {test_job['test']['name']}")
	ok = test_encode_decode(**test_job)
	return ok, test_job['number'], test_job['test']

def run_tests(args):
	"""Read a JSON array and run test described by each entry."""

	fails = 0
	counter = 0

	# Load tests
	with open(args.test_file) as f:
		tests = json.load(f)
	if type(tests) != list:
		print("Top level of tests JSON must be an array.", file=sys.stderr)
		return 1

	# Load codecs
	with open(args.codecs_file) as f:
		codecs = json.load(f)

	# Load inputs
	with open(args.inputs_file) as f:
		inputs = json.load(f)
	
	# Load bases
	with open(args.bases_file) as f:
		bases = json.load(f)

	if args.sets:
		test_set = set(args.sets.split(','))
	else:
		test_set = set()

	test_jobs = []

	for codec in codecs:
		
		# Set-up codec parameters
		codec_parameters = dict()
		codec_parameters['base_encoder'] = codec
		codec_parameters['width'] = codecs[codec]['width']
		codec_parameters['height'] = codecs[codec]['height']

		for sequence in codecs[codec]['sequences']:

			## Build test jobs - apply filters from command line
			for n, test in enumerate(tests):
				
				# Sets of tests
				if test_set:
					if 'sets' not in test or test_set.isdisjoint(set(test['sets'])):
						continue
				
				# Codec
				if codec not in test['sets']:
					continue

				# Set-up test parameters
				test_parameters = codec_parameters.copy()
				input_type = test['input']
				base_type = test['base']
				test_parameters['input_file'] = f"{inputs[input_type][sequence]}.yuv"
				test_parameters['base'] = f"{bases[base_type][sequence]}.bit"
				test_parameters['base_recon'] = f"{bases[base_type][sequence]}.yuv"

				bitdepth = re.search("[0-9]+bit", test_parameters['input_file'])[0]
				bitdepth = int(bitdepth.replace("bit", ""))

				if 'luma' in test_parameters['input_file']:
					test_parameters['format'] = f"y{bitdepth}"
				else:
					test_parameters['format'] = f"yuv420p{bitdepth}"

				test_jobs.append({
					'number': counter,
					'test': test,
					'encoder': os.path.abspath(args.encoder),
					'decoder': os.path.abspath(args.decoder),
					'validator': os.path.abspath(args.validator),
					'harness': os.path.abspath(args.harness),
					'input_dir': args.input_dir,
					'base_dir': args.base_dir,
					'default_parameters': test_parameters,
					'show_progress': args.progress,
					'decode_only': args.decode_only,
					'sequence': sequence})

				counter += 1

	## Run the jobs
	if args.parallel == "0":
		print(f"Running {len(test_jobs)} tests", file=sys.stderr)
		# Run jobs serially in process
		for j in test_jobs:
			ok = test_encode_decode(**j)
			if not ok:
				fails += 1
	else:
		print(f"Running {len(test_jobs)} tests using {args.parallel} workers", file=sys.stderr)
		# Run jobs in parallel in seperate processes
		with concurrent.futures.ProcessPoolExecutor(max_workers=int(args.parallel)) as executor:
			for ok,n,updated_test in executor.map(run_test, test_jobs):
				if not ok:
					fails += 1
				# Collect updated data from subprocess
				test_jobs[n] = updated_test

	if fails != 0:
		print(f"Tests failed: {fails}", file=sys.stderr)

	# Dump json with hashes
	hashes = dict()
	for test in test_jobs:
		try:
			test_dict = {
				"base": test['base_recon'],
				"hash": test['checksum']
			}
		except KeyError:
			test_dict = {}
		hashes[f"{test['sequence']}_{test['name']}_v-nova_v{test['version']:02d}"] = test_dict

	with open("hashes.json", "wt") as f:
		json.dump(hashes, f, indent=4)

	return fails

if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("--encoder", help="Test Model encoder", default=platform_exe("..", "ModelEncoder"))
	parser.add_argument("--decoder", help="Test Model decoder", default=platform_exe("..", "ModelDecoder"))
	parser.add_argument("--validator", help="Validator", default=platform_exe("..", "lcevc_validator"))
	parser.add_argument("--harness", help="Dec Harness", default=platform_exe("..", "lcevc_dec_harness"))
	parser.add_argument("--test_file", help="JSON file of tests", default="tests.json")
	parser.add_argument("--bases_file", help="JSON file of bases", default="bases.json")
	parser.add_argument("--inputs_file", help="JSON file of inputs", default="inputs.json")
	parser.add_argument("--codecs_file", help="JSON file of codecs", default="codecs.json")
	parser.add_argument("--input_dir", help="Base for test inputs", default="inputs")
	parser.add_argument("--base_dir", help="Base for test input base and base recon.", default="bases")
	parser.add_argument("--sets", help="Select set of tests to be run", default="")
	parser.add_argument("--progress", help="Display progresss spinner while coding (none, spinner or verbose)", default="none")
	parser.add_argument("--parallel", help="Number of parallel workers to use", default="0")
	parser.add_argument("--decode_only", help="Perform decoding only", default="")
	args = parser.parse_args()

	sys.exit(run_tests(args))
