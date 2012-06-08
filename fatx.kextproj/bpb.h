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
/* $FreeBSD: src/sys/msdosfs/bpb.h,v 1.7 1999/08/28 00:48:07 peter Exp $ */
/*	$NetBSD: bpb.h,v 1.7 1997/11/17 15:36:24 ws Exp $	*/

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
/* FATX modifications by Joe Groff. */

/*
 * The following structures represent how the bpb's look on disk.  shorts
 * and longs are just character arrays of the appropriate length.  This is
 * because the compiler forces shorts and longs to align on word or
 * halfword boundaries.
 *
 * XXX The little-endian code here assumes that the processor can access
 * 16-bit and 32-bit quantities on byte boundaries.  If this is not true,
 * use the macros for the big-endian case.
 */
#include <machine/endian.h>
#if (BYTE_ORDER == LITTLE_ENDIAN) 			/* && defined(UNALIGNED_ACCESS) */
#define	getushort(x)	*((u_int16_t *)(x))
#define	getulong(x)	*((u_int32_t *)(x))
#define	putushort(p, v)	(*((u_int16_t *)(p)) = (v))
#define	putulong(p, v)	(*((u_int32_t *)(p)) = (v))
#else
#define getushort(x)	(((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8))
#define getulong(x)	(((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8) \
			 + (((u_int8_t *)(x))[2] << 16)	\
			 + (((u_int8_t *)(x))[3] << 24))
#define putushort(p, v)	(((u_int8_t *)(p))[0] = (v),	\
			 ((u_int8_t *)(p))[1] = (v) >> 8)
#define putulong(p, v)	(((u_int8_t *)(p))[0] = (v),	\
			 ((u_int8_t *)(p))[1] = (v) >> 8, \
			 ((u_int8_t *)(p))[2] = (v) >> 16,\
			 ((u_int8_t *)(p))[3] = (v) >> 24)
#endif
