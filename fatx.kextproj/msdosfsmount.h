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
/* $FreeBSD: src/sys/msdosfs/msdosfsmount.h,v 1.20 2000/01/27 14:43:07 nyan Exp $ */
/*	$NetBSD: msdosfsmount.h,v 1.17 1997/11/17 15:37:07 ws Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
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
/* FATX modifications by Joe Groff. */

#ifndef _MSDOSFS_MSDOSFSMOUNT_H_
#define	_MSDOSFS_MSDOSFSMOUNT_H_

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MSDOSFSMNT);
#endif

/*
 * Layout of the mount control block for a msdos file system.
 */
struct msdosfsmount {
	struct mount *pm_mountp;/* vfs mount struct for this fs */
	dev_t pm_dev;		/* block special device mounted */
	uid_t pm_uid;		/* uid to set as owner of the files */
	gid_t pm_gid;		/* gid to set as owner of the files */
	mode_t pm_mask;		/* mask to and with file protection bits */
	struct vnode *pm_devvp;	/* vnode for block device mntd */
	struct bootsectorx pm_bootsect;	/* Boot sector for this fs */
	u_long pm_Sectors;		/* Number of sectors this fs spans */
	u_long pm_BytesPerSec;  /* Number of bytes per sector - always 512 */
	u_long pm_ResSectors;   /* Number of reserved sectors - always 0 */
	u_long pm_RootDirEnts;  /* Maximum number of entries in root dir - always 256 */
	u_long pm_BlockSize;	/* device's block size */
	u_long pm_BlocksPerSec;	/* pm_BytesPerSec divided by pm_BlockSize (device blocks per DOS sector) - always 1 */
	u_long pm_FATsecs;		/* number of sectors per FAT */
	u_long pm_FATs;			/* number of FATs */
	u_long pm_rootdirblk;	/* block # of root directory number */
	u_long pm_rootdirsize;	/* number of physical (device) blocks in root directory - always 32 sectors */
	u_long pm_firstcluster;	/* sector number of first cluster (relative to start of volume) */
	u_long pm_maxcluster;	/* maximum cluster number */
	u_long pm_freeclustercount;	/* number of free clusters */
	u_long pm_cnshift;	/* shift file offset right this amount to get a cluster number */
	u_long pm_crbomask;	/* and a file offset with this mask to get cluster rel offset */
	u_long pm_bnshift;	/* shift amount to convert between bytes and physical (device) blocks */
	u_long pm_bpcluster;	/* bytes per cluster */
	u_long pm_fmod;		/* ~0 if fs is modified, this can rollover to 0	*/
	u_long pm_fatblocksize;	/* size of fat blocks in bytes */
	u_long pm_fatblocksec;	/* size of fat blocks in sectors */
	u_long pm_fatmask;	/* mask to use for fat numbers */
	u_long pm_fsinfo;	/* fsinfo block number - always none (0) for fatx */
	u_long pm_nxtfree;	/* next free cluster in fsinfo block */
	u_int pm_fatmult;	/* these 2 values are used in fat */
	u_int pm_fatdiv;	/*	offset computation */
	u_int pm_curfat;	/* current fat - always 0 */
	u_int *pm_inusemap;	/* ptr to bitmap of in-use clusters */
	u_int pm_flags;		/* see below */
	struct netexport pm_export;	/* export information */
	u_int8_t  pm_ul[128];   /* Local upper->lower table */
	u_int8_t  pm_lu[128];   /* Local lower->upper table */
	u_int8_t  pm_d2u[128];  /* DOS->local table */
	u_int8_t  pm_u2d[128];  /* Local->DOS table */
	u_int8_t pm_label[64];	/* Volume name/label */
};
/* Byte offset in FAT on filesystem pmp, cluster cn */
#define	FATOFS(pmp, cn)	((cn) * (pmp)->pm_fatmult / (pmp)->pm_fatdiv)

#define	VFSTOMSDOSFS(mp)	((struct msdosfsmount *)mp->mnt_data)

/* Number of bits in one pm_inusemap item: */
#define	N_INUSEBITS	(8 * sizeof(u_int))

/*
 * Convert pointer to buffer -> pointer to direntry
 */
#define	bptoep(pmp, bp, dirofs) \
	((struct direntry *)(((bp)->b_data)	\
	 + ((dirofs) & (pmp)->pm_crbomask)))

/*
 * Convert number of blocks to number of clusters
 */
#define	de_bn2cn(pmp, bn) \
	((bn) >> ((pmp)->pm_cnshift - (pmp)->pm_bnshift))

/*
 * Convert number of clusters to number of blocks
 */
#define	de_cn2bn(pmp, cn) \
	((cn) << ((pmp)->pm_cnshift - (pmp)->pm_bnshift))

/*
 * Convert file offset to number of clusters
 */
#define de_cluster(pmp, off) \
	((off) >> (pmp)->pm_cnshift)

/*
 * Clusters required to hold size bytes
 */
#define	de_clcount(pmp, size) \
	(((size) + (pmp)->pm_bpcluster - 1) >> (pmp)->pm_cnshift)

/*
 * Convert file offset to first block number of cluster containing offset.
 */
#define de_blk(pmp, off) \
	(de_cn2bn(pmp, de_cluster((pmp), (off))))

/*
 * Convert number of clusters to number of bytes
 */
#define	de_cn2off(pmp, cn) \
	((cn) << (pmp)->pm_cnshift)

/*
 * Convert number of blocks to number of bytes
 */
#define	de_bn2off(pmp, bn) \
	((bn) << (pmp)->pm_bnshift)
/*
 * Map a cluster number into a filesystem relative block number.
 */
#define	cntobn(pmp, cn) \
	(de_cn2bn((pmp), (cn)-CLUST_FIRST) + (pmp)->pm_firstcluster)

/*
 * Calculate block number for directory entry in root dir, offset dirofs
 */
#define	roottobn(pmp, dirofs) \
	(de_blk((pmp), (dirofs)) + (pmp)->pm_rootdirblk)

/*
 * Calculate block number for directory entry at cluster dirclu, offset
 * dirofs
 */
#define	detobn(pmp, dirclu, dirofs) \
	((dirclu) == MSDOSFSROOT \
	 ? roottobn((pmp), (dirofs)) \
	 : cntobn((pmp), (dirclu)))

int msdosfs_init __P((struct vfsconf *vfsp));
int msdosfs_mountroot __P((void));
uid_t get_pmuid(struct msdosfsmount *pmp, uid_t current_user);

#endif /* KERNEL */

/*
 *  Arguments to mount MSDOS filesystems.
 */
struct msdosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export;	/* network export information */
	uid_t	uid;		/* uid that owns msdosfs files */
	gid_t	gid;		/* gid that owns msdosfs files */
	mode_t	mask;		/* mask to be applied for msdosfs perms */
	int	flags;		/* see below */
	int magic;		/* version number */
	u_int8_t  ul[128];      /* Local upper->lower table */
	u_int8_t  lu[128];      /* Local lower->upper table */
	u_int8_t  d2u[128];     /* DOS->local table */
	u_int8_t  u2d[128];     /* Local->DOS table */
	long	secondsWest;	/* for GMT<->local time conversions */
	u_int8_t  label[64];	/* Volume label in UTF-8 */
};

/*
 * Msdosfs mount options:
 */
#define MSDOSFSMNT_SECONDSWEST	0x40	/* Use secondsWest for GMT<->local time conversion */
#define MSDOSFSMNT_LABEL		0x80	/* UTF-8 volume label in label[]; deprecated */

/* All flags above: */
#define	MSDOSFSMNT_MNTOPT \
	(MSDOSFSMNT_SECONDSWEST|MSDOSFSMNT_LABEL)
#define	MSDOSFSMNT_RONLY	0x80000000	/* mounted read-only	*/
#define	MSDOSFSMNT_WAITONFAT	0x40000000	/* mounted synchronous	*/

#define MSDOSFS_ARGSMAGIC	0x46415458

#endif /* !_MSDOSFS_MSDOSFSMOUNT_H_ */
