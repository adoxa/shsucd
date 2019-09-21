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
 *
 * v1.01, 6 & 10 June, 2005:
 *   Win32 port (image on NTFS is a single file);
 *   use creation date if there is no modification date;
 *   always use KiB for free space message (MiB might be too close to notice).
 */

#define PVERS "1.01"
#define PDATE "10 June, 2005"


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <time.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# ifdef __MINGW32__
#  include <tcconio.h>
# endif
# define far
# define farmalloc malloc
# define _fmemcmp  memcmp
# define _fstrncpy strncpy
# define setftime( h, t ) SetFileTime( h, NULL, NULL, t )
#else
# include <fcntl.h>
# include <sys/stat.h>
# include <alloc.h>
# include <dos.h>
# include <string.h>
typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;
typedef unsigned int  UINT;
#endif

#define PriVolDescSector 16
#define ISO_ID		 "CD001"
#define HSF_ID		 "CDROM"
#define CD_ISO		 'I'
#define CD_HSF		 'H'
#define CD_Unknown	 'U'

#define IMG_SIZE	 262144L
#define IMG_SHIFT	 18

#define OCTETS(from,to) (to - from + 1)

struct ISO_CD
{
  BYTE	Fill	[OCTETS(   1,	 1 )];
  char	cdID	[OCTETS(   2,	 6 )];
  BYTE	Fill2	[OCTETS(   7,	40 )];
  char	volLabel[OCTETS(  41,	72 )];
  BYTE	Fill3	[OCTETS(  73,	80 )];
  DWORD volSize;
  BYTE	Fill4	[OCTETS(  85,  813 )];
  char	cr8Date [OCTETS( 814,  830 )];
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
  BYTE	Fill4	[OCTETS(  93,  790 )];
  char	cr8Date [OCTETS( 791,  806 )];
  char	modDate [OCTETS( 807,  822 )];
  BYTE	Fill5	[OCTETS( 823, 2048 )];
} far* hsf;


void  CreateCacheName( void );
int   Image( DWORD start );
void  CheckFreeSpace( DWORD volSize );
void  GetFTime( void );
int   CDReadLong( UINT SectorCount, DWORD StartSector );
void  ReadPVD( void );
void  progress( DWORD cur, DWORD max );
int   Abort( const char* msg1, const char* msg2 );
char* thoufmt( DWORD num );
void  get_country_info( void );


char far* dta;
char  CDfmt = CD_Unknown;
int   CD = -1;		// Drive number of the CD/DVD (A: = 0)
int   DVD = 0;		// 1 for a DVD (>= 2GiB and not NTFS)
char  CacheName[260];
char* img_char;
DWORD sectors;

char  decisep = '.', thousep = ',', timesep = ':';
char  prochar[2][4] = { "°±²Û", "-+*#" };
int   ascii = 0;

#ifdef _WIN32
FILETIME ft;
HANDLE fdin;
#define MAX 32
#define FIVESECS 5000	// CLOCKS_PER_SEC == 1000
#else
struct ftime ft;
#define MAX 30u
#define FIVESECS 91	// CLOCKS_PER_SEC == 18.2 (Borland)
#endif

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
#ifndef _WIN32
  union REGS regs;
#endif
  int	j, len;
  char* dot;
  DWORD s;
  int	rc = E_OK;

  if (argc > 1)
  {
    if (argv[1][0] == '?' || argv[1][1] == '?' || !strcmp( argv[1], "--help" ))
    {
      puts(
  "Optical Media Image by Jason Hood <jadoxa@yahoo.com.au>.\n"
  "Version "PVERS" ("PDATE"). Freeware.\n"
  "http://shsucdx.adoxa.vze.com/\n"
  "\n"
  "Create an image of a CD- or DVD-ROM.\n"
  "\n"
  "omi [Drive] [Image] [Sectors] [-s] [-a]\n"
  "\n"
  "Drive:   drive letter containing disc (default is first CD/DVD)\n"
  "Image:   name of image (default is label + \".ISO\" [CD] or \".I\" [DVD])\n"
  "Sectors: number of sectors to image (default is entire disc)\n"
  "-s:      split the image, even if it would fit as one file\n"
  "-a:      use an ASCII progress bar"
	  );
      return E_OK;
    }

    for (j = 1; j < argc; ++j)
    {
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
	  ascii = !ascii;
	else
	  strcpy( CacheName, argv[j] );
      }
      else
      {
	s = strtoul( argv[j], &dot, 0 );
	if (*dot == '\0')
	  sectors = s;
	else
	  strcpy( CacheName, argv[j] );
      }
    }
  }

  dta = farmalloc( MAX << 11 ); // transfer up to MAX blocks at a time
  if (dta == NULL)
  {
    fputs( "ERROR: Not enough memory.\n", stderr );
    return E_MEM;
  }

#ifdef _WIN32
  if (CD == -1)
  {
    len = GetLogicalDriveStrings( 2048, dta );
    for (dot = dta; len; dot += 4, len -= 4)
    {
      if (GetDriveType( dot ) == DRIVE_CDROM)
      {
	CD = *dot - 'A';
	break;
      }
    }
    if (CD == -1)
    {
      fputs( "ERROR: No CD-ROM drives assigned.\n", stderr );
      return E_NOCD;
    }
  }
  else
  {
    dta[0] = CD + 'A';
    dta[1] = ':';
    dta[2] = '/';
    dta[3] = '\0';
    if (GetDriveType( dta ) != DRIVE_CDROM)
    {
      fprintf( stderr, "ERROR: %c: is not a CD-ROM drive.\n", *dta );
      return E_NOCD;
    }
  }
  sprintf( dta, "//./%c:", CD + 'A' );
  fdin = CreateFile( dta, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
		     NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
  if (fdin == INVALID_HANDLE_VALUE)
  {
    fprintf( stderr, "ERROR: Cannot open %s.\n", dta );
    return E_NOCD;
  }

#else
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
      fprintf( stderr, "ERROR: %c: is not a CD-ROM drive.\n", CD + 'A' );
      return E_NOCD;
    }
  }
  if (regs.x.bx == 0)
  {
    fputs( "ERROR: No CD-ROM drives assigned.\n", stderr );
    return E_NOCD;
  }
#endif

  iso = (struct ISO_CD far*)dta;
  hsf = (struct HSF_CD far*)dta;
  ReadPVD();
  if (CDfmt == CD_Unknown)
  {
    fputs( "ERROR: Unknown CD/DVD format or drive not ready.\n", stderr );
    return E_NOCD;
  }

  if (!sectors)
    sectors = (CDfmt == CD_ISO) ? iso->volSize : hsf->volSize;
  if (!DVD)
  {
#ifdef _WIN32
    char buf[8];
    if (*CacheName && CacheName[1] == ':')
    {
      buf[0] = *CacheName;
      buf[1] = ':';
      buf[2] = '/';
      buf[3] = '\0';
      dot = buf;
    }
    else
    {
      *buf = '\0';
      dot = NULL;
    }
    GetVolumeInformation( dot, NULL, 0, NULL, NULL, NULL, buf, sizeof(buf) );
    if (strcmp( buf, "NTFS" ) != 0)
#endif
    DVD = (sectors >= 1048576uL);
  }

  if (!*CacheName)
    CreateCacheName();
  if (DVD)
  {
#ifdef _WIN32
    img_char = strchr( CacheName, '\0' );
    img_char[1] = '\0';
#else
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
#endif
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
  int	rc;
  char	action;
#ifdef _WIN32
  HANDLE handle;
  DWORD  w;
  #define write_cd( h, b, n, w ) WriteFile( h, b, n << 11, &w, NULL )
  #define back_up( h, w ) SetFilePointer( h, -(LONG)w, NULL, FILE_CURRENT )
  #define close_cd( h ) CloseHandle( h )
#else
  int	handle;
  WORD	w;
  #define write_cd( h, b, n, w ) _dos_write( h, b, (WORD)n << 11, (WORD*)&w );
  #define back_up( h, w ) lseek( h, -(long)w, SEEK_CUR )
  #define close_cd( h ) close( h )
#endif

  if (access( CacheName, 0 ) == 0)
  {
    printf( "\"%s\" already exists.\n"
	    "Press 'O' to overwrite, 'R' to resume, anything else to exit.\n",
	    CacheName );
    action = getch() | 0x20;
    if (action != 'o' && action != 'r')
      return E_EXISTS;
  }
  else
    action = 0;

#ifdef _WIN32
  handle = CreateFile( CacheName, GENERIC_WRITE, 0, NULL,
		       (action == 'o') ? CREATE_ALWAYS : OPEN_ALWAYS, 0, NULL );
  if (handle == INVALID_HANDLE_VALUE)
#else
  rc = O_BINARY | O_CREAT | O_WRONLY;
  if (action == 'o')
    rc |= O_TRUNC;
  handle = open( CacheName, rc, S_IWRITE );
  if (handle == -1)
#endif
  {
    fprintf( stderr, "ERROR: \"%s\" could not be created.\n", CacheName );
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
#if !defined( _WIN32 )
  printf( "Writing \"%s\"; size: %s.\n", CacheName, thoufmt( volSize << 11 ) );
#elif defined( __LCC__ )
  printf( "Writing \"%s\"; size: %'I64d.\n", CacheName, (INT64)volSize << 11 );
#else
  printf( "Writing \"%s\"; size: ", CacheName );
  if (volSize < 2097152) puts( thoufmt( volSize << 11 ) );
  else
  {
    __int64 sz = (__int64)volSize << 11;
    printf( "%s,", thoufmt( (DWORD)(sz / 1000000) ) );
    puts( thoufmt( (DWORD)(sz % 1000000) ) );
  }
#endif

  _setcursortype( _NOCURSOR );
  rc = E_OK;
  if (action == 'r')
  {
    // Back up a bit, since there may have been a write error.
#ifdef _WIN32
    LARGE_INTEGER fs;
    fs.LowPart = GetFileSize( handle, (PULONG)&fs.HighPart );
    i = (DWORD)(fs.QuadPart >> 11);
    i = (i <= MAX) ? 0 : i - MAX;
    fs.QuadPart = (LONGLONG)i << 11;
    SetFilePointer( handle, fs.LowPart, &fs.HighPart, FILE_BEGIN );
#else
    i = filelength( handle ) >> 11;
    i = (i <= MAX) ? 0 : i - MAX;
    lseek( handle, i << 11, SEEK_SET );
#endif
  }
  else
    i = 0;
  progress( ~0, volSize );
  while (i < volSize)
  {
    progress( i, volSize );

    n = volSize - i;
    if (n > MAX)
      n = MAX;

    while (!CDReadLong( (UINT)n, start + i ))
    {
      if (Abort( "Read error", "try again" ))
	goto aborted;
    }

    for (;;)
    {
      write_cd( handle, dta, n, w );
      if (w == ((UINT)n << 11))
	break;
      if (Abort( "Write error", "try again" ))
	goto aborted;
      back_up( handle, w );
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

  close_cd( handle );

  return rc;
}


void progress( DWORD cur, DWORD max )
{
  static int old_pc;
  static int len;
	 int pc;

  static clock_t begin_time, total_time, rate_time;
  static UINT	 old_time;
  static DWORD	 rate_val;
	 clock_t elapsed;

  if (cur == ~0)
  {
    char* p;
    len = strlen( p = thoufmt( max ) );
    cprintf( "  0%c0%% [..................................................] %s"
	     , decisep, p );
    rate_time  = begin_time = clock();
    rate_val   = 0;
    old_pc     = 0;
    return;
  }
  if (rate_val == 0)
    rate_val = cur;

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

  elapsed = clock() - begin_time;
  if (elapsed >= FIVESECS)
  {
    if (elapsed - rate_time >= FIVESECS)
    {
      total_time = elapsed + (max - cur) * (elapsed - rate_time)
					 / (cur - rate_val);
      rate_time  = elapsed;
      rate_val	 = cur;
    }
    elapsed = (elapsed >= total_time) ? 0 : (total_time - elapsed) * 5/FIVESECS;
    if ((UINT)elapsed != old_time)
    {
      gotoxy( 63 + len, wherey() );
      cprintf( "%2d%c%02d", (UINT)elapsed / 60, timesep, (UINT)elapsed % 60 );
      old_time = (UINT)elapsed;
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


int CDReadLong( UINT SectorCount, DWORD StartSector )
{
#ifdef _WIN32
  static LARGE_INTEGER pos;
  LARGE_INTEGER ofs;
  DWORD len;

  // Let's try and avoid unnecessary seeking.
  ofs.QuadPart = (LONGLONG)StartSector << 11;
  if (ofs.QuadPart != pos.QuadPart)
    SetFilePointer( fdin, ofs.LowPart, &ofs.HighPart, FILE_BEGIN );
  ReadFile( fdin, dta, SectorCount << 11, &len, NULL );
  pos.QuadPart += len;
  return (len == (SectorCount << 11));

#else
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
#endif
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
  DWORD cluster, free;
#ifdef _WIN32
  DWORD bps, spc;
  char	drv_str[MAX_PATH];
  WIN32_FIND_DATA find;
  HANDLE hfind;
#else
  int	drv;
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
#endif

#ifdef _WIN32
  if (CacheName[1] == ':')
  {
    drv_str[0] = *CacheName & 0xdf;
    drv_str[1] = ':';
    drv_str[2] = '/';
  }
  else
  {
    GetCurrentDirectory( sizeof(drv_str), drv_str );
  }
  drv_str[3] = '\0';
  GetDiskFreeSpace( drv_str, &spc, &bps, &free, NULL );
  cluster = spc * bps;

#else
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
      fprintf( stderr, "ERROR: %c: is an invalid drive.\n", *drv_str );
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
#endif

  if (DVD)
  {
    *img_char = '?';    // May find more than intended, but never mind
    free -= ((sectors >> IMG_SHIFT) * 30*2048 + cluster-1) / cluster;
  }
#ifdef _WIN32
  hfind = FindFirstFile( CacheName, &find );
  if (hfind != INVALID_HANDLE_VALUE)
  {
    do
    {
      LONGLONG size = ((LONGLONG)find.nFileSizeHigh << 32)
		      + find.nFileSizeLow;
      free += (DWORD)((size + cluster-1) / cluster);
    } while (FindNextFile( hfind, &find ));
    FindClose( hfind );
  }
#else
  drv = _dos_findfirst( CacheName, FA_HIDDEN | FA_SYSTEM, &find );
  while (drv == 0)
  {
    free += (find.size + cluster-1) / cluster;
    drv = _dos_findnext( &find );
  }
#endif
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
    } while ((cluster >>= 1) != 0);
  }
  if (free < volSize)
  {
    fprintf( stderr, "Not enough free space on %c: (%sKiB required, ",
	     *drv_str, thoufmt( volSize << 1 ) );
    fprintf( stderr, "%sKiB available).\n", thoufmt( free << 1 ) );
    exit( E_CREATE );
  }
}


void CreateCacheName( void )
{
  char far* label;
  char* name = CacheName;
  int	j;

  label = (CDfmt == CD_ISO) ? iso->volLabel : hsf->volLabel;
#ifdef _WIN32
  j = 32;
#else
  j = 8;
#endif
  for (; j > 0; --j)
  {
    if (*label == ' ')
      break;
    *name++ = *label++;
  }
  if (name == CacheName)
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
#ifdef _WIN32
  SYSTEMTIME t;
#endif

  _fstrncpy( buf, mod, 14 );
  sscanf( buf, "%4d%2d%2d%2d%2d%2d", &year, &month, &day,
				     &hour, &min,   &sec );
  if (year == 0)
  {
    mod = (CDfmt == CD_ISO) ? iso->cr8Date : hsf->cr8Date;
    _fstrncpy( buf, mod, 14 );
    sscanf( buf, "%4d%2d%2d%2d%2d%2d", &year, &month, &day,
				       &hour, &min,   &sec );
  }
#ifdef _WIN32
  t.wYear   = year;
  t.wMonth  = month;
  t.wDay    = day;
  t.wHour   = hour;
  t.wMinute = min;
  t.wSecond = sec;
  t.wMilliseconds = 0;
  SystemTimeToFileTime( &t, &ft );
#else
  ft.ft_year  = year - 1980;
  ft.ft_month = month;
  ft.ft_day   = day;
  ft.ft_hour  = hour;
  ft.ft_min   = min;
  ft.ft_tsec  = sec >> 1;
#endif
}


char* thoufmt( DWORD num )
{
  static char buf[16];

#ifdef __LCC__
  sprintf( buf, "%'lu", num );
  return buf;

#else
  char* pos;
  int	th;

  pos = buf + 15;
  th  = 0;
  do
  {
    if (++th == 4)
    {
      *--pos = thousep;
      th = 0;
    }
    else
    {
      *--pos = (num % 10) + '0';
      num /= 10;
    }
  } while (num);

  return pos;
#endif
}


void get_country_info( void )
{
#ifdef _WIN32
  char sep;

  if (GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, &sep, 1 ))
    decisep = sep;
  if (GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, &sep, 1 ))
    thousep = sep;
  if (GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_STIME, &sep, 1 ))
    timesep = sep;

#else
  struct COUNTRY c;

  if (country( 0, &c ))
  {
    decisep = c.co_desep[0];
    thousep = c.co_thsep[0];
    timesep = c.co_tmsep[0];
  }
#endif
}
