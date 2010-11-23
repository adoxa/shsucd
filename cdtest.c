/*
  cdtest.c - Test harness for the SHSUCDX Int2F/15 functions.

  Jason Hood, 22 September, 2004.

  Outputs the results of calling Int2F/AH=15,AL=00-0F (ie. including
  unsupported functions, but excluding 10 [device request]). It also
  calls the two redirector installation functions (Int2F/AX=1100:
  stack=DADA for MSCDEX; BX=BABE for SMARTDrive). Since the results
  will vary depending upon the CD present, manual verification is
  necessary. To test 0F (directory entry), pass a filename (complete
  path, including drive letter).

  11 October, 2004:
    added test for read-only function;
    restore state of tilde/read-only.

  17 & 19 November, 2004:
    added test of FCB name function (when filename given);
    include drive entry size.

  26 November, 2004:
    "invalidate" all buffers;
    directory entry filename is now a complete path, with drive.

  28 November, 2004:
    recognise new version of SHSUCDX' SMARTDrive check (changed SMARTDRV_Q,
     display compile-time options).

  5 May, 2005:
    use four characters for the options flag;
    initialise buffers "manually" to make binary smaller;
    added dump function.
*/

#include <stdio.h>
#include <string.h>
#include <dos.h>


struct REGPACK regs;
int num_drives;
int first_drive;


// Test the SHSUCDX installation check.
int install_check( void )
{
  // First, test the standard redirector installation check.
  asm {
	mov	ax, 0xDADA
	push	ax
	mov	ax, 0x1100
	int	0x2f
	pop	ax
  }
  if (_AX == 0xADAD)
  {
    // Now test SHSUCDX SMARTDrive installation check.
    regs.r_ax = 0x1100;
    regs.r_bx = 0xBABE;
    intr( 0x2f, &regs );
    if (regs.r_bx != 0xBABE)
    {
      printf( "SHSUCDX v3 installed: %d drive(s) located at %04X:%04X,"
	       " %d bytes each entry.\n",
	      regs.r_cx, regs.r_es, regs.r_di, regs.r_dx );
      printf( "Compiled with: %s, CD root form%s, High Sierra %ssupported,\n"
	      "(%04X)         Joliet %ssupported, image on CD %ssupported.\n\n",
	      (regs.r_bx &  1) ? "8086" : "386",
	      (regs.r_bx &  2) ? "" : " not used",
	      (regs.r_bx &  4) ? "" : "not ", regs.r_bx,
	      (regs.r_bx &  8) ? "" : "not ",
	      (regs.r_bx & 16) ? "" : "not " );
      return 1;
    }
  }

  puts( "SHSUCDX v3 is not installed." );
  return 0;
}


// Display a hex and character dump.
void dump( char* buf, int len )
{
  int k, n;
  unsigned char* p = (unsigned char*)buf;
  unsigned char* e = p + len;

  while (p < e)
  {
    fputs( "   ", stdout );
    n = (p + 16 <= e) ? 16 : e - p;
    for (k = 0; k < n; ++k)
    {
      printf( " %02x", p[k] );
    }
    for (; k < 17; ++k)
      fputs( "   ", stdout );
    do
    {
      printf( "%c", (*p >= 32) ? *p : '.' );
      ++p;
    } while (--n);
    putchar( '\n' );
  }
}


char* fail_msg = NULL;

int mpx( int func )
{
  regs.r_ax = 0x1500 + func;
  intr( 0x2f, &regs );
  printf( "%02x: ", func );
  if (regs.r_flags & 1)
  {
    if (fail_msg && regs.r_ax == 1)
      printf( "%s not supported.\n", fail_msg );
    else
      printf( "failed, error %d (%02xh).\n", regs.r_ax, regs.r_ax );
  }
  return !(regs.r_flags & 1);
}

// 00h: Get number of drive letters (installation check)
//  In: BX = 0
// Out: BX = number of CD-ROM drive letters used
//	CX = starting drive letter (0=A:)
void test00( void )
{
  regs.r_bx = 0;
  if (mpx( 0x00 ))
  {
    printf( "%d drive(s), first drive is %c:.\n",
	    regs.r_bx, regs.r_cx + 'A' );
    num_drives = regs.r_bx;
    first_drive = regs.r_cx;
  }
}

// 01h:  Get drive device list
//  In:  ES:BX -> buffer to hold drive letter list (5 bytes per drive)
// Out:  buffer filled:
//	   BYTE  subunit number in driver
//	   DWORD address of device driver header
void test01( void )
{
  struct
  {
    char subunit;
    void far* header;
  }
  buf[32];
  buf[0].subunit = 0;
  buf[0].header  = MK_FP( 0xdead, 0xbeef );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  if (mpx( 0x01 ))
  {
    int j;
    puts( "subunit header" );
    for (j = 0; j < num_drives; ++j)
      printf( "       %d    %Fp\n", buf[j].subunit, buf[j].header );
  }
}

// 02h:  Get copyright file name
// 03h:  Get abstract file name
// 04h:  Get biblio file name
//  In:  ES:BX -> 38-byte buffer for name of respective file
//	    CX	= drive number (0=A:)
// Out:  CF set if drive is not a CD-ROM drive
//	    AX = 000Fh (invalid drive)
//	 CF clear if successful
void test02( void )
{
  char buf[38];
  strcpy( buf, "failed" );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = first_drive;
  if (mpx( 0x02 ))
    printf( "Copyright     file name: \"%s\"\n", buf );
}

void test03( void )
{
  char buf[38];
  strcpy( buf, "failed" );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = first_drive;
  if (mpx( 0x03 ))
    printf( "Abstract      file name: \"%s\"\n", buf );
}

void test04( void )
{
  char buf[38];
  strcpy( buf, "failed" );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = first_drive;
  if (mpx( 0x04 ))
    printf( "Bibliographic file name: \"%s\"\n", buf );
}

// 05h:  Read VTOC
//  In:  ES:BX -> 2048-byte buffer
//	    CX	= drive number (0=A:)
//	    DX	= sector index (0=first volume descriptor,1=second,...)
// Out:  CF set on error
//	    AX = error code (15=invalid drive,21=not ready)
//	 CF clear if successful
//	    AX = volume descriptor type (1=standard,FFh=terminator,0=other)
void test05( void )
{
  char buf[2048];
  strcpy( buf, "failed" );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = first_drive;
  regs.r_dx = 0;
  if (mpx( 0x05 ))
  {
    printf( "volume descriptor type: %s\n",
	    (regs.r_ax == 1)	? "standard" :
	    (regs.r_ax == 0xff) ? "terminator" : "other" );
    dump( buf, 16 );
  }
}

// 08h:  Absolute disk read
//  In:  ES:BX -> buffer
//	    CX	= drive number (0=A:)
//	 SI:DI	= starting sector number
//	    DX	= number of sectors to read
// Out:  CF set on error
//	    AL = error code (0Fh invalid drive,15h not ready)
//	 CF clear if successful
void test08( void )
{
  char buf[2048];
  strcpy( buf, "failed" );

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = first_drive;
  regs.r_si = 0x0000;
  regs.r_di = 0x0010;
  regs.r_dx = 1;
  if (mpx( 0x08 ))
  {
    fputs( "sector 16:\n", stdout );
    dump( buf, 16 );
  }
}

// 0Bh:  CDROM check
//  In:  CX = drive number (0=A:)
// Out:  BX = ADADh if MSCDEX.EXE installed
//	 AX = support status
//	   0000h if drive not supported
//	   nonzero if supported
void test0b( void )
{
  regs.r_cx = 0;
  if (mpx( 0x0b ))
  {
    if (regs.r_bx != 0xADAD)
    {
      printf( "wrong BX value: %04x\n", regs.r_bx );
      return;
    }
    printf( "A: is %ssupported.\n", (regs.r_ax) ? "" : "not " );
  }
  regs.r_cx = first_drive;
  if (mpx( 0x0b ))
    printf( "%c: is %ssupported.\n",
	    first_drive + 'A', (regs.r_ax) ? "" : "not " );
}

// 0Ch:  MSCDEX version
//  In:  BX = 0000h
// Out:  BH = major version
//	 BL = minor version
void test0c( void )
{
  regs.r_bx = 0;
  if (mpx( 0x0c ))
    printf( "version %d.%02d\n", regs.r_bx >> 8, regs.r_bx & 255 );
}

// 0Dh:  Drive letters
//  In:  ES:BX -> buffer for drive letter list (1 byte per drive)
// Out:  buffer filled with drive numbers (0=A:). Each byte corresponds
//	  to the drive in the same position for function 1501h
void test0d( void )
{
  char buf[32];
  buf[0] = '?' - 'A';

  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  if (mpx( 0x0d ))
  {
    int j;
    fputs( "drive letters: ", stdout );
    for (j = 0; j < num_drives; ++j)
      putchar( buf[j] + 'A' );
    putchar( '\n' );
  }
}

// 0Fh:  Get Directory Entry
//  In:  CL = drive number (0=A:)
//	 CH bit 0 = copy flag
//		      clear if direct copy
//		      set if copy removes ISO/High Sierra diffs
//	 ES:BX -> ASCIZ path name
//	 SI:DI -> buffer for directory entry
//		    must be 255 bytes for direct copy, 285 bytes for canonical
// Out:  CF set on error
//	    AX = error code
//	 CF clear if successful
//	    AX = disk format (0=High Sierra,1=ISO 9660)
//
//  In:  CL = 0ffh
//	   CH = function
//		0 get tilde usage in AX
//		1 set tilde usage: BX = 0 no tildes, anything else to use tildes
//		2 get read-only attribute in AX
//		3 set read-only attribute: BX = 0 not ro, anything else ro
//	 CL = 0feh
//	   ES:SI -> pointer to directory entry
//	   ES:DI -> 11-byte buffer for FCB name
//	      DX  = alias number (0 for none)
void test0f( void )
{
  int tildes, ro;

  regs.r_cx = 0x0100 + first_drive;
  fail_msg = "canonical entry";
  mpx( 0x0f );
  fail_msg = NULL;

  regs.r_cx = 0x00ff;
  regs.r_ax = 0x150f;
  intr( 0x2f, &regs );
  tildes = regs.r_ax;
  regs.r_cx = 0x02ff;
  regs.r_ax = 0x150f;
  intr( 0x2f, &regs );
  ro = regs.r_ax;

  regs.r_cx = 0x01ff;
  regs.r_bx = 1;
  if (mpx( 0x0f ))
  {
    puts( "tildes should have been turned on." );
    regs.r_cx = 0x00ff;
    if (mpx( 0x0f ))
      printf( "tildes are %s.\n", (regs.r_ax) ? "on" : "off" );
  }
  regs.r_cx = 0x01ff;
  regs.r_bx = 0;
  if (mpx( 0x0f ))
  {
    puts( "tildes should have been turned off." );
    regs.r_cx = 0x00ff;
    if (mpx( 0x0f ))
      printf( "tildes are %s.\n", (regs.r_ax) ? "on" : "off" );
  }

  regs.r_cx = 0x03ff;
  regs.r_bx = 1;
  if (mpx( 0x0f ))
  {
    puts( "read-only attribute should have been turned on." );
    regs.r_cx = 0x02ff;
    if (mpx( 0x0f ))
      printf( "read-only attribute is %s.\n", (regs.r_ax) ? "on" : "off" );
  }
  regs.r_cx = 0x03ff;
  regs.r_bx = 0;
  if (mpx( 0x0f ))
  {
    puts( "read-only attribute should have been turned off." );
    regs.r_cx = 0x02ff;
    if (mpx( 0x0f ))
      printf( "read-only attribute is %s.\n", (regs.r_ax) ? "on" : "off" );
  }

  regs.r_cx = 0x01ff;
  regs.r_bx = tildes;
  regs.r_ax = 0x150f;
  intr( 0x2f, &regs );
  regs.r_cx = 0x03ff;
  regs.r_bx = ro;
  regs.r_ax = 0x150f;
  intr( 0x2f, &regs );
}

void dir_ent( char* name )
{
  char buf[255];
  char fcb[11];
  strcpy( buf, "failed" );
  strcpy( fcb, "failed" );

  regs.r_ax = 0x1500;
  intr( 0x2f, &regs );
  regs.r_cx = (*name | 0x20) - 'a';
  regs.r_es = FP_SEG( name );
  regs.r_bx = FP_OFF( name ) + 2;
  regs.r_si = FP_SEG( buf );
  regs.r_di = FP_OFF( buf );
  if (mpx( 0x0f ))
  {
    printf( "%s directory entry for \"%s\":\n", (regs.r_ax)?"ISO":"HS", name );
    dump( buf, buf[0] );
    regs.r_es = FP_SEG( buf );
    regs.r_si = FP_OFF( buf );
    regs.r_di = FP_OFF( fcb );
    regs.r_dx = 2;
    regs.r_cx = 0xfe;
    if (mpx( 0x0f ))
      printf( "FCB name: \"%.11s\"\n", fcb );
  }
}


int main( int argc, char* argv[] )
{
  if (argc > 1)
  {
    if (argv[1][0] == '/' && argv[1][1] == '?')
    {
      puts(
"Simple test harness for SHSUCDX CD-ROM functions (Int2F/AH=15).\n"
"Also tests installation checks (Int2F/AX=1100, stack=DADA and BX=BABE).\n"
"Tests all Int2F/AH=15 functions 00 to 0F (ie. it includes unsupported\n"
"functions, but excludes 10 device request). To test 0F directory entry\n"
"and FCB name generation (assuming entry number 2 if tildes are on)\n"
"specify a filename (complete path, including drive).\n"
"Since results differ according to CD, manual verification is necessary.\n"
"\n"
"Jason Hood <jadoxa@yahoo.com.au>, 5 May, 2005."
      );
      return 0;
    }
    dir_ent( argv[1] );
  }
  else if (install_check())
  {
    test00();
    test01();
    test02();
    test03();
    test04();
    test05();
    fail_msg = "debug on";
    mpx( 0x06 );
    fail_msg = "debug off";
    mpx( 0x07 );
    fail_msg = NULL;
    test08();
    fail_msg = "absolute write";
    mpx( 0x09 );
    fail_msg = "reserved";
    mpx( 0x0a );
    fail_msg = NULL;
    test0b();
    test0c();
    test0d();
    fail_msg = "volume descriptor preference";
    mpx( 0x0e );
    fail_msg = NULL;
    test0f();
  }

  return 0;
}
