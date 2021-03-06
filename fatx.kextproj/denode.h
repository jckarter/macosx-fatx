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
/* $FreeBSD: src/sys/msdosfs/denode.h,v 1.20 1999/12/29 04:54:52 peter Exp $ */
/*	$NetBSD: denode.h,v 1.25 1997/11/17 15:36:28 ws Exp $	*/

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

/*
 * This is the pc filesystem specific portion of the vnode structure.
 *
 * To describe a file uniquely the de_dirclust, de_diroffset, and
 * de_StartCluster fields are used.
 *
 * de_dirclust contains the cluster number of the directory cluster
 *	containing the entry for a file or directory.
 * de_diroffset is the index into the cluster for the entry describing
 *	a file or directory.
 * de_StartCluster is the number of the first cluster of the file or directory.
 *
 * Now to describe the quirks of the pc filesystem.
 * - Clusters 0 and 1 are reserved.
 * - The first allocatable cluster is 2.
 * - The root directory is of fixed size and all blocks that make it up
 *   are contiguous.
 * - Cluster 0 refers to the root directory when it is found in the
 *   startcluster field of a directory entry that points to another directory.
 * - Cluster 0 implies a 0 length file when found in the start cluster field
 *   of a directory entry that points to a file.
 * - You can't use the cluster number 0 to derive the address of the root
 *   directory.
 * - Multiple directory entries can point to a directory. The entry in the
 *   parent directory points to a child directory.  Any directories in the
 *   child directory contain a ".." entry that points back to the parent.
 *   The child directory itself contains a "." entry that points to itself.
 * - The root directory does not contain a "." or ".." entry.
 * - Directory entries for directories are never changed once they are created
 *   (except when removed).  The size stays 0, and the last modification time
 *   is never changed.  This is because so many directory entries can point to
 *   the physical clusters that make up a directory.  It would lead to an
 *   update nightmare.
 * - The length field in a directory entry pointing to a directory contains 0
 *   (always).  The only way to find the end of a directory is to follow the
 *   cluster chain until the "last cluster" marker is found.
 *
 * My extensions to make this house of cards work.  These apply only to the in
 * memory copy of the directory entry.
 * - A reference count for each denode will be kept since dos doesn't keep such
 *   things.
 */

/*
 * Internal pseudo-offset for (nonexistent) directory entry for the root
 * dir in the root dir
 */
#define	MSDOSFSROOT_OFS	0x1fffffff

/*
 * The fat cache structure. fc_fsrcn is the filesystem relative cluster
 * number that corresponds to the file relative cluster number in this
 * structure (fc_frcn).
 */
struct fatcache {
	u_long fc_frcn;		/* file relative cluster number */
	u_long fc_fsrcn;	/* filesystem relative cluster number */
};

/*
 * The fat entry cache as it stands helps make extending files a "quick"
 * operation by avoiding having to scan the fat to discover the last
 * cluster of the file. The cache also helps sequential reads by
 * remembering the last cluster read from the file.  This also prevents us
 * from having to rescan the fat to find the next cluster to read.  This
 * cache is probably pretty worthless if a file is opened by multiple
 * processes.
 */
#define	FC_SIZE		2	/* number of entries in the cache */
#define	FC_LASTMAP	0	/* entry the last call to pcbmap() resolved
				 * to */
#define	FC_LASTFC	1	/* entry for the last cluster in the file */

#define	FCE_EMPTY	0xffffffff	/* doesn't represent an actual cluster # */

/*
 * Set a slot in the fat cache.
 */
#define	fc_setcache(dep, slot, frcn, fsrcn) \
        do { \
	(dep)->de_fc[slot].fc_frcn = frcn; \
	(dep)->de_fc[slot].fc_fsrcn = fsrcn; \
        } while(0)

/*
 * This is the in memory variant of a dos directory entry.  It is usually
 * contained within a vnode.
 */
struct denode {
	struct lock__bsd__ de_lock;	/* denode lock >Keep this first< */
	struct denode *de_next;	/* Hash chain forward */
	struct denode **de_prev; /* Hash chain back */
	struct vnode *de_vnode;	/* addr of vnode we are part of */
	struct vnode *de_devvp;	/* vnode of blk dev we live on */
	u_long de_flag;		/* flag bits */
	dev_t de_dev;		/* device where direntry lives */
	u_long de_dirclust;	/* cluster of the directory file containing this entry */
	u_long de_diroffset;	/* offset of this entry in the directory cluster */
	u_long de_fndoffset;	/* offset of found dir entry */
	int de_fndcnt;		/* number of slots before de_fndoffset */
	long de_refcnt;		/* reference count */
	struct msdosfsmount *de_pmp;	/* addr of our mount struct */
	u_char de_Name[43];	/* name, from DOS directory entry */
	u_char de_Attributes;	/* attributes, from directory entry */
	u_short de_CTime;	/* creation time */
	u_short de_CDate;	/* creation date */
	u_short de_ATime;   /* access time */
	u_short de_ADate;	/* access date */
	u_short de_MTime;	/* modification time */
	u_short de_MDate;	/* modification date */
	u_long de_StartCluster; /* starting cluster of file */
	u_long de_FileSize;	/* size of file in bytes */
	struct fatcache de_fc[FC_SIZE];	/* fat cache */
	u_quad_t de_modrev;	/* Revision level for lease. */
	struct msdosfs_lockf *de_lockf; /* Head of byte-level lock list. */
};

/*
 * Values for the de_flag field of the denode.
 */
#define	DE_UPDATE	0x0004	/* Modification time update request */
#define	DE_CREATE	0x0008	/* Creation time update */
#define	DE_ACCESS	0x0010	/* Access time update */
#define	DE_MODIFIED	0x0020	/* Denode has been modified */
#define	DE_RENAME	0x0040	/* Denode is in the process of being renamed */


/*
 * Transfer directory entries between internal and external form.
 * dep is a struct denode * (internal form),
 * dp is a struct direntry * (external form).
 */
#define DE_INTERNALIZE(dep, dp)				\
	(de_internalize_name((dep)->de_Name, (dp)->deName, (dp)->deNameLength), \
	 (dep)->de_Attributes = (dp)->deAttributes,	\
	 (dep)->de_CTime = getushort((dp)->deCTime),	\
	 (dep)->de_CDate = getushort((dp)->deCDate),	\
	 (dep)->de_ATime = getushort((dp)->deATime),	\
	 (dep)->de_ADate = getushort((dp)->deADate),	\
	 (dep)->de_MTime = getushort((dp)->deMTime),	\
	 (dep)->de_MDate = getushort((dp)->deMDate),	\
	 (dep)->de_StartCluster = getulong((dp)->deStartCluster), \
	 (dep)->de_FileSize = getulong((dp)->deFileSize))

#define DE_EXTERNALIZE(dp, dep)					\
	(de_externalize_name((dp)->deName, &(dp)->deNameLength, (dep)->de_Name), \
	 (dp)->deAttributes = (dep)->de_Attributes,	\
	 putushort((dp)->deCTime, (dep)->de_CTime),	\
	 putushort((dp)->deCDate, (dep)->de_CDate),	\
	 putushort((dp)->deATime, (dep)->de_ATime),	\
	 putushort((dp)->deADate, (dep)->de_ADate),	\
	 putushort((dp)->deMTime, (dep)->de_MTime),	\
	 putushort((dp)->deMDate, (dep)->de_MDate),	\
	 putulong((dp)->deStartCluster, (dep)->de_StartCluster), \
	 putulong((dp)->deFileSize,					\
	     ((dep)->de_Attributes & ATTR_DIRECTORY) ? 0 : (dep)->de_FileSize))
/*
 * DE_EXTERNALIZE_ROOT is used to write the root directory's dates to the
 * volume label entry (if any).
 */
#define DE_EXTERNALIZE_ROOT(dp, dep)			\
	(putushort((dp)->deCTime, (dep)->de_CTime),	\
	 putushort((dp)->deCDate, (dep)->de_CDate),	\
	 putushort((dp)->deATime, (dep)->de_ATime),	\
	 putushort((dp)->deADate, (dep)->de_ADate),	\
	 putushort((dp)->deMTime, (dep)->de_MTime),	\
	 putushort((dp)->deMDate, (dep)->de_MDate))

#define	de_forw		de_chain[0]
#define	de_back		de_chain[1]

#ifdef KERNEL

#define	VTODE(vp)	((struct denode *)(vp)->v_data)
#define	DETOV(de)	((de)->de_vnode)

#define	DETIMES(dep, acc, mod, cre) do {								\
	if ((dep)->de_flag & DE_UPDATE) {									\
		(dep)->de_flag |= DE_MODIFIED;									\
		unix2dostime((mod), &(dep)->de_MDate, &(dep)->de_MTime,			\
		    NULL);														\
		(dep)->de_Attributes |= ATTR_ARCHIVE;							\
	}																	\
	if ((dep)->de_flag & DE_ACCESS) {									\
		u_int16_t adate, atime;											\
		unix2dostime((acc), &adate, &atime, NULL);						\
		if (adate != (dep)->de_ADate || atime != (dep)->de_ATime) {		\
			(dep)->de_flag |= DE_MODIFIED;								\
			(dep)->de_ADate = adate;									\
			(dep)->de_ATime = atime;									\
		}																\
	}																	\
	if ((dep)->de_flag & DE_CREATE) {									\
		unix2dostime((cre), &(dep)->de_CDate, &(dep)->de_CTime,	NULL);  \
		    (dep)->de_flag |= DE_MODIFIED;								\
	}																	\
	(dep)->de_flag &= ~(DE_UPDATE | DE_CREATE | DE_ACCESS);				\
} while (0);


static __inline__ void
getnanotime(struct timespec * t_p)
{
    struct timeval tv;

    microtime(&tv);
    t_p->tv_sec = tv.tv_sec;
    t_p->tv_nsec = tv.tv_usec * 1000;
    return;
}


/*
 * This overlays the fid structure (see mount.h)
 */
struct defid {
	u_short defid_len;	/* length of structure */
	u_short defid_pad;	/* force long alignment */

	u_long defid_dirclust;	/* cluster this dir entry came from */
	u_long defid_dirofs;	/* offset of entry within the cluster */
#if 0
	u_long	defid_gen;	/* generation number */
#endif
};

//extern vop_t **msdosfs_vnodeop_p;

int msdosfs_cache_lookup __P((struct vop_lookup_args *));
int msdosfs_inactive __P((struct vop_inactive_args *));
int msdosfs_reclaim __P((struct vop_reclaim_args *));
int msdosfs_blktooff __P((struct vop_blktooff_args *ap));
int msdosfs_offtoblk __P((struct vop_offtoblk_args *ap));
int msdosfs_cmap __P((struct vop_cmap_args *ap));

/*
 * Internal service routine prototypes.
 */
void de_internalize_name(u_char *denodeName, const u_char *direntryName, u_int8_t direntryNameLength);
void de_externalize_name(u_char *direntryName, u_int8_t *direntryNameLength, const u_char *denodeName);
int deget __P((struct msdosfsmount *, u_long, u_long, struct denode **));
int readep __P((struct msdosfsmount *pmp, u_long dirclu, u_long dirofs,  struct buf **bpp, struct direntry **epp));
int readde __P((struct denode *dep, struct buf **bpp, struct direntry **epp));
int deextend __P((struct denode *dep, u_long length, int flags, struct ucred *cred));
int fillinusemap __P((struct msdosfsmount *pmp));
void reinsert __P((struct denode *dep));
int dosdirempty __P((struct denode *dep));
int createde __P((struct denode *dep, struct denode *ddep, struct denode **depp, struct componentname *cnp));
int deupdat __P((struct denode *dep, int waitfor));
int removede __P((struct denode *pdep, struct denode *dep));
int detrunc __P((struct denode *dep, u_long length, int flags, struct ucred *cred, struct proc *p));
int doscheckpath __P(( struct denode *source, struct denode *target));
int findslots __P((struct denode *dep, struct componentname *cnp));
u_long defileid(struct denode *dep);
#endif	/* KERNEL */
