/*
  smarter.c - Patch SMARTDrive 5.02 to cache SHSUCDX 3.00.

  Jason Hood, 3 June, 2003.

  22 October, 2003:
    search for offsets, allowing IBM's version to be patched.

  25 September, 2004:
    work with SHSUCDX 3.00;
    recognise another version of SMARTDrive 5.02 (search forward for offset).

  19 November, 2004:
    drive entry size is now returned via the install check;
    added help screen.

  28 November, 2004:
    recognise new version of SHSUCDX' SMARTDrive check (changed SMARTDRV_Q,
     different return value).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char shsucdx[] =
{
  0xbbu, 0xbeu, 0xbau,		// mov	bx, SMARTDRV_Q
  0xb8u, 0x00,	0x11,		// mov	ax, (REDIR shl 8) + InstallChk
  0xcdu, 0x2f,			// int	2fh
  0x81u, 0xfbu, 0xbeu, 0xbau,	// cmp	bx, SMARTDRV_Q
  0x74,  0x01,			// if ne,
  0xc3u,			//  ret
  0x29,  0xc0u, 		// zero ax
  0xc3u 			// ret
};

int main( int argc, char* argv[] )
{
  FILE*  file;
  char	 name[80];
  char*  buf;
  size_t len;
  int	 ofs;

  if (argc > 1 && (strcmp( argv[1], "/?" ) == 0 ||
		   strcmp( argv[1], "--help" ) == 0))
  {
    puts(
    "Patch SMARTDrive 5.02 to cache SHSUCDX (version 3.00 and higher).\n"
    "Only one parameter is accepted, which is the filename of SMARTDrive.\n"
    "If absent, it will try the path given in the \"winbootdir\" environment\n"
    "variable, or C:\\WINDOWS, with SMARTDRV.EXE. If the patch is successful,\n"
    "SMARTCDX.EXE will be created in the current directory.\n"
    "Note: SMARTCDX will not cache MSCDEX.\n"
    "\n"
    "Jason Hood <jadoxa@yahoo.com.au>, 28 November, 2004."
    );
    return 0;
  }

  if (argc == 1)
  {
    char* windir = getenv( "winbootdir" );
    if (windir == NULL) windir = "c:/windows";
    strcpy( name, windir );
    strcat( name, "/smartdrv.exe" );
  }
  else strcpy( name, argv[1] );

  if ((file = fopen( name, "rb" )) == NULL)
  {
    fprintf( stderr, "Unable to open \"%s\".\n", name );
    return 1;
  }
  fseek( file, 0, SEEK_END );
  len = (size_t)ftell( file );
  buf = malloc( len );
  if (!buf)
  {
    fprintf( stderr, "Not enough memory (wanted to allocate %u).\n", len );
    return 2;
  }
  rewind( file );
  fread( buf, 1, len, file );
  fclose( file );

  ofs = 256;
  while (ofs > -1024)
  {
    if (*(short*)(buf+ofs + 0x8d23u) == 0x0275)
      break;
    --ofs;
  }

  if ((*(short*)(buf+ofs + 0x8d15u) != 0x0114 &&  // Verify offsets
       *(short*)(buf+ofs + 0x8d15u) != 0x0113) ||
       *(short*)(buf+ofs + 0x8d23u) != 0x0275)
  {
    fputs( "Unrecognised SMARTDrive version.\n", stderr );
    return 3;
  }

  if ((file = fopen( "smartcdx.exe", "wb" )) == NULL)
  {
    fputs( "Unable to create SMARTCDX.EXE\n", stderr );
    return 4;
  }

  buf[ofs+0x8d0fu] = 0x19;		 // Jump past tests
  *(short*)(buf+ofs + 0x8d2eu) = 0x9090; // Remove MOV DI, [DI] (set directly)

  *(short*)(buf+ofs + 0x8d90u) = 0x9090; // SHSUCDX uses A=0, but MSCDEX uses
  *(short*)(buf+ofs + 0x8dc5u) = 0x9090; //  A=1 - remove SMARTDrive's DEC AL
  *(short*)(buf+ofs + 0x8deeu) = 0x9090;
  *(short*)(buf+ofs + 0x8df8u) = 0x9090;

  *(short*)(buf+ofs + 0x8e03u) = 0xd301; // Adjust the drive entry size
  buf[ofs+0x8e05u] = 0x90u;		 // ADD BX, 25h --> ADD BX, DX

  // Replace the MSCDEX test with the SHSUCDX test.
  memcpy( buf+ofs + 0x8e0cu, shsucdx, sizeof(shsucdx) );

  if (fwrite( buf, 1, len, file ) != len)
  {
    fputs( "Not all bytes written!\n", stderr );
    return 5;
  }

  fclose( file );

  return 0;
}
