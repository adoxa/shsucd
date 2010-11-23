
				  SHSUCD Suite

			   Copyright Jason Hood 2005

			     Freeware. Version 3-2

		     Derived from SHSUCD 1.4b by John McCoy


    ===========
    Description
    ===========

    The SHSUCD suite is a set of programs  dealing  with  the  CD-ROM.	They
    allow  access  to  the  CD-ROM as a normal drive letter; create an image
    file from the contents of a CD-ROM; access an image file as a drive; and
    test certain CD-ROM functions.


    ========
    Programs
    ========

    The following programs are included in the suite:

	SHSUCDX  v3.02	Provides access to the CD-ROM as a drive (MSCDEX)
	SHSUCDHD v3.01	Simulates a CD-ROM using an image file
	SHSUCDRD v1.00	Simulates a CD-ROM using an image file in memory
	SHSUDVHD v1.00	Simulates a DVD-ROM using multiple image files
	SHSUCDRI v1.00	Creates an image from a CD-ROM in memory
	OMI	 v1.00	Creates an image from a CD-ROM or DVD-ROM
	ISOBAR	 v1.00	Extracts the boot image from a bootable CD-ROM
	CDTEST		Tests the CD-ROM functions (Int2F/AH=15)
	SMARTER 	Patches SMARTDrive 5.02 to cache SHSUCDX


    =======
    SHSUCDX
    =======

    SHSUCDX is a CD-ROM redirector (substitute for MSCDEX).  It differs from
    MSCDEX in that it can be unloaded, it occupies less memory and only sel-
    ected units are mapped.  It can also handle lowercase names and ISO long
    names (where the 8.3 name would be duplicated).  Please see SHSUCDX.TXT.


    ======================================
    SHSUCDHD, SHSUCDRD, SHSUDVHD, SHSUCDRI
    ======================================

    These programs are CD-ROM device drivers that emulate CD- or DVD-ROMs by
    using  image  files (possibly in memory).  The image file can be created
    by OMI, or by any program that can generate a standard .ISO  file  (DVDs
    can only be created by OMI).  Please see IMAGE.TXT.


    ===
    OMI
    ===

    OMI (Optical Media Image) will create an image file from  a  CD-ROM,  or
    multiple  files from a DVD-ROM.  The image is a direct copy of the disc,
    which can then be used by the above programs to access the CD/DVD  with-
    out needing the physical disc.  Please see IMAGE.TXT.


    ======
    ISOBAR
    ======

    ISOBAR (ISO Boot Archive Remover) will display information about a boot-
    able CD-ROM and optionally extract the floppy or hard disk image, or the
    no emulation code.	In multiple-image configurations, only the  initial/
    default image is recognised.

    -----
    Usage
    -----

    Without any options, ISOBAR will display the boot information for the CD
    in	the first CD-ROM drive.  A drive letter (with or without a colon) or
    an image file can be used to display information about another  CD.   To
    actually  extract  the  image, use "-o" followed by the filename to give
    the image.	By default, hard disk images will be extracted in full;  add
    "-d" to just extract the logical drive (ie. the partition).

    -----------
    Information
    -----------

    The boot information displayed is (all numbers are hexadecimal):

	Catalog Sector	Sector containing the El Torito boot catalog
	Platform	CPU required in order to boot
	ID String	String identifying the manufacturer/developer
	Bootable	Indicates the CD is bootable
	Boot Type	The medium emulated to achieve boot
	Load Segment	Memory address to load initial boot image
	System Type	The format of the partition
	Sector Count	The (emulated) sectors to store at Load Segment
	Image Sector	Sector containing the image
	Image Size	Size of the image in bytes (and decimal)

    To convert the CD sector to an image file offset, multiply by 2048.

    ---------
    Exit Code
    ---------

	0	No problems
	1	Unknown/invalid option
	2	Not enough memory
	3	Problem with CD (SHSUCDX not installed, drive is not a
		  CD-ROM, unrecognised format, no disc present)
	4	Unable to create image file
	5	Read/write error


    ======
    CDTEST
    ======

    CDTEST is a simple program to test the CD-ROM functions on interrupt 2F,
    AH=15.   It  also  tests the installation checks using AX=1100.  It will
    only work with SHSUCDX (and only this version,  not  earlier  versions).
    All functions 00 to 0F are tested (ie.  including unsupported functions,
    but excluding 10 device request).  To test 0F directory entry, specify a
    file  name	(which	must  be  a  complete path, including drive letter).
    Since different CDs will yield different results,  the  output  of	each
    function  is  displayed,  rather  than  a "pass/fail" message, so manual
    verification is necessary.


    =======
    SMARTER
    =======

    SMARTER will patch SMARTDrive 5.02 to enable it to cache SHSUCDX.  If no
    file  name is specified, it will try SMARTDRV.EXE in the directory given
    in the "winbootdir" environment variable, or C:\WINDOWS if that  doesn't
    exist.   If the patch is successful, SMARTCDX.EXE will be created in the
    current directory.	Note that SMARTCDX is not capable of caching MSCDEX.
    It	is  recommended  the  SMARTDRV line in CONFIG.SYS or AUTOEXEC.BAT be
    duplicated and commented, modifying the copy to point to SMARTCDX.

    ---------
    Exit Code
    ---------

	0	SMARTCDX.EXE created OK, or help displayed
	1	Could not open SMARTDRV.EXE or specified file
	2	Not enough memory
	3	Unrecognised SMARTDrive version
	4	Could not create SMARTCDX.EXE
	5	Could not write SMARTCDX.EXE


    =========
    Compiling
    =========

    I have used NASM 0.98.39 and Borland C++ 3.1.  Due to my  heavy  use  of
    the  NASM  preprocessor, a 32-bit version is required, with about 6MB of
    free memory (to avoid paging).  I think version .36 may compile, but not
    before  that.   Users  of  other  C  compilers should be able to compile
    SMARTER.C, but the other C programs may need  modifications  (I've  used
    REGPACK  and intr, which seems Borland-specific, so you'll have to split
    REGPACK into REGS and SREGS and use int86x).  Please see the MAKEFILE.

    There's no need to tell me about UPX, but feel free to use it yourself.

    Gzip decompression used by SHSUCDRD is from a modified zlib 1.2.1.	 The
    compiled  library is supplied with the suite, so zlib is not needed, un-
    less you want to use a different compiler.	My modifications to zlib are
    in the SHSUCDRD.TXT patch file (apply with -p1).  It's designed for Bor-
    land; other compilers may require modification (to SHSUCDRD, as well).


    =====
    Files
    =====

    The following files are included in the package:

	README.TXT	This file
	SHSUCDX.TXT	Documentation for SHSUCDX
	IMAGE.TXT	Documentation for all the image programs
	TODO.TXT	List of possible remaining changes
	SHSUCDX.COM	MSCDEX replacement (386+)
	SHSUCDHD.EXE	Use an image file as a CD (386+)
	SHSUCDRD.EXE	Use an image file in memory as a CD (386+)
	SHSUDVHD.EXE	Use image files as a DVD (386+)
	SHSUCDRI.EXE	Create an image file in memory, use it as CD (386+)
	OMI.EXE 	Create an image file from a CD or DVD
	ISOBAR.EXE	Extract the boot image from a bootable CD
	CDTEST.EXE	Test the CD functions (Int2F/AH=15)
	SMARTER.EXE	Patch to allow SMARTDrive 5.02 to cache SHSUCDX
	SHCDX86.COM	MSCDEX replacement (8086)
	SHCDHD86.EXE	Use an image file as a CD (8086)
	SHCDRD86.EXE	Use an image file in memory as a CD (286)
	SHDVHD86.EXE	Use image files as a DVD (8086)
	SHCDRI86.EXE	Create an image file in memory, use it as CD (286)
	SHSUCDX.NSM	NASM source code for SHSUCDX
	SHSUCDHD.NSM	NASM source code for SHSUCDHD
	SHSUCDRD.NSM	NASM source code for SHSUCDRD
	SHSUDVHD.NSM	NASM source code for SHSUDVHD
	SHSUCDRI.NSM	NASM source code for SHSUCDRI
	NASM.MAC	General purpose NASM macros
	UNDOC.MAC	Undocumented DOS and internal structures
	CDROM.MAC	CD-ROM structures
	OMI.C		(Borland) C source code for OMI
	ISOBAR.C	(Borland) C source code for ISOBAR
	CDTEST.C	(Borland) C source code for CDTEST
	SMARTER.C	(Borland) C source code for SMARTER
	MAKEFILE	(Borland) Makefile for the suite
	ZLIBCDRD.LIB	Library used by SHSUCDRD to decompress images
	ZLIBCDRD.TXT	Patches to zlib 1.2.1 to generate above


    =======
    Contact
    =======

    mailto:jadoxa@yahoo.com.au
    mailto:shsucd@yahoogroups.com
    http://shsucdx.adoxa.cjb.net/

    Jason Hood
    11 Buckle Street
    North Rockhampton
    Qld 4701
    Australia


    =========================
    Jason Hood, 31 May, 2005.
