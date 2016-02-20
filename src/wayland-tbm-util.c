/*
Copyright (C) 2015 Samsung Electronics co., Ltd. All Rights Reserved.

Contact:
      SooChan Lim <sc1.lim@samsung.com>,
      Sangjin Lee <lsj119@samsung.com>,
      Boram Park <boram1288.park@samsung.com>,
      Changyeon Lee <cyeon.lee@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wayland-tbm-int.h"

void
_wayland_tbm_util_get_appname_brief(char *brief)
{
	char delim[] = "/";
	char *token = NULL;
	char temp[255] = {0,};
	char *saveptr = NULL;

	token = strtok_r(brief, delim, &saveptr);

	while (token != NULL) {
		memset(temp, 0x00, 255 * sizeof(char));
		strncpy(temp, token, 254 * sizeof(char));
		token = strtok_r(NULL, delim, &saveptr);
	}

	snprintf(brief, sizeof(temp), "%s", temp);
}

void
_wayland_tbm_util_get_appname_from_pid(long pid, char *str)
{
	FILE *fp;
	int len;
	long app_pid = pid;
	char fn_cmdline[255] = {0,};
	char cmdline[255] = {0,};

	snprintf(fn_cmdline, sizeof(fn_cmdline), "/proc/%ld/cmdline", app_pid);

	fp = fopen(fn_cmdline, "r");
	if (fp == 0) {
		fprintf(stderr, "cannot file open /proc/%ld/cmdline", app_pid);
		return;
	}

	if (!fgets(cmdline, 255, fp)) {
		fprintf(stderr, "fail to get appname for pid(%ld)\n", app_pid);
		fclose(fp);
		return;
	}
	fclose(fp);

	len = strlen(cmdline);
	if (len < 1)
		memset(cmdline, 0x00, 255);
	else
		cmdline[len] = 0;

	snprintf(str, sizeof(cmdline), "%s", cmdline);
}

