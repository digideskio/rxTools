/*
 * Copyright (C) 2015 The PASTA Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include "fs.h"
#include "hid.h"
#include "lang.h"
#include "padgen.h"
#include "crypto.h"
#include "progress.h"
#include "strings.h"

#define BUF_SIZE 0x10000
#define MOVABLE_SEED_SIZE 0x120

static uint_fast8_t NcchPadgen(NcchInfo *info) {
	uint_fast8_t keyslot;
	uint32_t size = 0;
	size_t i;
	
	if (!info->n_entries ||
		info->n_entries > MAX_PAD_ENTRIES ||
		info->ncch_info_version != 0xF0000003
	) return 0;
	
	for (i = info->n_entries; i--; size += info->entries[i].size_mb);
	statusInit(size, lang(SF_GENERATING), lang(S_NCCH_XORPAD));
	for (i = info->n_entries; i--;) {
		if (info->entries[i].uses7xCrypto >> 8 == 0xDEC0DE) // magic value to manually specify keyslot
			keyslot = info->entries[i].uses7xCrypto & 0x3F;
		else if (info->entries[i].uses7xCrypto == 0xA) // won't work on an Old 3DS
			keyslot = 0x18;
		else if (info->entries[i].uses7xCrypto)
			keyslot = 0x25;
		else
			keyslot = 0x2C;
			
		if (!CreatePad(keyslot, &info->entries[i].CTR, info->entries[i].keyY, info->entries[i].size_mb, info->entries[i].filename, i))
			return 0;
	}

	return 1;
}

static uint_fast8_t SdPadgen(SdInfo *info) {
	File pf;
	const wchar_t *filename;
	const wchar_t *filenames[] = {
		L"0:movable.sed",
		L"2:private/movable.sed",
		L"1:private/movable.sed",
		0
	};
	const wchar_t **pfilename;
	union {
		uint8_t data[MOVABLE_SEED_SIZE];
		uint32_t magic;
	} movable_seed = {0};
	uint32_t size = 0;
	size_t i;

	if (!info->n_entries ||
		info->n_entries > MAX_PAD_ENTRIES
	) return 0;

	for (pfilename = filenames; *pfilename; pfilename++) {
		filename = *pfilename;
		// Load console 0x34 keyY from movable.sed if present on SD card
		if (FileOpen(&pf, filename, 0)) {
			if ((FileRead2(&pf, &movable_seed, MOVABLE_SEED_SIZE) != MOVABLE_SEED_SIZE ||
				movable_seed.magic != 'DEES') &&
				(FileClose(&pf) || 1)
			) return 0;
			FileClose(&pf);
			setup_aeskey(0x34, AES_BIG_INPUT | AES_NORMAL_INPUT, &movable_seed.data[0x110]);
			use_aeskey(0x34);
			break;
		}
	}
	for (i = info->n_entries; i--; size += info->entries[i].size_mb);
	statusInit(size, lang(SF_GENERATING), lang(S_SD_XORPAD));
	for (i = info->n_entries; i--;) {
		if (!CreatePad(0x34, &info->entries[i].CTR, NULL, info->entries[i].size_mb, info->entries[i].filename, i))
			return 0;
	}

	return 1;
}

uint_fast8_t PadGen(wchar_t *filename) {
	File pf;
	union {
		SdInfo sd;
		NcchInfo ncch;
	} info;
	
	if (!filename || !FileOpen(&pf, filename, 0) || (
		FileRead2(&pf, &info, pf.fsize) != pf.fsize &&
		(FileClose(&pf) || 1)
	)) return 0;

	FileClose(&pf);
	return info.ncch.padding == 0xFFFFFFFF ? NcchPadgen(&info.ncch) : SdPadgen(&info.sd);
}

uint_fast8_t CreatePad(uint_fast8_t keyslot, aes_ctr *CTR, uint8_t *keyY, uint32_t size_mb, const char *filename, int index) {
	static const uint8_t zero_buf[AES_BLOCK_SIZE] __attribute__((aligned(AES_BLOCK_SIZE))) = {0};
	File pf;
	uint8_t buf[BUF_SIZE];
	uint32_t size = size_mb << 20;
	wchar_t fname[sizeof(((SdInfoEntry*)0)->filename)];

	if (mbstowcs(fname, filename, sizeof(((SdInfoEntry*)0)->filename)) == (size_t)-1 ||
		!FileOpen(&pf, wcsncmp(fname, L"sdmc:/", 6) == 0 ? fname + 6 : fname, 1)
	) return 0;

	setup_aeskey(keyslot, AES_BIG_INPUT | AES_NORMAL_INPUT, keyY);
	use_aeskey(keyslot);
	aes_ctr ctr __attribute__((aligned(32))) = *CTR;

	for (size_t i = 0; i < size; i += BUF_SIZE) {
		size_t j;
		for (j = 0; j < BUF_SIZE && i + j < size; j += AES_BLOCK_SIZE) {
			set_ctr(AES_BIG_INPUT | AES_NORMAL_INPUT, &ctr);
			aes_decrypt((void*)zero_buf, buf + j, 1, AES_CTR_MODE);
			add_ctr(&ctr, 1);
		}
		FileWrite2(&pf, &buf, j);
		progressSetPos((i + j) >> 20); //progress in MB
		if (GetInput() & keys[KEY_B].mask) return 0;
	}
	progressPinOffset();
	FileClose(&pf);
	return 1;
}
