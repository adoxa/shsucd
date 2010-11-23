# Makefile for the SHSUCD suite.
# Jason Hood, 28 & 30 May, 2005.

# Programs used:
#   NASM 0.98.39
#   ALINK 1.6
#   Borland C++ 3.1 (with TLINK 7.1.30.1)
#   Borland MAKE 4.0


AS = nasm
AFLAGS = -O9
LD = alink
LFLAGS =
CC = bcc
CFLAGS = -1- -O1 -G- -w -d -f- -k-

PROGS = shsucdx.com shsucdhd.exe shsudvhd.exe shsucdri.exe shsucdrd.exe \
	omi.exe isobar.exe cdtest.exe smarter.exe


.nsm.com:
	$(AS) $(AFLAGS) -l$*.lst -o$@ $<
	$(AS) $(AFLAGS) -Di8086 -l$(*:su=)86.lst -o$(*:su=)86.com $<

.nsm.exe:
	$(AS) $(AFLAGS) -l$*.lst -fobj $<
	$(AS) $(AFLAGS) -Di8086 -l$(*:su=)86.lst -fobj -o$(*:su=)86.obj $<
	$(LD) $(LFLAGS) $*
	$(LD) $(LFLAGS) $(*:su=)86

.c.exe:
	$(CC) $(CFLAGS) $<


all: $(PROGS)

shsucdx.com:  shsucdx.nsm
shsucdhd.exe: shsucdhd.nsm
shsudvhd.exe: shsudvhd.nsm
shsucdri.exe: shsucdri.nsm

shsucdrd.exe: shsucdrd.nsm zlibcdrd.lib
	$(AS) $(AFLAGS) -lshsucdrd.lst -fobj shsucdrd.nsm
	$(AS) $(AFLAGS) -Di8086 -lshcdrd86.lst -fobj -oshcdrd86.obj shsucdrd.nsm
	$(LD) $(LFLAGS) shsucdrd zlibcdrd.lib
	$(LD) $(LFLAGS) shcdrd86 zlibcdrd.lib

omi.exe:     omi.c
isobar.exe:  isobar.c
cdtest.exe:  cdtest.c
smarter.exe: smarter.c

clean:
	del *.lst
	del *.obj
	del *.com
	del *.exe
