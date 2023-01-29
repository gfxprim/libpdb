// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2009-2023 Cyril Hrubis <metan@ucw.cz>
 */

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "utils.h"
#include "libpdb.h"

#define ERR(...) { fprintf(stderr, "%s: %i: ", __FILE__, __LINE__); \
                   fprintf(stderr, __VA_ARGS__); \
		   fprintf(stderr, "\n"); }

int pdb_load_header(struct pdb *self)
{
	uint8_t b[PDB_HEADER_SIZE];
	uint8_t *buf = b;
	struct pdb_header *header = &self->header;

	if (read(self->fd, buf, PDB_HEADER_SIZE) < PDB_HEADER_SIZE) {
		ERR("read() failed :(")
		return -1;
	}

	load_string(&buf, header->name, PDB_HEADER_NAME_SIZE);
	header->file_attributes     = load_big_word(&buf);
	header->version             = load_big_word(&buf);
	header->creation_date       = load_big_dword(&buf);
	header->modification_date   = load_big_dword(&buf);
	header->last_backup_date    = load_big_dword(&buf);
	header->modification_number = load_big_dword(&buf);
	header->app_info_area       = load_big_dword(&buf);
	header->sort_info_area      = load_big_dword(&buf);
	load_string(&buf, header->database_type, 4);
	load_string(&buf, header->creator_id, 4);
	header->unique_id_seed      = load_big_dword(&buf);
	header->next_record_list_id = load_big_dword(&buf);
	header->number_of_records   = load_big_word(&buf);

	return 0;
}

int pdb_load_record_lists(struct pdb *pdb)
{
	int i;
	uint16_t number_of_records = pdb->header.number_of_records;
	uint8_t b[number_of_records*PDB_RECORD_LIST_SIZE];
	uint8_t *buf = b;
	struct pdb_record_list *tmp = malloc(sizeof(struct pdb_record_list)*number_of_records);

	if (!tmp) {
		ERR("malloc() failed :(")
		return -2;
	}

	if (read(pdb->fd, buf, number_of_records*PDB_RECORD_LIST_SIZE) < (ssize_t)number_of_records*PDB_RECORD_LIST_SIZE) {
		ERR("read() failed :(")
		free(tmp);
		return -1;
	}

	pdb->record_list = tmp;

	for (i = 0; i < number_of_records; i++) {
		tmp[i].record_data_offset = load_big_dword(&buf);
		tmp[i].record_attributes  = load_byte(&buf);
		tmp[i].unique_id[0] = load_byte(&buf);
		tmp[i].unique_id[1] = load_byte(&buf);
		tmp[i].unique_id[2] = load_byte(&buf);
	}

	return 0;
}

struct pdb *pdb_open(const char *path)
{
	struct pdb *pdb;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		int no = errno;
		ERR("Can't open file %s: %s", path, strerror(no))
		return NULL;
	}

	pdb = malloc(sizeof (struct pdb) + strlen(path) + 1);
	if (!pdb)
		goto err0;

	strcpy(pdb->path, path);
	pdb->fd = fd;
	pdb->enc = LU_NONE;

	if (pdb_load_header(pdb))
		goto err1;

	if (pdb_load_record_lists(pdb))
		goto err1;

	return pdb;
err1:
	free(pdb);
err0:
	close(fd);
	return NULL;
}

int pdb_close(struct pdb *handle)
{
	int no = close(handle->fd);
	free(handle->record_list);
	free(handle);
	return no;
}

void pdb_set_enc(struct pdb *pdb, enum lu_enc enc)
{
	char tmp[sizeof(pdb->header.name)];

	pdb->enc = enc;

	lu_to_utf8_cpy(pdb->header.name, tmp, enc);
	strcpy(tmp, pdb->header.name);
}

struct pdb_text_header *pdb_read_text_header(struct pdb *pdb)
{
	struct pdb_text_header *header = malloc(sizeof(struct pdb_text_header));
	uint8_t b[PDB_TEXT_HEADER_SIZE];
	uint8_t *buf = b;

	if (!header) {
		ERR("Malloc failed :(");
		return NULL;
	}

	lseek(pdb->fd, pdb->record_list[0].record_data_offset, SEEK_SET);

	if (read(pdb->fd, buf, PDB_TEXT_HEADER_SIZE) < PDB_TEXT_HEADER_SIZE) {
		ERR("Fread failed :(");
		free(header);
		return NULL;
	}

	header->compression   = load_big_word(&buf);
	header->reserved      = load_big_word(&buf);
	header->text_size     = load_big_dword(&buf);
	header->record_count  = load_big_word(&buf);
	header->record_size   = load_big_word(&buf);
	header->cur_possition = load_big_dword(&buf);

	return header;
}

struct pdb_buf *pdb_load_record(struct pdb *pdb, uint16_t record)
{
	struct pdb_buf *buf;
	uint16_t record_size;

	if (record >= pdb->header.number_of_records) {
		ERR("Invalid record!")
		return NULL;
	}

	/* last one */
	if (record == pdb->header.number_of_records - 1) {
		lseek(pdb->fd, 0, SEEK_END);
		record_size = lseek(pdb->fd, 0, SEEK_CUR) - pdb->record_list[record].record_data_offset;
	} else
		record_size = pdb->record_list[record+1].record_data_offset - pdb->record_list[record].record_data_offset;

	buf = malloc(sizeof(struct pdb_buf) + record_size);
	if (!buf) {
		ERR("Malloc failed :(");
		return NULL;
	}

	lseek(pdb->fd, pdb->record_list[record].record_data_offset, SEEK_SET);

	if(read(pdb->fd, buf->data, record_size) < record_size) {
		ERR("Fread failed :(")
		free(buf);
		return NULL;
	}

	buf->size = record_size;

	return buf;
}

static size_t pdb_lz77_decompressed_size(struct pdb_buf *buf)
{
	uint16_t i = 0;
	size_t size = 0;

	while (i < buf->size) {
		uint8_t b = buf->data[i++];

		switch (b) {
		case 0x00:
		case 0x09 ... 0x7f:
			size++;
		break;
		case 0x01 ... 0x08:
			while (b-- && i < buf->size)
				size++;
		break;
		case 0x80 ... 0xbf: {
			uint16_t data = (b<<8) | buf->data[i++];
			uint8_t length = (data & 0x0007) + 3;

			size+=length;
		} break;
		case 0xc0 ... 0xff:
			size+=2;
		break;
		}
	}

	return size;
}

struct pdb_buf *pdb_decompress_lz77(struct pdb *pdb, struct pdb_buf *buf)
{
	uint16_t i = 0, j = 0;
	char *decomp;
	size_t decomp_size = pdb_lz77_decompressed_size(buf) + 1;

	decomp = malloc(decomp_size);
	if (!decomp) {
		ERR("Malloc failed :(");
		return NULL;
	}

	decomp[decomp_size-1] = 0;

	while (i < buf->size) {
		uint8_t b = buf->data[i++];

		switch (b) {
		case 0x00:
		case 0x09 ... 0x7f:
			decomp[j++] = b;
		break;
		case 0x01 ... 0x08:
			while (b-- && i < buf->size)
				decomp[j++] = buf->data[i++];
		break;
		case 0x80 ... 0xbf: {
			uint16_t data = (b<<8) | buf->data[i++];
			uint16_t distance = (data & 0x3fff)>>3;
			uint8_t length = (data & 0x0007) + 3;
			uint8_t l;

			for (l = 0; l < length; l++, j++)
				decomp[j] = j >= distance ? decomp[j - distance] : 0;
		} break;
		case 0xc0 ... 0xff:
			decomp[j++] = ' ';
			decomp[j++] = b ^ 0x80;
		break;
		}
	}

	size_t utf8_size = lu_to_utf8_size(decomp, pdb->enc);
	struct pdb_buf *ret;

	ret = malloc(sizeof(struct pdb_buf) + utf8_size + 1);
	if (!ret) {
		ERR("Malloc failed :(");
		free(decomp);
		return NULL;
	}

	lu_to_utf8_cpy(decomp, (char*)ret->data, pdb->enc);

	free(decomp);

	ret->size = utf8_size;
	ret->data[utf8_size] = 0;

	return ret;
}
