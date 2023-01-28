// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2009-2023 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef LIBPDB_H__
#define LIBPDB_H__

#include <stdint.h>
#include <libutf.h>

#define PDB_HEADER_NAME_SIZE 32
#define PDB_HEADER_SIZE 78
#define PDB_RECORD_LIST_SIZE 8
#define PDB_TEXT_HEADER_SIZE 16

/*
 * record_data_offset: offset the data starts
 * record_attrebutes: 0x10 secret record bit
 *                    0x20 record in use
 *                    0x40 dirty record bit
 *                    0x80 delete record on next hotSync
 */
struct pdb_record_list
{
	uint32_t record_data_offset;
	uint8_t  record_attributes;
	uint8_t  unique_id[3];
};

/*
 * name: NULL terminated string used in palm device.
 * file_attributes: 0x0002 = read only
 *                  0x0004 = dirty appInfoArea
 *                  0x0008 = backup this
 *                  0x0010 = install newer over existing file
 *                  0x0020 = force reset
 *                  0x0040 = don't allow copy
 * version: defined by application
 * creation_date: time stamp
 * modification_date: time stamp
 * last_backup_date: time stamp
 * modification_number: ???
 * app_info_area: address in pdb file
 * sort_info_area: address in pdf file
 * database_type: type for palm application not null terminated
 * creator_id: creator for palm application not null terminated
 */
struct pdb_header {
	/* Make enough room for UTF8 conversion */
	char     name[4 * PDB_HEADER_NAME_SIZE + 1];
	uint16_t file_attributes;
	uint16_t version;
	uint32_t creation_date;
	uint32_t modification_date;
	uint32_t last_backup_date;
	uint32_t modification_number;
	uint32_t app_info_area;
	uint32_t sort_info_area;
	char     database_type[5];
	char     creator_id[5];
	uint32_t unique_id_seed;
	uint32_t next_record_list_id;
	uint16_t number_of_records;
};

struct pdb {
	int fd;
	enum lu_enc enc;
	struct pdb_header header;
	struct pdb_record_list *record_list;
	char path[];
};

struct pdb_buf {
	uint16_t size;
	uint8_t data[];
};

/*** PDB TEXt REAd file header ***/

/*
 * This header is stored as the first record in the
 * record list.
 *
 * compression: see PDB_TEXT_COMP_*
 * text_size: uncompressed file size
 * record_count: number_of_records from header - 1
 * record_size: size of record after decompression
 * cur_possition: saved position in text (not supported by all readers)
 */
struct pdb_text_header {
	uint16_t compression;
	uint16_t reserved;
	uint32_t text_size;
	uint16_t record_count;
	uint16_t record_size;
	uint32_t cur_possition;
};

#define PDB_TEXT_COMP_NONE 0x01
#define PDB_TEXT_COMP_LZ77 0x02

/**
 * Opens a pdb file and parses main header.
 *
 * @path A path to the file.
 * @return An initialized pdb handle or NULL in a case of a failure.
 */
struct pdb *pdb_open(const char *path);

/**
 * Closes a pdf file.
 *
 * @pdb A pdb handle.
 * @return Propagates a return value from close().
 */
int pdb_close(struct pdb *handle);

/**
 * @brief Sets text encoding.
 *
 * If set any text is converted into utf8, especially after a call of this
 * function the name in the header has been converted.
 *
 * @pdb An opened pdb.
 * @enc Source encoding for the strings.
 */
void pdb_set_enc(struct pdb *pdb, enum lu_enc enc);

/**
 * Reads a text header from the pdb file.
 *
 */
struct pdb_text_header *pdb_read_text_header(struct pdb *pdb);

struct pdb_buf *pdb_load_record(struct pdb *pdb, uint16_t record);

struct pdb_buf *pdb_decompress_lz77(struct pdb *pdb, struct pdb_buf *buf);

#endif /* LIBPDB_H__ */
