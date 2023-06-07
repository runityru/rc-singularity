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

void uppercase(char* source,char *dest)
	{
   while ((*dest++ = toupper(*source++)));
	}

int main(void)
	{
#define TEST_COUNT 14

	char *test_domains[TEST_COUNT] = {
		"a.ru",
		"bc.com",
		"def.net",
		"ghij.org",
		"klnmo.info",
		"pqrstu.site",
		"vwxyz01.top",
		"23456789.vip",
		"xn--p1ai.xn--p1ai",
		"ABCDEFGHI.BIZ",
		"JKLMNOPQRS.ONLINE",
		"TUVXYZ-XN--.COM.RU",
		"dfgfdg.gfgfh.ru.com",
		"VFHHGYYHJJBGF.realty"
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

		int len = strlen(test_domains[i]) - 1;
		cd_transform(test_domains[i],len,&tdata);
		tdata.hash = 512;
		cd_encode(&tdata);
		sz = cd_decode(outbuf,&tdata.head.fields,tdata.key_rest);
		if (sz != len || strncmp(outbuf,upper,len))
			printf("Codec test failed, domain %s encoded len %d, result %s len %d\n",upper,len,outbuf,sz), rv = 1;
		}

	return rv;
	}