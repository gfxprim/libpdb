// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2009-2023 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>
#include <iconv.h>
#include <string.h>
#include "libpdb.h"
#include "libutf.h"

static void print_header(struct pdb_header *header)
{
	time_t time;

	printf("name:                %s\n", header->name);
	printf("file_attributes:     %i\n", header->file_attributes);
	printf("version:             %i\n", header->version);
	time = header->creation_date;
	printf("creation date:       %s", ctime(&time));
	time = header->modification_date;
	printf("modification date:   %s", ctime(&time));
	time = header->last_backup_date;
	printf("last backup date:    %s", ctime(&time));
	printf("modification number: %u\n", header->modification_number);
	printf("app info area:       %u\n", header->app_info_area);
	printf("sort info area:      %u\n", header->sort_info_area);
	printf("database type:       %s\n", header->database_type);
	printf("creator id:          %s\n", header->creator_id);
	printf("unique id seed:      %u\n", header->unique_id_seed);
	printf("next record list id  %u\n", header->next_record_list_id);
	printf("number of records:   %u\n", header->number_of_records);
	printf("\n");
}

static void print_record_lists(struct pdb *pdb)
{
	unsigned int i;

	for (i = 0; i < pdb->header.number_of_records; i++) {
		printf("record offset:     0x%x\n", pdb->record_list[i].record_data_offset);
		printf("record attributes: %u\n", pdb->record_list[i].record_attributes);
		printf("record unique id:  0x%x\n\n", pdb->record_list[i].unique_id[0]<<16|
		                                      pdb->record_list[i].unique_id[1]<<8 |
		                                      pdb->record_list[i].unique_id[2]);

	}
}

static void print_text_header(struct pdb_text_header *header)
{
	printf("compression:   %u\n", header->compression);
	printf("reserved:      %u\n", header->reserved);
	printf("text_size:      %u\n", header->text_size);
	printf("record_count:  %u\n", header->record_count);
	printf("record_size:   %u\n", header->record_size);
	printf("cur_possition: %u\n", header->cur_possition);
	printf("\n");
}

static void print_help(int exit_code)
{
	printf("pdbdump -ehsltd -rnum file(s).pdb\n"\
	       " -e set text encoding (-e ? for list)\n"\
	       " -h print this help\n"\
	       " -s print header    \n"\
	       " -l print record list\n"\
	       " -t print text header\n"\
	       " -rnum print record nr num\n"\
	       " -d decompress record\n");
	exit(exit_code);
}

static void print_raw_buf(struct pdb_buf *buf)
{
	fwrite(buf->data, buf->size, 1, stdout);
}

static void print_encodings(void)
{
	unsigned int i;

	printf("Available input encodings:\n");

	for (i = 0; i < LU_ENC_CNT; i++)
		printf("%s\n", lu_enc_to_name(i));
}

static void set_encoding(const char *enc_name, enum lu_enc *enc)
{
	if (!strcmp(enc_name, "?")) {
		print_encodings();
		exit(0);
	}

	*enc = lu_name_to_enc(enc_name);

	if (*enc == LU_ENC_ERR) {
		print_encodings();
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	struct pdb *pdb;
	struct pdb_text_header *header;
	struct pdb_buf *buf1;
	struct pdb_buf *buf2;
	int opt;
	int show_header = 0;
	int show_text_header = 0;
	int show_record_lists = 0;
	int print_raw_record = 0;
	int decompress_record = 0;
	enum lu_enc enc = LU_NONE;
	uint16_t record_nr;
	int i;

	while ((opt = getopt(argc, argv, "de:hsltr:")) != -1) {
		switch (opt) {
			case 'e':
				set_encoding(optarg, &enc);
			break;
			case 'h':
				print_help(EXIT_SUCCESS);
			break;
			case 's':
				show_header = 1;
			break;
			case 'l':
				show_record_lists = 1;
			break;
			case 't':
				show_text_header = 1;
			break;
			case 'r':
				print_raw_record = 1;
				record_nr = atoi(optarg);
			break;
			case 'd':
				decompress_record = 1;
			break;
			default:
				print_help(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		printf("No input file.\n");
		exit(EXIT_FAILURE);
	}

	for (i = optind; i < argc; i++) {
		pdb = pdb_open(argv[i]);

		if (!pdb) {
			printf("Can't open file `%s'.\n", argv[i]);
			continue;
		}

		pdb_set_enc(pdb, enc);

		if (show_header || show_record_lists || show_text_header)
			printf("%s:\n\n", basename(argv[i]));

		if (show_header) {
			printf("***** pdb header *****\n");
				print_header(&(pdb->header));
		}

		if (show_record_lists)  {
			printf("***** pdb record list *****\n");
				print_record_lists(pdb);
		}

		if (show_text_header) {
			printf("***** pdb text header *****\n");
				header = pdb_read_text_header(pdb);
				print_text_header(header);
				free(header);
		}

		if (print_raw_record) {
			buf1 = pdb_load_record(pdb, record_nr);
			if (buf1 == NULL)
				printf("Failed to open record nr. %u\n", record_nr);
			else {
				if (decompress_record) {
					buf2 = pdb_decompress_lz77(pdb, buf1);
					if (buf2 == NULL) {
						printf("Failed to decompress record nr. %u\n", record_nr);
					} else {
						print_raw_buf(buf2);
						free(buf2);
					}
				} else
					print_raw_buf(buf1);
			}
			free(buf1);
		}

		pdb_close(pdb);
	}

	exit(1);
}
