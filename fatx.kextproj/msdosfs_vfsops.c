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
/* $FreeBSD: src/sys/msdosfs/msdosfs_vfsops.c,v 1.63 2000/05/05 09:58:36 phk Exp $ */
/*	$NetBSD: msdosfs_vfsops.c,v 1.51 1997/11/17 15:36:58 ws Exp $	*/

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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h> 				/* defines ALLPERMS */
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <mach/kmod.h>
#include <kern/thread.h>

#include <miscfs/specfs/specdev.h>

#include "bpb.h"
#include "bootsect.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"

#define MSDOSFS_DFLTBSIZE       4096

extern uid_t console_user;

extern u_int16_t dos2unicode[32];

extern long msdos_secondsWest;	/* In msdosfs_conv.c */

#if 0
MALLOC_DEFINE(M_MSDOSFSMNT, "FATX mount", "FATX mount structure");
static MALLOC_DEFINE(M_MSDOSFSFAT, "FATX FAT", "FATX file allocation table");
#endif

static int	update_mp __P((struct mount *mp, struct msdosfs_args *argp));
static int	mountmsdosfs __P((struct vnode *devvp, struct mount *mp,
				  struct proc *p, struct msdosfs_args *argp));
static int	msdosfs_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
				    struct vnode **, int *, struct ucred **));
static int	msdosfs_mount __P((struct mount *, char *, caddr_t,
				   struct nameidata *, struct proc *));
static int	msdosfs_root __P((struct mount *, struct vnode **));
static int	msdosfs_statfs __P((struct mount *, struct statfs *,
				    struct proc *));
static int	msdosfs_sync __P((struct mount *, int, struct ucred *,
				  struct proc *));
static int	msdosfs_unmount __P((struct mount *, int, struct proc *));
static int	msdosfs_vptofh __P((struct vnode *, struct fid *));

static int	get_root_label(struct mount *mp);

static int
update_mp(mp, argp)
	struct mount *mp;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);

	pmp->pm_gid = argp->gid;
	pmp->pm_uid = argp->uid;
	pmp->pm_mask = argp->mask & ALLPERMS;
	pmp->pm_flags |= argp->flags & MSDOSFSMNT_MNTOPT;
	if (argp->flags & MSDOSFSMNT_SECONDSWEST)
		msdos_secondsWest = argp->secondsWest;

	if (argp->flags & MSDOSFSMNT_LABEL)
		bcopy(argp->label, pmp->pm_label, sizeof(pmp->pm_label));

	return 0;
}

#if 0
#ifndef __FreeBSD__
int
msdosfs_mountroot()
{
	register struct mount *mp;
	struct proc *p = curproc;	/* XXX */
	size_t size;
	int error;
	struct msdosfs_args args;

	if (root_device->dv_class != DV_DISK)
		return (ENODEV);

	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if (bdevvp(rootdev, &rootvp))
		panic("msdosfs_mountroot: can't setup rootvp");

	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &msdosfs_vfsops;
	mp->mnt_flag = 0;
	LIST_INIT(&mp->mnt_vnodelist);

	args.flags = 0;
	args.uid = 0;
	args.gid = 0;
	args.mask = 0777;
        
	if ((error = mountmsdosfs(rootvp, mp, p, &args)) != 0) {
		FREE(mp, M_TEMP);
		return (error);
	}

	if ((error = update_mp(mp, &args)) != 0) {
		(void)msdosfs_unmount(mp, 0, p);
		FREE(mp, M_TEMP);
		return (error);
	}

	if ((error = vfs_lock(mp)) != 0) {
		(void)msdosfs_unmount(mp, 0, p);
		FREE(mp, M_TEMP);
		return (error);
	}

	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_vnodecovered = NULLVP;
	(void) copystr("/", mp->mnt_stat.f_mntonname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)msdosfs_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	return (0);
}
#endif
#endif

/*
 * mp - path - addr in user space of mount point (ie /usr or whatever)
 * data - addr in user space of mount params including the name of the block
 * special file to treat as a filesystem.
 */
static int
msdosfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;	  /* vnode for blk device to mount */
	struct msdosfs_args args; /* will hold data from mount request */
	/* msdosfs specific mount control block */
	struct msdosfsmount *pmp = NULL;
	size_t size;
	int error, flags;
	mode_t accessmode;

	error = copyin(data, (caddr_t)&args, sizeof(struct msdosfs_args));
	if (error)
		return (error);
	if (args.magic != MSDOSFS_ARGSMAGIC)
		args.flags = 0;

	/*
	 * Do any preliminary checks:
	 * a. Make sure we can store passed in args in memory
	 */
    if ((error = copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size)))
        return error;
    bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
    if ((error = copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
			&size)))
        return error;
    bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

    
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		pmp = VFSTOMSDOSFS(mp);
		error = 0;
		if (!(pmp->pm_flags & MSDOSFSMNT_RONLY) && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = vflush(mp, NULLVP, flags);
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			/* not yet implemented */
			error = EOPNOTSUPP;
		if (error)
			return (error);
		if ((pmp->pm_flags & MSDOSFSMNT_RONLY) && (mp->mnt_kern_flag & MNTK_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (p->p_ucred->cr_uid != 0) {
				devvp = pmp->pm_devvp;
				vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
				error = VOP_ACCESS(devvp, VREAD | VWRITE,
						   p->p_ucred, p);
				if (error) {
					VOP_UNLOCK(devvp, 0, p);
					return (error);
				}
				VOP_UNLOCK(devvp, 0, p);
			}
			pmp->pm_flags &= ~MSDOSFSMNT_RONLY;
                        
		}
		if (args.fspec == 0) {
#ifdef	__notyet__		/* doesn't work correctly with current mountd	XXX */
			if (args.flags & MSDOSFSMNT_MNTOPT) {
				pmp->pm_flags &= ~MSDOSFSMNT_MNTOPT;
				pmp->pm_flags |= args.flags & MSDOSFSMNT_MNTOPT;
				if (pmp->pm_flags & MSDOSFSMNT_NOWIN95)
					pmp->pm_flags |= MSDOSFSMNT_SHORTNAME;
			}
#endif
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &pmp->pm_export, &args.export));
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	error = namei(ndp);
	if (error)
		return (error);
	devvp = ndp->ni_vp;
//	NDFREE(ndp, NDF_ONLY_PNBUF);

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (p->p_ucred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p);
		if (error) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp, 0, p);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = mountmsdosfs(devvp, mp, p, &args);
#ifdef MSDOSFS_DEBUG		/* only needed for the printf below */
		pmp = VFSTOMSDOSFS(mp);
#endif
	} else {
		if (devvp != pmp->pm_devvp)
			error = EINVAL;	/* XXX needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}

	error = update_mp(mp, &args);
	if (error) {
		msdosfs_unmount(mp, MNT_FORCE, p);
		return error;
	}

	(void) msdosfs_statfs(mp, &mp->mnt_stat, p);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_mount(): mp %p, pmp %p, inusemap %p\n", mp, pmp, pmp->pm_inusemap);
#endif
	return (0);
}

static int
mountmsdosfs(devvp, mp, p, argp)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp;
	struct buf *bp;
	dev_t dev = devvp->v_rdev;
#if 0
#ifndef __FreeBSD__
	struct partinfo dpart;
	int bsize = 0, dtype = 0, tmp;
#endif
#endif
	struct bootsectorx *bsp;
	u_int32_t blocksize;
	u_int64_t devsize;
	u_int8_t SecPerClust;
	u_long clusters;
	int	ronly, error;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	VOP_UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	VOP_UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	bp  = NULL; /* both used in error_exit */
	pmp = NULL;

#if 0
#ifndef __FreeBSD__
	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		/*
	 	 * We need the disklabel to calculate the size of a FAT entry
		 * later on. Also make sure the partition contains a filesystem
		 * of type FS_MSDOS. This doesn't work for floppies, so we have
		 * to check for them too.
	 	 *
	 	 * At least some parts of the msdos fs driver seem to assume
		 * that the size of a disk block will always be 512 bytes.
		 * Let's check it...
		 */
		error = VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart,
				  FREAD, NOCRED, p);
		if (error)
			goto error_exit;
		tmp   = dpart.part->p_fstype;
		dtype = dpart.disklab->d_type;
		bsize = dpart.disklab->d_secsize;
		if (bsize != 512 || (dtype!=DTYPE_FLOPPY && tmp!=FS_MSDOS)) {
			error = EINVAL;
			goto error_exit;
		}
	}
#endif
#endif
	/*
	 * Read the boot sector of the filesystem, and then check the
	 * boot signature.  If not a dos boot sector then error out.
	 *
	 * NOTE: 2048 is a maximum sector size in current...
	 */
        error = meta_bread(devvp, 0, 2048, NOCRED, &bp);
	if (error)
		goto error_exit;
	bp->b_flags |= B_AGE;
	bsp = (struct bootsectorx *)bp->b_data;


        /*
            * The first four bytes are 'FATX'.
            */
        if (bsp->bsXBootSectSig[0] != XBOOTSIG0
			|| bsp->bsXBootSectSig[1] != XBOOTSIG1
			|| bsp->bsXBootSectSig[2] != XBOOTSIG2
			|| bsp->bsXBootSectSig[3] != XBOOTSIG3)
        {
            error = EINVAL;
            goto error_exit;
        }

	MALLOC(pmp, struct msdosfsmount *, sizeof(*pmp), M_TEMP, M_WAITOK);
	bzero((caddr_t)pmp, sizeof *pmp);
	pmp->pm_mountp = mp;

	/*
	 * Compute several useful quantities from the bootsector
	 * and set values hardcoded for FATX.
	 */
	SecPerClust = getulong(bsp->bsXSectPerClust);
	pmp->pm_BytesPerSec = 512;
	pmp->pm_ResSectors = 0;
	pmp->pm_FATs = getushort(bsp->bsXFATS);
	pmp->pm_RootDirEnts = SecPerClust*8; /* Root dir always takes one cluster */

	/* SecPerClust and FATs should always be 32 and 1 for FATX, respectively */
	if(SecPerClust != 32 || pmp->pm_FATs != 1) {
		error = EINVAL;
		goto error_exit;
	}

	/* Determine device size */
	error = VOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t) &blocksize, FREAD, NOCRED, p);
	if(error)
		goto error_exit;
	error = VOP_IOCTL(devvp, DKIOCGETBLOCKCOUNT, (caddr_t) &devsize, FREAD, NOCRED, p);
	if(error)
		goto error_exit;
	devsize *= blocksize;
	
	/* Make sure the device isn't too large */
	if((devsize/512) > 0xFFFFFFFFULL) {
		error = EFBIG; /* XXX better error code? */
		goto error_exit;
	}
#ifdef MSDOSFS_DEBUG
	printf("mountmsdosfs: device blocksize %u bytes %llu\n", blocksize, devsize);
#endif
	pmp->pm_Sectors = devsize / 512;
	/* If the device requires more than 65,525 clusters, it's FATX32; otherwise it's FATX16
	 * 260 = 8 sectors for bootsector + 256 sectors for a 65525-entry FAT16 */
	if((pmp->pm_Sectors - 260)/SecPerClust >= 65525) {
		pmp->pm_fatmask = FAT32_MASK;
		pmp->pm_fatmult = 4;
		pmp->pm_fatdiv = 1;
	} else {
		pmp->pm_fatmask = FAT16_MASK;
		pmp->pm_fatmult = 2;
		pmp->pm_fatdiv = 1;
	}
	
	pmp->pm_FATsecs = pmp->pm_Sectors/SecPerClust * pmp->pm_fatmult;
	/* Round up to next 4KB boundary if necessary */
	if(pmp->pm_FATsecs & 4095)
		pmp->pm_FATsecs = (pmp->pm_FATsecs & ~4095) + 4096;
	/* Convert from bytes to sectors */
	pmp->pm_FATsecs /= 512;

	/* calculate the ratio of sector size to device block size */
	pmp->pm_BlockSize = blocksize;
	pmp->pm_BlocksPerSec = pmp->pm_BytesPerSec / pmp->pm_BlockSize;

#ifdef MSDOSFS_DEBUG
	printf("mountmsdosfs(): %u sectors, block size %u, FAT%d spanning %u sectors\n",
			pmp->pm_Sectors, pmp->pm_BlockSize, (pmp->pm_fatmask == FAT32_MASK? 32 : 16), pmp->pm_FATsecs);
#endif

	/*
	 * Check a few values (could do some more):
	 * - logical sector size: power of 2, >= block size
	 * - sectors per cluster: power of 2, >= 1
	 * - number of sectors:   >= 1, <= size of partition
	 */
	if (pmp->pm_BytesPerSec < pmp->pm_BlockSize) {
		error = EINVAL;
		goto error_exit;
	}

                /*
                 * Compute the root directory and first cluster as sectors
                 * so that pm_maxcluster will be correct, below.
                 */
	pmp->pm_rootdirblk = 8 + pmp->pm_FATsecs;
	pmp->pm_rootdirsize = SecPerClust;
	pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
                
	/* Change the root directory values to physical (device) blocks */
	pmp->pm_rootdirblk *= pmp->pm_BlocksPerSec;
	pmp->pm_rootdirsize *= pmp->pm_BlocksPerSec;

	pmp->pm_maxcluster = (pmp->pm_Sectors - pmp->pm_firstcluster) / SecPerClust + 1;

	pmp->pm_firstcluster *= pmp->pm_BlocksPerSec;	/* Convert to physical (device) blocks */

        /* Compute number of clusters this FAT could hold based on its total size */
	clusters = pmp->pm_FATsecs * pmp->pm_BytesPerSec;	/* Size of FAT in bytes */
	clusters /= pmp->pm_fatmult;				/* Max number of clusters, rounded down */
        
	if (pmp->pm_maxcluster >= clusters) {
		printf("Warning: number of clusters (%ld) exceeds FAT "
		    "capacity (%ld)\n", pmp->pm_maxcluster + 1, clusters);
		pmp->pm_maxcluster = clusters - 1;
	}


	pmp->pm_fatblocksize = MSDOSFS_DFLTBSIZE;

	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;
	pmp->pm_bnshift = ffs(pmp->pm_BlockSize) - 1;

	/*
	 * Compute mask and shift value for isolating cluster relative byte
	 * offsets and cluster numbers from a file offset.
	 */
	pmp->pm_bpcluster = SecPerClust * pmp->pm_BytesPerSec;
	pmp->pm_crbomask = pmp->pm_bpcluster - 1;
	pmp->pm_cnshift = ffs(pmp->pm_bpcluster) - 1;

	/*
	 * Check for valid cluster size
	 * must be a power of 2
	 */
	if (pmp->pm_bpcluster ^ (1 << pmp->pm_cnshift)) {
		error = EINVAL;
		goto error_exit;
	}

	/* Copy volume label from boot sector into mount point */
	/* FIXME: get volume label for FATX */
#if 0
	{
		struct extboot *extboot;
		int i;
		u_char uc;
		
		/* Start out assuming no label (empty string) */
		pmp->pm_label[0] = '\0';

		if (FAT32(pmp)) {
			extboot = (struct extboot *) bsp->bs710.bsExt;
		} else {
			extboot = (struct extboot *) bsp->bs50.bsExt;
		}
		
		if (extboot->exBootSignature == EXBOOTSIG) {
			/*
			 * Copy the label from the boot sector into the mount point.
			 *
			 * We don't call dos2unicodefn() because it assumes the last three
			 * characters are an extension, and it will put a period before the
			 * extension.
			 */
			for (i=0; i<11; i++) {
				uc = extboot->exVolumeLabel[i];
				if (i==0 && uc == SLOT_E5)
					uc = 0xE5;
				pmp->pm_label[i] = (uc < 0x80 || uc > 0x9F ? uc : dos2unicode[uc - 0x80]);
			}

			/* Remove trailing spaces, add NUL terminator */
			for (i=10; i>=0 && pmp->pm_label[i]==' '; --i)
				;
			pmp->pm_label[i+1] = '\0';
		}
	}
#endif        
	/*
	 * Release the bootsector buffer.
	 */
	brelse(bp);
	bp = NULL;

	/*
	 * Allocate memory for the bitmap of allocated clusters, and then
	 * fill it in.
	 */
	MALLOC(pmp->pm_inusemap, u_int *,
	       ((pmp->pm_maxcluster + N_INUSEBITS - 1) / N_INUSEBITS) *
	        sizeof(*pmp->pm_inusemap), M_TEMP, M_WAITOK);

	/*
	 * fillinusemap() needs pm_devvp.
	 */
	pmp->pm_dev = dev;
	pmp->pm_devvp = devvp;

	/*
	 * Have the inuse map filled in.
	 */
	if ((error = fillinusemap(pmp)) != 0)
		goto error_exit;

	/*
	 * If they want fat updates to be synchronous then let them suffer
	 * the performance degradation in exchange for the on disk copy of
	 * the fat being correct just about all the time.  I suppose this
	 * would be a good thing to turn on if the kernel is still flakey.
	 */
	if (mp->mnt_flag & MNT_SYNCHRONOUS)
		pmp->pm_flags |= MSDOSFSMNT_WAITONFAT;

	/*
	 * Finish up.
	 */
	if (ronly)
		pmp->pm_flags |= MSDOSFSMNT_RONLY;
	else {
		pmp->pm_fmod = 1;
        }
	mp->mnt_data = (qaddr_t) pmp;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_specflags |= SI_MOUNTEDON;

	(void) get_root_label(mp);

	return 0;

error_exit:
	if (bp)
		brelse(bp);
	(void) VOP_CLOSE(devvp, ronly ? FREAD : FREAD | FWRITE, NOCRED, p);
	if (pmp) {
		if (pmp->pm_inusemap)
			FREE(pmp->pm_inusemap, M_TEMP);
		FREE(pmp, M_TEMP);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
msdosfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

/*
 * Unmount the filesystem described by mp.
 */
static int
msdosfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct msdosfsmount *pmp;
	int error, flags;
	int force;

	flags = 0;
	force = 0;
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
		force = 1;
	}

	error = vflush(mp, NULLVP, flags);
	if (error && !force)
		return (error);
	pmp = VFSTOMSDOSFS(mp);

	/* [2753891] If the volume was mounted read/write, mark it clean now */
	if ((pmp->pm_flags & MSDOSFSMNT_RONLY) == 0) {
		error = markvoldirty(pmp, 0);
		if (error && !force)
			return (error);
	}
        
#ifdef MSDOSFS_DEBUG
	{
		struct vnode *vp = pmp->pm_devvp;

		printf("msdosfs_umount(): just before calling VOP_CLOSE()\n");
		printf("flag %08lx, usecount %d, writecount %d, holdcnt %ld\n",
		    vp->v_flag, vp->v_usecount, vp->v_writecount, vp->v_holdcnt);
		printf("id %lu, mount %p, op %p\n",
		    vp->v_id, vp->v_mount, vp->v_op);
		printf("freef %p, freeb %p, mount %p\n",
		    vp->v_freelist.tqe_next, vp->v_freelist.tqe_prev,
		    vp->v_mount);
		printf("cleanblkhd %p, dirtyblkhd %p, numoutput %ld, type %d\n",
		    vp->v_cleanblkhd.lh_first,
		    vp->v_dirtyblkhd.lh_first,
		    vp->v_numoutput, vp->v_type);
		printf("union %p, tag %d, data[0] %08x, data[1] %08x\n",
		    vp->v_socket, vp->v_tag,
		    ((u_int *)vp->v_data)[0],
		    ((u_int *)vp->v_data)[1]);
	}
#endif
	pmp->pm_devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(pmp->pm_devvp,
		    (pmp->pm_flags&MSDOSFSMNT_RONLY) ? FREAD : FREAD | FWRITE,
		    NOCRED, p);
	if (error && !force)
		return(error);
	vrele(pmp->pm_devvp);
	FREE(pmp->pm_inusemap, M_TEMP);
	FREE(pmp, M_TEMP);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (0);
}

static int
msdosfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct denode *ndep;
	int error;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_root(); mp %p, pmp %p\n", mp, pmp);
#endif
	error = deget(pmp, MSDOSFSROOT, MSDOSFSROOT_OFS, &ndep);
	if (error)
		return (error);
	*vpp = DETOV(ndep);
	return (0);
}


/*
 * Do operations associated with quotas
 */
int msdosfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
    return (EOPNOTSUPP);
}


static int
msdosfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct msdosfsmount *pmp;

	pmp = VFSTOMSDOSFS(mp);
	sbp->f_bsize = pmp->pm_bpcluster;
	sbp->f_iosize = pmp->pm_bpcluster;
	sbp->f_blocks = pmp->pm_maxcluster + 1;
	sbp->f_bfree = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_files = pmp->pm_RootDirEnts;			/* XXX */
	sbp->f_ffree = 0;	/* what to put in here? */
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

static int
msdosfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	struct vnode *vp, *nvp;
	struct denode *dep;
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error, allerror = 0;
        int didhold;		/* set if ubc_hold needed to hold the vnode */
        
	/*
	 * If we ever switch to not updating all of the fats all the time,
	 * this would be the place to update them from the first one.
	 */
	if (pmp->pm_fmod != 0) {
		if (pmp->pm_flags & MSDOSFSMNT_RONLY)
			panic("msdosfs_sync: rofs mod");
		else {
			/* update fats here */
		}
	}
	/*
	 * Write back each (modified) denode.
	 */
	simple_lock(&mntvnode_slock);
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;

		simple_lock(&vp->v_interlock);
		nvp = vp->v_mntvnodes.le_next;
		dep = VTODE(vp);
		
		/* If this node is locked or being reclaimed, then skip it. */
		if (vp->v_tag != VT_MSDOSFS || dep == NULL || vp->v_flag & (VXLOCK|VORECLAIM))
		{
			simple_unlock(&vp->v_interlock);
			continue;
		}
		
		if (vp->v_type == VNON ||
		    ((dep->de_flag &
		    (DE_ACCESS | DE_CREATE | DE_UPDATE | DE_MODIFIED)) == 0 &&
		    ( vp->v_dirtyblkhd.lh_first == NULL && !(vp->v_flag & VHASDIRTY))))
		{
			simple_unlock(&vp->v_interlock);
			continue;
		}
		
		simple_unlock(&mntvnode_slock);
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
		if (error) {
			simple_lock(&mntvnode_slock);
			if (error == ENOENT)
				goto loop;
			continue;
		}
                didhold = ubc_hold(vp);		/* Make sure the vnode doesn't go away until we're done flushing it. */
		error = VOP_FSYNC(vp, cred, waitfor, p);
		if (error)
			allerror = error;
		VOP_UNLOCK(vp, 0, p);
                if (didhold)
                    ubc_rele(vp);
		vrele(vp);
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);

	/*
	 * Flush filesystem control info.
	 */
		vn_lock(pmp->pm_devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_FSYNC(pmp->pm_devvp, cred, waitfor, p);
		if (error)
			allerror = error;
		VOP_UNLOCK(pmp->pm_devvp, 0, p);
	return (allerror);
}


static int
msdosfs_vget(mp, ino, vpp)
	struct mount *mp;
	void *ino;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}


static int
msdosfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct defid *defhp = (struct defid *) fhp;
	struct denode *dep;
	int error;

	return (EOPNOTSUPP);

	error = deget(pmp, defhp->defid_dirclust, defhp->defid_dirofs, &dep);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	*vpp = DETOV(dep);
	return (0);
}


static int
msdosfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct denode *dep;
	struct defid *defhp;

	dep = VTODE(vp);
	defhp = (struct defid *)fhp;
	defhp->defid_len = sizeof(struct defid);
	defhp->defid_dirclust = dep->de_dirclust;
	defhp->defid_dirofs = dep->de_diroffset;
	/* defhp->defid_gen = dep->de_gen; */
	return (0);
}

/*
 * Fast-FileSystem only?
 */
static int
msdosfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
     int * name;
     u_int namelen;
     void* oldp;
     size_t * oldlenp;
     void * newp;
     size_t newlen;
     struct proc * p;
{
     return (EOPNOTSUPP);
}

struct vfsops msdosfs_vfsops = {
	msdosfs_mount,
	msdosfs_start,
	msdosfs_unmount,
	msdosfs_root,
	msdosfs_quotactl,
	msdosfs_statfs,
	msdosfs_sync,
	msdosfs_vget,
	msdosfs_fhtovp,
	msdosfs_vptofh,
	msdosfs_init,
	msdosfs_sysctl
};

#if 0
VFS_SET(msdosfs_vfsops, msdos, MOUNT_NFS, 0);
#endif

static char msdosfs_name[MFSNAMELEN] = "fatx";

struct kmod_info_t *msdosfs_kmod_infop;

typedef int (*PFI)();

extern int vfs_opv_numops;

extern struct vnodeopv_desc msdosfs_vnodeop_opv_desc;

__private_extern__ int
fatx_start(struct kmod_info_t *ki, void *data)
{
#pragma unused(data)
	struct vfsconf	*newvfsconf = NULL;
	int	j;
	int	(***opv_desc_vector_p)(void *);
	int	(**opv_desc_vector)(void *);
	struct vnodeopv_entry_desc	*opve_descp; 
	int	error = 0; 
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);
	msdosfs_kmod_infop = ki;
	/*
	 * This routine is responsible for all the initialization that would
	 * ordinarily be done as part of the system startup;
	 */

	MALLOC(newvfsconf, void *, sizeof(struct vfsconf), M_TEMP,
	       M_WAITOK);
	bzero(newvfsconf, sizeof(struct vfsconf));
	newvfsconf->vfc_vfsops = &msdosfs_vfsops;
	strncpy(&newvfsconf->vfc_name[0], msdosfs_name, MFSNAMELEN);
	newvfsconf->vfc_typenum = maxvfsconf++;
	newvfsconf->vfc_refcount = 0;
	newvfsconf->vfc_flags = MNT_LOCAL;
	newvfsconf->vfc_mountroot = NULL;
	newvfsconf->vfc_next = NULL;

	opv_desc_vector_p = msdosfs_vnodeop_opv_desc.opv_desc_vector_p;

	/*
	 * Allocate and init the vector.
	 * Also handle backwards compatibility.
	 */
	/* XXX - shouldn't be M_TEMP */

	MALLOC(*opv_desc_vector_p, PFI *, vfs_opv_numops * sizeof(PFI),
	       M_TEMP, M_WAITOK);
	bzero(*opv_desc_vector_p, vfs_opv_numops*sizeof(PFI));

	opv_desc_vector = *opv_desc_vector_p;

	for (j = 0; msdosfs_vnodeop_opv_desc.opv_desc_ops[j].opve_op; j++) {
		opve_descp = &(msdosfs_vnodeop_opv_desc.opv_desc_ops[j]);

		/*
		 * Sanity check:  is this operation listed
		 * in the list of operations?  We check this
		 * by seeing if its offest is zero.  Since
		 * the default routine should always be listed
		 * first, it should be the only one with a zero
		 * offset.  Any other operation with a zero
		 * offset is probably not listed in
		 * vfs_op_descs, and so is probably an error.
		 *
		 * A panic here means the layer programmer
		 * has committed the all-too common bug
		 * of adding a new operation to the layer's
		 * list of vnode operations but
		 * not adding the operation to the system-wide
		 * list of supported operations.
		 */
		if (opve_descp->opve_op->vdesc_offset == 0 &&
		    opve_descp->opve_op->vdesc_offset != VOFFSET(vop_default)) {
			printf("load_msdosfs: operation %s not listed in %s.\n",
			       opve_descp->opve_op->vdesc_name,
			       "vfs_op_descs");
			panic("load_msdosfs: bad operation");
		}
		/*
		 * Fill in this entry.
		 */
		opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
		    opve_descp->opve_impl;
	}

	/*  
	 * Finally, go back and replace unfilled routines
	 * with their default.  (Sigh, an O(n^3) algorithm.  I
	 * could make it better, but that'd be work, and n is small.)
	 */  
	opv_desc_vector_p = msdosfs_vnodeop_opv_desc.opv_desc_vector_p;

	/*   
	 * Force every operations vector to have a default routine.
	 */  
	opv_desc_vector = *opv_desc_vector_p;
	if (opv_desc_vector[VOFFSET(vop_default)] == NULL)
	    panic("load_msdosfs: operation vector without default routine.");
	for (j = 0; j < vfs_opv_numops; j++)  
		if (opv_desc_vector[j] == NULL)
			opv_desc_vector[j] =
			    opv_desc_vector[VOFFSET(vop_default)];

	/* Ok, vnode vectors are set up, vfs vectors are set up, add it in */
	error = vfsconf_add(newvfsconf);

	if (newvfsconf)
		FREE(newvfsconf, M_TEMP);

	thread_funnel_set(kernel_flock, funnel_state);
	return (error == 0 ? KERN_SUCCESS : KERN_FAILURE);
}
     

__private_extern__ int  
fatx_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	vfsconf_del(msdosfs_name);
	FREE(*msdosfs_vnodeop_opv_desc.opv_desc_vector_p, M_TEMP);  

	thread_funnel_set(kernel_flock, funnel_state);
	return (KERN_SUCCESS);
}   


/*
 * Look through the root directory for a volume label entry.
 * If found, use it to replace the label in the mount point.
 */
static int get_root_label(struct mount *mp)
{
	/* FATX doesn't store volume label in root directory */
	return EEXIST;
}
