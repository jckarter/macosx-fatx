/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* $FreeBSD: src/sys/msdosfs/msdosfs_conv.c,v 1.29 1999/08/28 00:48:08 peter Exp $ */
/*	$NetBSD: msdosfs_conv.c,v 1.25 1997/11/17 15:36:40 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * System include files.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/dirent.h>

/*
 * MSDOSFS include files.
 */
#include "direntry.h"

/*
 * Total number of days that have passed for each month in a regular year.
 */
static u_short regyear[] = {
	31, 59, 90, 120, 151, 181,
	212, 243, 273, 304, 334, 365
};

/*
 * Total number of days that have passed for each month in a leap year.
 */
static u_short leapyear[] = {
	31, 60, 91, 121, 152, 182,
	213, 244, 274, 305, 335, 366
};

/*
 * Variables used to remember parts of the last time conversion.  Maybe we
 * can avoid a full conversion.
 */
static u_long  lasttime;
static u_long  lastday;
static u_short lastddate;
static u_short lastdtime;

static __inline u_int8_t find_lcode __P((u_int16_t code, u_int16_t *u2w));

/*
 * This variable contains the number of seconds that local time is west of GMT
 * It is updated every time an msdosfs volume is mounted.  This value is
 * essentially tz_minuteswest * 60 - (tz_dsttime ? 3600 : 0) based on the
 * timezone returned by gettimeofday() in the mount_msdosfs tool.
 *
 * This has a problem with daylight savings time.  If the current daylight
 * savings time setting does not match the date being converted, then it
 * will be off by one hour.  The only way to properly fix this is to know
 * (inside the kernel) what dates should be adjusted for daylight savings,
 * and which should not.  That would let us return correct GMT for dates
 * throughout the year.  Of course, the GUI would have to do the opposite
 * conversion when converting to local time for display to the user.
 */
long msdos_secondsWest = 0;

/*
 * Convert the unix version of time to dos's idea of time to be used in
 * file timestamps. The passed in unix time is assumed to be in GMT.
 */
void
unix2dostime(tsp, ddp, dtp, dhp)
	struct timespec *tsp;
	u_int16_t *ddp;
	u_int16_t *dtp;
	u_int8_t *dhp;
{
	u_long t;
	u_long days;
	u_long inc;
	u_long year;
	u_long month;
	u_short *months;

	/*
	 * If the time from the last conversion is the same as now, then
	 * skip the computations and use the saved result.
	 */
	t = tsp->tv_sec - msdos_secondsWest;
	t &= ~1;	/* Round down to multiple of 2 seconds */
	if (lasttime != t) {
		lasttime = t;
		lastdtime = (((t / 2) % 30) << DT_2SECONDS_SHIFT)
		    + (((t / 60) % 60) << DT_MINUTES_SHIFT)
		    + (((t / 3600) % 24) << DT_HOURS_SHIFT);

		/*
		 * If the number of days since 1970 is the same as the last
		 * time we did the computation then skip all this leap year
		 * and month stuff.
		 */
		days = t / (24 * 60 * 60);
		if (days != lastday) {
			lastday = days;
			for (year = 1970;; year++) {
				inc = year & 0x03 ? 365 : 366;
				if (days < inc)
					break;
				days -= inc;
			}
			months = year & 0x03 ? regyear : leapyear;
			for (month = 0; days >= months[month]; month++)
				;
			if (month > 0)
				days -= months[month - 1];
			lastddate = ((days + 1) << DD_DAY_SHIFT)
			    + ((month + 1) << DD_MONTH_SHIFT);
			/*
			 * Remember dos's idea of time is relative to 1980.
			 * unix's is relative to 1970.  If somehow we get a
			 * time before 1980 then don't give totally crazy
			 * results.
			 */
			if (year > 1980)
				lastddate += (year - 1980) << DD_YEAR_SHIFT;
		}
	}
	if (dtp)
		*dtp = lastdtime;
	if (dhp)
		*dhp = (tsp->tv_sec & 1) * 100 + tsp->tv_nsec / 10000000;

	*ddp = lastddate;
}

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

static u_short lastdosdate;
static u_long  lastseconds;

/*
 * Convert from dos' idea of time to unix'. This will probably only be
 * called from the stat(), and fstat() system calls and so probably need
 * not be too efficient.
 */
void
dos2unixtime(dd, dt, dh, tsp)
	u_int dd;
	u_int dt;
	u_int dh;
	struct timespec *tsp;
{
	u_long seconds;
	u_long month;
	u_long year;
	u_long days;
	u_short *months;

	if (dd == 0) {
		/*
		 * Uninitialized field, return the epoch.
		 */
		tsp->tv_sec = 0;
		tsp->tv_nsec = 0;
		return;
	}
	seconds = (((dt & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) << 1)
	    + ((dt & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	    + ((dt & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600
	    + dh / 100;
	/*
	 * If the year, month, and day from the last conversion are the
	 * same then use the saved value.
	 */
	if (lastdosdate != dd) {
		lastdosdate = dd;
		days = 0;
		year = (dd & DD_YEAR_MASK) >> DD_YEAR_SHIFT;
		days = year * 365;
		days += year / 4 + 1;	/* add in leap days */
		if ((year & 0x03) == 0)
			days--;		/* if year is a leap year */
		months = year & 0x03 ? regyear : leapyear;
		month = (dd & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
		if (month < 1 || month > 12) {
			printf("dos2unixtime(): month value out of range (%ld)\n",
			    month);
			month = 1;
		}
		if (month > 1)
			days += months[month - 2];
		days += ((dd & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
	tsp->tv_sec = seconds + lastseconds + msdos_secondsWest;
	tsp->tv_nsec = (dh % 100) * 10000000;
}

/* Check the validity of a FATX filename.
 * Must not be longer than 42 characters.
 * Must contain only ASCII characters (0x20-0x7f), no slashes or backslashes.
 * XXX the xbox probably forbids other characters */
int
validate_fatx_name(const u_char *name, size_t len)
{
	size_t i;
#ifdef MSDOSFS_DEBUG
	printf("validate_fatx_name(): Validating name %.*s length %u\n", len, name, len);
#endif
	if(len > 42)
		return ENAMETOOLONG;
	for(i = 0; i < len; ++i) {
		/* Check for invalid characters */
		if(name[i] <  ' ' || name[i] > '\x7f') return EINVAL;
		if(name[i] == '/' || name[i] == '\\') return EINVAL;
	}
	return 0;
}

/* Case-insensitive, length-limited string comparison 
 * XXX Only properly handles ASCII letters A-Z/a-z; case of ANSI/UTF-8 characters is not handled */
int
_strncasecmp(const char *a, const char *b, size_t len)
{
	int ac, bc;
	size_t i;
	for(i = 0; i < len; ++i)
	{
		ac = *a++; bc = *b++;
		if(ac >= 'A' && ac <= 'Z') ac += ('a' - 'A');
		if(bc >= 'A' && bc <= 'Z') bc += ('a' - 'A');
		if(ac < bc) return -1;
		if(ac > bc) return +1;
		if(!ac && !bc) return 0;
	}
	return 0;
}
	