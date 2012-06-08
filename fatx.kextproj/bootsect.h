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
/* $FreeBSD: src/sys/msdosfs/bootsect.h,v 1.7 1999/08/28 00:48:06 peter Exp $ */
/*	$NetBSD: bootsect.h,v 1.9 1997/11/17 15:36:17 ws Exp $	*/

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
/* FATX modifications made by Joe Groff. */

/* "Boot sector" is a bit of a misnomer in the xbox's case since no boot code
   actually resides in the sector, and the structure actually comprises not one
   but eight sectors, but let's keep the name for tradition's sake. */
struct bootsectorx {
	int8_t		bsXBootSectSig[4];  /* Signature 'FATX' */
	u_int8_t	bsXVolumeID[4];		/* Volume ID */
	u_int8_t    bsXSectPerClust[4];	/* Cluster size on this volume (always 32) */
	u_int8_t	bsXFATS[2];			/* Number of FATs (always 1) */
	u_int8_t	bsXReserved[4082];  /* Pad struct to 4096 bytes */
};

#define XBOOTSIG0 'F'
#define XBOOTSIG1 'A'
#define XBOOTSIG2 'T'
#define XBOOTSIG3 'X'
