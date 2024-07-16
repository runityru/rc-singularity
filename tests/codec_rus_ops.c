/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "../defines.h"
#include "../codec.h"

void uppercase(char*source,char *dest)
	{
	int i,tlen = 0;
	unsigned char nc,nc2;
	
	for (i = 0; (nc = (unsigned char)source[i]); i++)
		{
		switch(nc)
			{
			case 0xD0:
				switch (nc2 = (unsigned char)source[++i])
					{
					case 0xB0: dest[tlen++] = 0xD0; dest[tlen++] = 0x90; break; // А
					case 0xB1: dest[tlen++] = 0xD0; dest[tlen++] = 0x91; break; // Б
					case 0xB2: dest[tlen++] = 0xD0; dest[tlen++] = 0x92; break; // В
					case 0xB3: dest[tlen++] = 0xD0; dest[tlen++] = 0x93; break; // Г
					case 0xB4: dest[tlen++] = 0xD0; dest[tlen++] = 0x94; break; // Д
					case 0xB5: dest[tlen++] = 0xD0; dest[tlen++] = 0x95; break; // Е
					case 0xB6: dest[tlen++] = 0xD0; dest[tlen++] = 0x96; break; // Ж
					case 0xB7: dest[tlen++] = 0xD0; dest[tlen++] = 0x97; break; // З
					case 0xB8: dest[tlen++] = 0xD0; dest[tlen++] = 0x98; break; // И
					case 0xB9: dest[tlen++] = 0xD0; dest[tlen++] = 0x99; break; // Й
					case 0xBA: dest[tlen++] = 0xD0; dest[tlen++] = 0x9A; break; // К
					case 0xBB: dest[tlen++] = 0xD0; dest[tlen++] = 0x9B; break; // Л
					case 0xBC: dest[tlen++] = 0xD0; dest[tlen++] = 0x9C; break; // М
					case 0xBD: dest[tlen++] = 0xD0; dest[tlen++] = 0x9D; break; // Н
					case 0xBE: dest[tlen++] = 0xD0; dest[tlen++] = 0x9E; break; // О
					case 0xBF: dest[tlen++] = 0xD0; dest[tlen++] = 0x9F; break; // П
					default:
						dest[tlen++] = 0xD0; dest[tlen++] = nc2;
					}
				break;
			case 0xD1:
				switch (nc2 = (unsigned char)source[++i])
					{
					case 0x80: dest[tlen++] = 0xD0; dest[tlen++] = 0xA0; break; // Р
					case 0x81: dest[tlen++] = 0xD0; dest[tlen++] = 0xA1; break; // С
					case 0x82: dest[tlen++] = 0xD0; dest[tlen++] = 0xA2; break; // Т
					case 0x83: dest[tlen++] = 0xD0; dest[tlen++] = 0xA3; break; // У
					case 0x84: dest[tlen++] = 0xD0; dest[tlen++] = 0xA4; break; // Ф
					case 0x85: dest[tlen++] = 0xD0; dest[tlen++] = 0xA5; break; // Х
					case 0x86: dest[tlen++] = 0xD0; dest[tlen++] = 0xA6; break; // Ц
					case 0x87: dest[tlen++] = 0xD0; dest[tlen++] = 0xA7; break; // Ч
					case 0x88: dest[tlen++] = 0xD0; dest[tlen++] = 0xA8; break; // Ш
					case 0x89: dest[tlen++] = 0xD0; dest[tlen++] = 0xA9; break; // Щ
					case 0x8A: dest[tlen++] = 0xD0; dest[tlen++] = 0xAA; break; // Ъ
					case 0x8B: dest[tlen++] = 0xD0; dest[tlen++] = 0xAB; break; // Ы
					case 0x8C: dest[tlen++] = 0xD0; dest[tlen++] = 0xAC; break; // Ь
					case 0x8D: dest[tlen++] = 0xD0; dest[tlen++] = 0xAD; break; // Э
					case 0x8E: dest[tlen++] = 0xD0; dest[tlen++] = 0xAE; break; // Ю
					case 0x8F: dest[tlen++] = 0xD0; dest[tlen++] = 0xAF; break; // Я
					case 0x91: dest[tlen++] = 0xD0; dest[tlen++] = 0x81; break; // Ё
					default:
						dest[tlen++] = 0xD1; dest[tlen++] = nc2;
					}
				break;
			default:
				dest[tlen++] = nc;
			}
		}
   dest[tlen] = 0;
	}

int main(void)
	{
#define TEST_COUNT 17

	char *test_domains[TEST_COUNT] = {
		"00",
		"А",
		"1Б",
		"ВГ",
		"2ДЕ",
		"ЁЖЗ",
		"3ИЙК",
		"ЛМНО",
		"4ПРСТ",
		"УФХЦЧ",
		"5ШЩЬЫъ",
		"ЭЮЯабв",
		"6гдеёжз",
		"ийклмно",
		"7простуф",
		"хцчшщьыъ",
		"эюя890-.ф"
		};

	char outbuf[MAX_KEY_SOURCE + 2];
	char upper[MAX_KEY_SOURCE + 2];
	FTransformData tdata;
	tdata.value_source = NULL;
	tdata.value_size = 0;
	tdata.use_phantom = 0;

	unsigned i;
	int rv = 0;

	for (i = 0; i < TEST_COUNT; i++)
		{
		uppercase(test_domains[i],upper);
		cd_transform(test_domains[i],MAX_KEY_SOURCE,&tdata);
		tdata.hash = 512;
		cd_encode(&tdata);

		int sz = cd_decode(outbuf,&tdata.head.fields,tdata.key_rest);
		outbuf[sz] = 0;
		if(strcmp(outbuf,upper))
			printf("Codec test failed, domain %s result %s\n",upper,outbuf), rv = 1;


		int len = strlen(test_domains[i]);
		if (len <= 2)
			continue;
		cd_transform(test_domains[i],len,&tdata);
		tdata.hash = 512;
		cd_encode(&tdata);
		sz = cd_decode(outbuf,&tdata.head.fields,tdata.key_rest);
		if (sz != len || strncmp(outbuf,upper,len))
			printf("Codec test failed, domain %s encoded len %d, result %s len %d\n",upper,len,outbuf,sz), rv = 1;
		}

	return rv;
	}