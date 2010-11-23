/*
 * udf.c: Test program to read the root directory of a UDF-formatted disc.
 *
 * Jason Hood, 29 December, 2005.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <dos.h>
#include <alloc.h>
#include <string.h>

typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;
typedef unsigned int  UINT;

void CDReadLong( UINT SectorCount, DWORD StartSector );

char  buf[2048];
int   CD = -1;
DWORD MVDS_Location, MVDS_Length;
DWORD Partition_Location;
DWORD FSD_Location;
DWORD RootDir_Location, RootDir_Length;

int main( int argc, char* argv[] )
{
  int	i;
  char* isofile = NULL;
  DWORD n;
  UINT	len;
  union REGS regs;

  if (argc > 1)
    isofile = argv[1];

  if (!isofile)
  {
    regs.x.ax = 0x1500;
    regs.x.bx = 0;
    int86( 0x2f, &regs, &regs );
    CD = regs.x.cx;
  }
  else if (isofile[1] == '\0' || (isofile[1] == ':' && isofile[2] == '\0'))
  {
    CD = (*isofile | 0x20) - 'a';
    regs.x.ax = 0x150B;
    regs.x.cx = CD;
    int86( 0x2f, &regs, &regs );
    if (regs.x.bx != 0xADAD)
      regs.x.bx = 0;
    else if (regs.x.ax == 0)
    {
      fprintf( stderr, "ERROR: %c: is not a CD-ROM drive.\n", CD + 'A' );
      return 1;
    }
  }
  else
  {
    puts( "udf [drive]\n"
	  "\n"
	  "List the contents of the root directory of CD-ROM drive." );
    return 1;
  }

  if (regs.x.bx == 0)
  {
    fputs( "ERROR: No CD-ROM drives assigned.\n", stderr );
    return 1;
  }

  // Read Volume Recognition Sequence, when I know how

  // Read Anchor Volume Descriptor Pointer
  CDReadLong( 1, 0x100 );
  // Should check checksum & CRC, but just use tag location
  if (*(DWORD*)(buf + 0x0c) != 0x100)
  {
    CDReadLong( 1, 0x200 );
    if (*(DWORD*)(buf + 0x0c) != 0x200)
    {
      puts( "Did not find AVDP." );
      return 1;
    }
  }

  // Read location & length of the Main Volume Descriptor Sequence
  MVDS_Location = *(DWORD*)(buf + 36);
  MVDS_Length	= *(DWORD*)(buf + 32);

  // Scan for Partition Descriptor
  do
  {
    CDReadLong( 1, MVDS_Location++ );
  } while (*buf != 5 && --MVDS_Length);

  if (*buf != 5)
  {
    puts( "Unable to locate Partition Descriptor." );
    return 1;
  }

  Partition_Location = *(DWORD*)(buf + 0xbc);

  // Scan for Logical Volume Descriptor (assume after PD)
  do
  {
    CDReadLong( 1, MVDS_Location++ );
  } while (*buf != 6 && --MVDS_Length);

  if (*buf != 6)
  {
    puts( "Unable to locate Logical Volume Descriptor." );
    return 1;
  }

  FSD_Location = *(DWORD*)(buf + 0xfc);

  CDReadLong( 1, Partition_Location + FSD_Location );

  RootDir_Location = *(DWORD*)(buf + 0x194);
  CDReadLong( 1, Partition_Location + RootDir_Location );

  RootDir_Location = *(DWORD*)(buf + 0xa8);
  RootDir_Length   = *(DWORD*)(buf + 0xb0 + RootDir_Location);
  RootDir_Location = *(DWORD*)(buf + 0xb0 + RootDir_Location + 4);
  CDReadLong( 1, Partition_Location + RootDir_Location );

  // Read the root directory
  isofile = buf;
  do
  {

    putchar( (isofile[0x12] & 1) ? 'h' : '-' );
    putchar( (isofile[0x12] & 2) ? 'd' : '-' );
    putchar( (isofile[0x12] & 4) ? 'd' : '-' );
    putchar( (isofile[0x12] & 8) ? 'p' : '-' );
    putchar( ' ' );
    n = isofile[0x13];
    len = *(WORD*)(isofile + 0x24);
    if (n == 0)
    {
      putchar( '.' );
      putchar( '.' );
    }
    else
    {
      for (i = 1; i < n; ++i)
	putchar( isofile[0x26 + len + i] );
    }
    putchar( '\n' );

    len = 0x10 + *(WORD*)(isofile + 0x0a);
    isofile += len;
    RootDir_Length -= len;
  } while ((int)RootDir_Length > 0);

  return 0;
}


void CDReadLong( UINT SectorCount, DWORD StartSector )
{
  struct REGPACK regs;

  regs.r_ax = 0x1508;
  regs.r_es = FP_SEG( buf );
  regs.r_bx = FP_OFF( buf );
  regs.r_cx = CD;
  regs.r_si = (WORD)(StartSector >> 16);
  regs.r_di = (WORD)StartSector;
  regs.r_dx = SectorCount;
  intr( 0x2f, &regs );

  if (regs.r_flags & 1) 	// carry flag set if error
  {
    fputs( "Read error!\n", stderr );
    exit( 1 );
  }
}
