OMI32.EXE and ISOBAR32.EXE are Win32 [1] versions of Optical Media Image and
ISO Boot Archive Remover.  These utilities are part of the SHSUCD suite, which
I'm assuming you're familiar with.  On the odd chance you stumbled across these
Win32 versions without seeing the DOS versions, here's a couple of notes:

* OMI creates an image of a CD- or DVD-ROM.  The CD image is a standard .ISO
  file, which can be read by FileDisk, amongst others.	The DVD image is in a
  custom format that can only be read by SHSUDVHD (in DOS).  However, creating
  the image on an NTFS drive will generate the usual .ISO file.

* ISOBAR extracts the boot image from a bootable CD-ROM or image.  A floppy
  image can be read by FileDisk; use VDK for a hard disk image.

The source has been tested with LCC-Win32 and MinGW (which generated the
binaries).  There shouldn't be (much of) a problem in using other compilers for
ISOBAR, but OMI uses a display library.  The library is available from me, but
the current implementation is designed for MinGW (LCC-Win32 comes with it,
although I've made some improvements).

URLs:

TCCONIO (the display library):
http://www.geocities.com/jadoxa/tcconio/
http://www.geocities.com/SiliconValley/Station/1177/foundry.html

SHSUCD:
http://www.geocities.com/jadoxa/shsucdx/

FileDisk:
http://www.acc.umu.se/~bosse/

VDK:
http://chitchat.at.infoseek.co.jp/vmware/


Jason Hood, 10 June, 2005.


[1] An NT version of Windows is required for all these programs; they will not
    work in 9X.
