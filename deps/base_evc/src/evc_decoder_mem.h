/*
 */
#pragma once

#include <stdint.h>

typedef struct evc_writer_t {
	void (*output_begin_fn)(void *writer_data, int is16bit, uint32_t width, uint32_t height);
	size_t (*output_write_fn)(void *writer_data, const uint8_t *output_data, size_t size);
	void (*output_end_fn)(void *writer_data);

	void *data;
} evc_writer_t;

int evc_decode_mem(const uint8_t *stream, size_t stream_size, unsigned output_bit_depth, const evc_writer_t *writer);

