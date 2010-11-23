/*
 * omi.c: Create an image of a CD or DVD (Optical Media Image).
 *
 * Jason Hood, 16 & 18 March, 5 & 8 May, 2005.
 *
 * A program to create a disk image from the contents of a CD/DVD. Since DOS
 * is practically limited to 2Gi files and DVDs can be more than that, use
 * multiple files, each of 512Mi. In addition, to simplify the driver code,
 * and since reads are limited to 62Ki, duplicate the first 60Ki of the next
 * file to this file.
 *
 * Usage is simple: specify the drive letter of the CD (if you have more than
 * one) and the name of the image (which will default to the label with an
 * extension of ".ISO" for CD and ".I" for DVD).
 *
 * Parts of this program are taken directly from CDCACHER.C by John McCoy.
 */

#define PVERS "1.00"
#define PDATE "28 May, 2005"


#include <stdlib.h>
#include <stdio.h>
#include <alloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <dos.h>
#include <string.h>
#include <conio.h>
#include <time.h>

#define PriVolDescSector 16
#define ISO_ID		 "CD001"
#define HSF_ID		 "CDROM"
#define CD_ISO		 'I'
#define CD_HSF		 'H'
#define CD_Unknown	 'U'

#define IMG_SIZE	 262144L
#define IMG_SHIFT	 18

#define OCTETS(from,to) (to - from + 1)

typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;

struct ISO_CD
{
  BYTE	Fill	[OCTETS(   1,	 1 )];
  char	cdID	[OCTETS(   2,	 6 )];
  BYTE	Fill2	[OCTETS(   7,	40 )];
  char	volLabel[OCTETS(  41,	72 )];
  BYTE	Fill3	[OCTETS(  73,	80 )];
  DWORD volSize;
  BYTE	Fill4	[OCTETS(  85,  830 )];
  char	modDate [OCTETS( 831,  847 )];
  BYTE	Fill5	[OCTETS( 848, 2048 )];
} far* iso;

struct HSF_CD
{
  BYTE	Fill	[OCTETS(   1,	 9 )];
  char	cdID	[OCTETS(  10,	14 )];
  BYTE	Fill2	[OCTETS(  15,	48 )];
  char	volLabel[OCTETS(  49,	80 )];
  BYTE	Fill3	[OCTETS(  81,	88 )];
  DWORD volSize;
  BYTE	Fill4	[OCTETS(  93,  806 )];
  char	modDate [OCTETS( 807,  822 )];
  BYTE	Fill5	[OCTETS( 823, 2048 )];
} far* hsf;


void  CreateCacheName( void );
int   Image( DWORD start );
void  CheckFreeSpace( DWORD volSize );
void  GetFTime( void );
int   CDReadLong( WORD SectorCount, DWORD StartSector );
void  ReadPVD( void );
void  progress( DWORD cur, DWORD max );
int   Abort( const char* msg1, const char* msg2 );
char* thoufmt( DWORD num );
void  get_country_info( void );


char far* dta;
char  CDfmt = CD_Unknown;
int   CD = -1;		// Drive number of the CD/DVD (A: = 0)
int   DVD = 0;		// 1 for a DVD (>= 2GiB)
char  CacheName[80];
char* img_char;
DWORD sectors;
struct ftime ft;

char  decisep = '.', thousep = ',', timesep = ':';
char  prochar[2][4] = { "°±²Û", "-+*#" };
char  ascii = 0;

enum
{
  E_OK, 		// No problems
  E_MEM,		// Not enough memory
  E_NOCD,		// Not a CD drive, MSCDEX/SHSUCDX not installed,
			//  unknown CD format or no CD present
  E_EXISTS,		// Image file already exists
  E_CREATE,		// Unable to create image file or not enough free space
  E_ABORTED		// User aborted (with read/write error)
};


int main( int argc, char** argv )
{
  union REGS regs;
  int	j, len;
  char* dot;
  DWORD s;
  int	rc;

  if (argc > 1)
  {
    if (argv[1][0] == '?' || argv[1][1] == '?' || !strcmp( argv[1], "--help" ))
    {
      puts(
  "Optical Media Image by Jason Hood <jadoxa@yahoo.com.au>.\n"
  "Version "PVERS" ("PDATE"). Freeware.\n"
  "http://shsucdx.adoxa.cjb.net/\n"
  "\n"
  "Create an image of a CD- or DVD-ROM.\n"
  "\n"
  "omi [Drive] [Image] [Sectors] [-s] [-a]\n"
  "\n"
  "Drive:   drive letter containing disk (default is first CD/DVD)\n"
  "Image:   name of image (default is label + \".ISO\" [CD] or \".I\" [DVD])\n"
  "Sectors: number of sectors to image (default is entire disk)\n"
  "-s:      split the image, even if it would fit as one file\n"
  "-a:      use an ASCII progress bar"
	  );
      return E_OK;
    }

    for (j = 1; j < argc; ++j)
    {
      char* bp;
      DWORD num;

      if (argv[j][1] == '\0' || (argv[j][1] == ':' && argv[j][2] == '\0'))
      {
	CD = (argv[j][0] | 0x20) - 'a';
      }
      else if (argv[j][0] == '-' || argv[j][0] == '/')
      {
	char o = argv[j][1] | 0x20;
	if (o == 's')
	  DVD = 1;
	else if (o == 'a')
	  ascii = 1;
	else
	  strcpy( CacheName, argv[j] );
      }
      else
      {
	num = strtoul( argv[j], &bp, 0 );
	if (*bp == '\0')
	  sectors = num;
	else
	  strcpy( CacheName, argv[j] );
      }
    }
  }

  dta = farmalloc( 30u << 11 ); // transfer up to 30 blocks at a time
  if (dta == NULL)
  {
    fputs( "Not enough memory.\n", stderr );
    return E_MEM;
  }

  if (CD == -1)
  {
    regs.x.ax = 0x1500;
    regs.x.bx = 0;
    int86( 0x2f, &regs, &regs );
    CD = regs.x.cx;
  }
  else
  {
    regs.x.ax = 0x150B;
    regs.x.cx = CD;
    int86( 0x2f, &regs, &regs );
    if (regs.x.bx != 0xADAD)
      regs.x.bx = 0;
    else if (regs.x.ax == 0)
    {
      fprintf( stderr, "%c: is not a CD/DVD drive.\n", CD + 'A' );
      return E_NOCD;
    }
  }
  if (regs.x.bx == 0)
  {
    fputs( "No CD/DVD available (install MSCDEX/SHSUCDX).\n", stderr );
    return E_NOCD;
  }

  iso = (struct ISO_CD far*)dta;
  hsf = (struct HSF_CD far*)dta;
  ReadPVD();
  if (CDfmt == CD_Unknown)
  {
    fputs( "Unknown CD/DVD format or drive not ready.\n", stderr );
    return E_NOCD;
  }

  if (!sectors)
    sectors = (CDfmt == CD_ISO) ? iso->volSize : hsf->volSize;
  if (!DVD)
    DVD = (sectors >= 1048576uL);

  if (!*CacheName)
    CreateCacheName();
  if (DVD)
  {
    dot = img_char = (CacheName[1] == ':') ? CacheName+2 : CacheName;
    for (j = 0; CacheName[j]; ++j)
    {
      if (CacheName[j] == '/' || CacheName[j] == '\\')
	img_char = CacheName + j + 1;
      else if (CacheName[j] == '.')
	dot = CacheName + j + 1;
    }
    if (dot > img_char)
    {
      img_char = dot;
      len = 3;
    }
    else
      len = 8;
    j = strlen( img_char );
    if (j >= len)
      img_char += len - 1;
    else
    {
      img_char += j;
      img_char[1] = '\0';
    }
  }

  CheckFreeSpace( sectors );
  GetFTime();

  if (DVD)
  {
    for (*img_char = 'A', s = 0; s < sectors; ++*img_char, s += IMG_SIZE)
    {
      rc = Image( s );
      if (rc != E_OK)
	break;
    }
  }
  else
    rc = Image( 0 );

  return rc;
}


int Image( DWORD start )
{
  DWORD i, n, volSize;
  int	handle;
  WORD	w;
  int	rc;
  char	action;

  handle = open( CacheName, O_BINARY );
  if (handle != -1)
  {
    close( handle );
    printf( "\"%s\" already exists.\n"
	    "Press 'O' to overwrite, 'R' to resume, anything else to exit.\n",
	    CacheName );
    action = getch() | 0x20;
    if (action != 'o' && action != 'r')
      return E_EXISTS;
  }
  else
    action = 0;

  rc = O_BINARY | O_CREAT | O_WRONLY;
  if (action == 'o')
    rc |= O_TRUNC;
  handle = open( CacheName, rc, S_IWRITE );
  if (handle == -1)
  {
    fprintf( stderr, "\"%s\" could not be created.\n", CacheName );
    return E_CREATE;
  }

  if (DVD)
  {
    volSize = IMG_SIZE + 30;
    if (start + volSize > sectors)
      volSize = sectors - start;
  }
  else
    volSize = sectors;

  if (action)
  {
    gotoxy( 1, wherey() - 1 );
    clreol();
    gotoxy( 1, wherey() - 1 );
    clreol();
  }
  printf( "Writing \"%s\"; size: %s.\n", CacheName, thoufmt( volSize << 11 ) );

  _setcursortype( _NOCURSOR );
  rc = E_OK;
  if (action == 'r')
  {
    // Back up a bit, since there may have been a write error.
    i = filelength( handle ) >> 11;
    i = (i <= 30) ? 0 : i - 30;
    lseek( handle, i << 11, SEEK_SET );
  }
  else
    i = 0;
  progress( ~0, volSize );
  while (i < volSize)
  {
    progress( i, volSize );

    n = volSize - i;
    if (n > 30)
      n = 30;

    while (!CDReadLong( (WORD)n, start + i ))
    {
      if (Abort( "Read error", "try again" ))
	goto aborted;
    }

    for (;;)
    {
      _dos_write( handle, dta, (WORD)n << 11, (WORD*)&w );
      if (w == ((WORD)n << 11))
	break;
      if (Abort( "Write error", "try again" ))
	goto aborted;
      lseek( handle, i << 11, SEEK_SET );
    }

    if (kbhit())
    {
      if (Abort( "Paused", "continue" ))
	goto aborted;
    }

    i += n;
  }
  if (rc == E_OK)
  {
    putch( '\r' );
    clreol();
    setftime( handle, &ft );
  }
  else
  {
  aborted:
    rc = E_ABORTED;
    cputs( "\r\n" );
  }
  _setcursortype( _NORMALCURSOR );

  close( handle );

  return rc;
}


void progress( DWORD cur, DWORD max )
{
  static int old_pc;
  static int len;
	 int pc;

  static clock_t begin_time, total_time, rate_time;
  static WORD	 old_time;
  static DWORD	 rate_val;
	 clock_t elapsed;

  if (cur == ~0)
  {
    char* p;
    len = strlen( p = thoufmt( max ) );
    cprintf( "  0%c0%% [..................................................] %s"
	     , decisep, p );
    begin_time = clock();
    rate_val   = cur;
    old_pc     = 0;
    return;
  }

  // This is number of sectors, so there's no problem with overflow.
  pc = (int)(1000 * cur / max);
  cprintf( "\r%3d%c%d", pc / 10, decisep, pc % 10 );

  pc /= 5;
  gotoxy( 9 + (old_pc >> 2), wherey() );
  while ((old_pc >> 2) < (pc >> 2))
  {
    putch( prochar[ascii][3] );
    old_pc += 4;
  }
  if (pc & 3)
    putch( prochar[ascii][(pc & 3) - 1] );

  gotoxy( 61, wherey() );
  cprintf( "%*s", len, thoufmt( max - cur ) );

  // Borland defines CLK_TCK as 18.2; let's avoid floating point.
  elapsed = clock() - begin_time;
  if (elapsed >= 91)	// five seconds
  {
    if (elapsed - rate_time >= 91)
    {
      total_time = elapsed + (max - cur) * (elapsed - rate_time)
					 / (cur - rate_val);
      rate_time  = elapsed;
      rate_val	 = cur;
    }
    elapsed = (elapsed >= total_time) ? 0 : (total_time - elapsed) * 5 / 91;
    if ((WORD)elapsed != old_time)
    {
      gotoxy( 63 + len, wherey() );
      cprintf( "%2d%c%02d", (WORD)elapsed / 60, timesep, (WORD)elapsed % 60 );
      old_time = (WORD)elapsed;
    }
  }
}


int Abort( const char* msg1, const char* msg2 )
{
  int rc;

  cprintf( "\r\n%s: press ESC to abort, anything else to %s.", msg1, msg2 );
  while (kbhit()) getch();
  rc = getch();
  if (kbhit()) getch(); // skip second key of function keys
  if (rc != 27)
  {
    putch( '\r' );
    clreol();
    gotoxy( 1, wherey() - 1 );
  }

  return (rc == 27);
}


int CDReadLong( WORD SectorCount, DWORD StartSector )
{
  struct REGPACK regs;

  regs.r_ax = 0x1508;
  regs.r_es = FP_SEG( dta );
  regs.r_bx = FP_OFF( dta );
  regs.r_cx = CD;
  regs.r_si = (WORD)(StartSector >> 16);
  regs.r_di = (WORD)StartSector;
  regs.r_dx = SectorCount;
  intr( 0x2f, &regs );
  return !(regs.r_flags & 1);	// carry flag set if error
}


void ReadPVD( void )
{
  if (CDReadLong( 1, PriVolDescSector ))
  {
    if (_fmemcmp( iso->cdID, ISO_ID, sizeof(iso->cdID) ) == 0)
      CDfmt = CD_ISO;
    else if (_fmemcmp( hsf->cdID, HSF_ID, sizeof(hsf->cdID) ) == 0)
      CDfmt = CD_HSF;
  }
}


void CheckFreeSpace( DWORD volSize )
{
  int	drv;
  DWORD cluster, free;
  char	drv_str[4];
  struct diskfree_t df;
  struct
  {
    WORD  size;
    WORD  ver;
    DWORD sectors_per_cluster;
    DWORD bytes_per_sector;
    DWORD avail_clusters;
    DWORD total_clusters;
    DWORD avail_physical_sectors;
    DWORD total_physical_sectors;
    DWORD avail_units;
    DWORD total_units;
    BYTE  reserved[8];
  } edf;
  union REGS regs;
  struct find_t find;

  if (CacheName[1] == ':')
    drv = (*CacheName | 0x20) - 'a' + 1;
  else
    _dos_getdrive( (WORD*)&drv );

  drv_str[0] = drv + 'A' - 1;
  drv_str[1] = ':';
  drv_str[2] = '\\';
  drv_str[3] = '\0';
  regs.x.ax = 0x7303;
  regs.x.cx = sizeof(edf);
  regs.x.di = (WORD)&edf;
  regs.x.dx = (WORD)drv_str;
  _ES = _DS;
  intdos( &regs, &regs );
  if (regs.x.cflag)
  {
    if (_dos_getdiskfree( drv, &df ))
    {
      fprintf( stderr, "%c: is an invalid drive.\n", *drv_str );
      exit( E_CREATE );
    }
    cluster = df.sectors_per_cluster * df.bytes_per_sector;
    free = df.avail_clusters;
  }
  else
  {
    cluster = edf.sectors_per_cluster * edf.bytes_per_sector;
    free = edf.avail_clusters;
  }
  if (DVD)
  {
    *img_char = '?';    // May find more than intended, but never mind
    free -= ((sectors >> IMG_SHIFT) * 30*2048 + cluster-1) / cluster;
  }
  drv = _dos_findfirst( CacheName, FA_HIDDEN | FA_SYSTEM, &find );
  while (drv == 0)
  {
    free += (find.size + cluster-1) / cluster;
    drv = _dos_findnext( &find );
  }
  // Normalise cluster size to 2Ki.
  if (cluster < 2048)
  {
    free >>= (cluster == 512) ? 2 : 1;
  }
  else if (cluster > 2048)
  {
    cluster >>= 12;
    do
    {
      if ((long)free < 0)
      {
	free = volSize;
	break;
      }
      free <<= 1;
    } while (cluster >>= 1);
  }
  if (free < volSize)
  {
    char* val;
    char  suf;
    if (volSize < 50000u)
    {
      val = thoufmt( volSize << 1 );
      suf = 'K';
    }
    else
    {
      val = thoufmt( volSize >> 9 );
      suf = 'M';
    }
    fprintf( stderr, "Not enough free space on %c: (%s%ciB required, ",
	     *drv_str, val, suf );
    if (free < 50000u)
    {
      val = thoufmt( free << 1 );
      suf = 'K';
    }
    else
    {
      val = thoufmt( free >> 9 );
      suf = 'M';
    }
    fprintf( stderr, "%s%ciB available).\n", val, suf );
    exit( E_CREATE );
  }
}


void CreateCacheName( void )
{
  char far* label;
  char* name = CacheName;
  int	j;

  label = (CDfmt == CD_ISO) ? iso->volLabel : hsf->volLabel;
  for (j = 0; j < 8; ++j)
  {
    if (*label == ' ')
      break;
    *name++ = *label++;
  }
  if (j == 0)
  {
    if (DVD)
    {
      *name++ = 'D';
      *name++ = 'V';
      *name++ = 'D';
    }
    else
    {
      *name++ = 'C';
      *name++ = 'D';
    }
  }
  strcpy( name, (DVD) ? ".I" : ".ISO" );
}


void GetFTime( void )
{
  char far* mod = (CDfmt == CD_ISO) ? iso->modDate : hsf->modDate;
  char buf[16];
  int  year, month, day, hour, min, sec;

  _fstrncpy( buf, mod, 14 );
  sscanf( buf, "%4d%2d%2d%2d%2d%2d", &year, &month, &day,
				     &hour, &min,   &sec );
  ft.ft_year  = year - 1980;
  ft.ft_month = month;
  ft.ft_day   = day;
  ft.ft_hour  = hour;
  ft.ft_min   = min;
  ft.ft_tsec  = sec >> 1;
}


char* thoufmt( DWORD num )
{
  static char buf[16];
  char* pos;
  int	th;

  pos = buf + 15;
  th  = 0;
  do
  {
    *--pos = (num % 10) + '0';
    num /= 10;
    if (++th == 3 && num)
    {
      *--pos = thousep;
      th = 0;
    }
  } while (num);

  return pos;
}


void get_country_info( void )
{
  struct COUNTRY c;

  if (country( 0, &c ))
  {
    decisep = c.co_desep[0];
    thousep = c.co_thsep[0];
    timesep = c.co_tmsep[0];
  }
}
