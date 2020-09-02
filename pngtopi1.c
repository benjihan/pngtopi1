/**
 * @file    pngtopi1.x
 * @author  Benjamin Gerard <https://sourceforge.net/u/benjihan>
 * @date    2017-05-31
 * @brief   a simple png to p[ci]1/2/3 (and reverse) image converter.
 */

/*
  ----------------------------------------------------------------------

  pngtopi1 - a simple png to p[ci]1/2/3 image converter (and reverse)
  Copyright (C) 2018-2020 Benjamin Gerard

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  ----------------------------------------------------------------------
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#else

# if !defined(DEBUG) && !defined(NDEBUG)
#  define NDEBUG 1
# endif

# ifndef COPYRIGHT
#  define COPYRIGHT "Copyright (c) 2018-2020 Benjamin Gerard"
# endif

# ifndef PROGRAM_NAME
#  define PROGRAM_NAME "pngtopi1"
# endif

# ifndef PACKAGE_STRING
#  define PACKAGE_STRING PROGRAM_NAME " " __DATE__
# endif

# ifndef FMT12
#  ifdef __GNUC__     /* /GB: Should test against the exact version */
#    define FMT12 __attribute__ ((format (printf, 1, 2)))
#  endif
# endif

#define _GNU_SOURCE 1

#endif /* HAVE_CONFIG_H */

/* ---------------------------------------------------------------------- */


/* std */
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ctype.h"
#include <getopt.h>
#include <errno.h>

#ifdef __MINGW32__
#include <libgen.h> /* GB: mingw does not have basename() in string.h  */
#endif

/* libpng */
#include <png.h>

/* ---------------------------------------------------------------------- */

/* Error codes */
enum {
  E_OK, E_ERR, E_ARG, E_INT, E_INP, E_OUT, E_PNG
};

/* For opt_pcx */
enum {
  PXX = 0, PIX = 1, PCX = 2, PNG = 3
};

static const char type_names[][4] = {
  "P??","PI?","PC?","PNG"
};

/* RGB conversion methods (bit-field) */
enum {
  CQ_TBD = 0,                  /* ..00 | To be determined           */
  CQ_STF = 1,                  /* ..01 | Using 3 bits per component */
  CQ_STE = 2,                  /* ..10 | Using 4 bits per component */

  CQ_000 = 4,                  /* 01.. | 0 fill                     */
  CQ_LBR = 8,                  /* 10.. | Left bit replication       */
  CQ_FDR = 12                  /* 11.. | Full dynamic range         */
};

/* Degas magic word */
enum {
  DEGAS_PI1 = 0x0000,
  DEGAS_PI2 = 0x0001,
  DEGAS_PI3 = 0x0002,
  DEGAS_PC1 = DEGAS_PI1+0x8000,
  DEGAS_PC2 = DEGAS_PI2+0x8000,
  DEGAS_PC3 = DEGAS_PI3+0x8000
};

static uint8_t opt_col = CQ_TBD; /* mask used color bits (default TBD)*/
static uint8_t opt_out = PXX;    /* {PXX,PIX,PCX,PNG} (see enum) */
static  int8_t opt_bla = 0;      /* blah blah level */
static uint8_t opt_dir = 0;      /* same dir versus current dir */

typedef unsigned int uint_t;

typedef struct myfile_s myfile_t;
struct myfile_s {
  FILE * file;
  char * path;
  int err;
  size_t len;
  uint8_t mode, report;
};

#define IMG_COMMON                              \
  int8_t magic[4];                              \
  char * path;                                  \
  int    type, w, h, d, c

typedef struct mypng_s mypng_t;
struct mypng_s {
  IMG_COMMON;

  int i, t, f, z, p;
  png_structp png;
  png_colorp  lut;
  int         lutsz;
  png_infop   inf;
  png_bytep  *rows;
};

typedef struct mypix_s mypix_t;
struct mypix_s {
  IMG_COMMON;
  uint8_t bits[32034];           /* uncompressed (include header)  */
};

typedef union myimg_s myimg_t;
union myimg_s {
  mypix_t pix;
  mypng_t png;
};

typedef struct colorcount_s colcnt_t;
struct colorcount_s {
  uint32_t rgb:12, cnt:20;              /* Is it legal ? */
};
static colcnt_t g_colcnt[0x1000];

static const struct degasfmt_s {
  const char name[4];
  unsigned short  id, minsz,   w,  h, d, c,rle;
} degas[6] = {
  { "PI1", DEGAS_PI1, 32034, 320,200, 2,16, 0 },
  { "PC1", DEGAS_PC1, 1634 , 320,200, 2,16, 1 },
  { "PI2", DEGAS_PI2, 32034, 640,200, 1,4,  0 },
  { "PC2", DEGAS_PC2, 839  , 640,200, 1,4,  1 },
  { "PI3", DEGAS_PI3, 32034, 640,400, 0,0,  0 },
  { "PC3", DEGAS_PC3, 854  , 640,400, 0,0,  1 },
};

/* ----------------------------------------------------------------------
 * Forward declarations
 **/

static char *create_output_path(char * ipath, const char * ext);
static int guess_type_from_path(char * path);
static int save_img_as(myimg_t * img, char * path, int type);
static int save_pix_as(mypix_t * pix, char * path, int type);
static int save_png_as(mypix_t * pix, char * path);
static myimg_t * mypix_from_file(myfile_t * const mf);
static void print_usage(int verbose);
static void print_version(void);


/* ----------------------------------------------------------------------
 *  Message functions
 **/

#ifndef FMT12
#define FMT12
#endif

/* debug message (-vv) */

#ifndef DEBUG

#define dmsg(FMT,...) (void)0

#else
static void dmsg(const char * fmt, ...) FMT12;
static void dmsg(const char * fmt, ...)
{
  if (opt_bla >= 2) {
    va_list list;
    va_start(list, fmt);
    vfprintf(stdout,fmt,list);
    fflush(stdout);
    va_end(list);
  }
}
#endif

/* additional message (-v) */
static void amsg(char * fmt, ...) FMT12;
static void amsg(char * fmt, ...)
{
  if (opt_bla >= 1) {
    va_list list;
    va_start(list, fmt);
    vfprintf(stdout,fmt,list);
    fflush(stdout);
    va_end(list);
  }
}

/* informational message */
static void imsg(const char * fmt, ...) FMT12;
static void imsg(const char * fmt, ...)
{
  if (opt_bla >= 0) {
    va_list list;
    va_start(list, fmt);
    vfprintf(stdout,fmt,list);
    fflush(stdout);
    va_end(list);
  }
}

/* warning message (-q) */
static void wmsg(const char * fmt, ...) FMT12;
static void wmsg(const char * fmt, ...)
{
  if (opt_bla >= 0) {
    va_list list;
    va_start(list, fmt);
    fprintf(stderr,"%s: ",PROGRAM_NAME);
    vfprintf(stderr,fmt,list);
    va_end(list);
    fflush(stderr);
  }
}

/* error message (-qq) */
static void emsg(const char * fmt, ...) FMT12;
static void emsg(const char * fmt, ...)
{
  if (opt_bla >= -1) {
    va_list list;
    va_start(list, fmt);
    fprintf(stderr,"%s: ",PROGRAM_NAME);
    vfprintf(stderr,fmt,list);
    va_end(list);
    fflush(stderr);
  }
}

static void syserror(const char * ipath, const char * alt)
{
  const char * estr = errno ? strerror(errno) : alt;
  if (ipath)
    emsg("(%d) %s -- %s\n", errno, estr, ipath);
  else
    emsg("%s\n", estr);
}

static void notpng(const char * path)
{
  emsg("invalid image format -- %s\n", path);
}

static void pngerror(const char * path)
{
  syserror(path,"libpng error");
}


/* ----------------------------------------------------------------------
 |
 | System functions (with error report)
 |
 * ---------------------------------------------------------------------- */

static void * mf_malloc(size_t l)
{
  void * ptr = malloc(l);
  if(!ptr)
    syserror(0,"alloc error");
  return ptr;
}

static void * mf_calloc(size_t l)
{
  void * ptr = mf_malloc(l);
  if (ptr) memset(ptr,0,l);
  return ptr;
}

static char * mf_strdup(const char * s, int len_or_0)
{
  char * d;
  if (!len_or_0)
    len_or_0 = strlen(s)+1;
  assert(len_or_0 > 0);
  if (d = mf_malloc(len_or_0), d) {
    strncpy(d,s,len_or_0);
    d[len_or_0-1] = 0;
  }
  return d;
}

static int mf_close(myfile_t * const mf)
{
  assert( mf );

  if (mf->file) {
    if ( fclose(mf->file) ) {
      mf->err = errno;
      if (mf->report) syserror(mf->path,"close");
      return -1;
    }
    mf->file = 0;
    dmsg("C<%c> %u \"%s\"\n",
         "ARW+"[mf->mode], (uint_t)mf->len, mf->path);
  }
  return 0;
}

static size_t mf_tell(myfile_t * const mf)
{
  size_t pos = ftell(mf->file);
  if (pos == -1)
    if (mf->report) syserror(mf->path, "tell error");
  return pos;
}

static size_t mf_seek(myfile_t * const mf, long offset, int whence)
{
  if ( fseek(mf->file, offset, whence) ) {
    if (mf->report) syserror(mf->path, "seek error");
    return -1;
  }
  return 0;
}


static int mf_open(myfile_t * const mf, char * path, int mode)
{
  const char * modes[4] = { "ab", "rb", "wb", "rb+" };

  assert( mf );
  assert( path );
  assert( *path );
  assert( mode == 1 || mode == 2 );

  memset(mf,0,sizeof(*mf));
  mf->report = 1;
  mf->mode = mode & 3;
  mf->path = path;
  mf->file = fopen(path, modes[mode & 3]);
  mf->err = errno;
  if (!mf->file) {
    if (mf->report) syserror(path, "open error");
    return -1;
  }

  switch (mf->mode) {
  case 1:
    if (-1 == mf_seek(mf,0,SEEK_END) ||
        -1 == (mf->len = mf_tell(mf))  ||
        -1 == mf_seek(mf,0,SEEK_SET)) {
      mf->report = 0;
      mf_close(mf);
      return -1;
    }
    break;
  case 2:
    mf->len = 0; break;
  default:
    abort();
  }

  dmsg("O<%c> %u \"%s\"\n",
       "ARW+"[mf->mode], (uint_t)mf->len, mf->path);

  return 0;
}

static size_t mf_read(myfile_t * const mf, void * data, size_t len)
{
  size_t n;

  assert( mf );
  assert( mf->mode == 1 );
  n = fread(data,1,len,mf->file);
  if (n == -1) {
    mf->err = errno;
    if (mf->report)
      syserror(mf->path, "read error");
  } else {
    /* dmsg("R<%c> +%u \"%s\"\n", */
    /*      "ARW+"[mf->mode], (uint_t) n, mf->path); */
    if (n != len) {
      if (mf->report)
        emsg("missing input data (%u/%u) -- %s\n\n",
             (uint_t)n, (uint_t)len, mf->path);
      n = -1;
    }
  }
  assert ( n == -1 || n == len );
  return n;
}

static size_t mf_write(myfile_t * const mf, const void * data, size_t len)
{
  size_t n;

  assert( mf );
  assert( mf->mode == 2 );

  errno = 0;
  n = fwrite(data,1,len,mf->file);
  if (n == -1) {
    mf->err = errno;
    if (mf->report)
      syserror(mf->path, "write error");
  } else {
    mf->len += n;
    /* dmsg("W<%c> +%u =%u \"%s\"\n", */
    /*      "ARW+"[mf->mode], (uint_t) n, (uint_t) mf->len, mf->path); */
    if (n != len) {
      if (mf->report)
        emsg("uncompleted write (%u/%u) -- %s\n",
             (uint_t)n, (uint_t)len, mf->path);
      n = -1;
    }
  }
  assert ( n == -1 || n == len );
  return n;
}

/* ----------------------------------------------------------------------
 |
 | Color conversions
 |
 * ---------------------------------------------------------------------- */

/* Table to convert 4 bits component to 8bits.
 *
 * Various method depending on:
 * - color component  depth STf:3 STe:4
 * - The left bit fill method (zero,replicated,full-range)
 */
static uint8_t col_4to8[16];            /* X -> XX */

/* Table to covert 8 bit color component to 4bit.
 *
 * Entries are (444) RGB outputs. The table is fill depending on the
 * color mode set.
 */
static uint16_t rgb_8to4[256];

/* The index of the table match the ST hardware RGB encoding.
 *
 *  STe for backward compatibility uses the bits #3,#7,#11 as the LSB
 *  of respectively green,blue,red component.
 */

/* STf: Zero fill */
static const uint8_t stf_zerofill[16] = {
  0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0,
  0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0
};

/* STf: Left bit replicated  */
static const uint8_t stf_replicated[16] = {
  0x00,0x24,0x49,0x6D,0x92,0xB6,0xDB,0xFF,
  0x00,0x24,0x49,0x6D,0x92,0xB6,0xDB,0xFF
};

/* Stf: Full range */
static const uint8_t stf_fullrange[16] = {
  0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF,
  0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF
};

/* STe: Zero fill */
static const uint8_t ste_zerofill[16] = {
  0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0,
  0x10,0x30,0x50,0x70,0x90,0xB0,0xD0,0xF0
};

/* STe: Left bit replicated  */
static const uint8_t ste_replicated[16] = {
  0x00,0x22,0x44,0x66,0x88,0xAA,0xCC,0xEE,
  0x11,0x33,0x55,0x77,0x99,0xBB,0xDD,0xFF
};

#define ste_fullrange ste_replicated

/* Convert STe color component to standard order */
static const uint8_t ste_to_std[16] = {
  /* ste: 0 1 2 3 4 5 6 7 8 9 A B C D E F
     std: 0 2 4 6 8 A C E 1 3 5 7 9 B D F */
  0x0,0x2,0x4,0x6,0x8,0xA,0xC,0xE,
  0x1,0x3,0x5,0x7,0x9,0xB,0xD,0xF
};

/* Convert standard 4bit component to STe color component to */

static const uint8_t std_to_ste[16] = {
  /* std: 0 1 2 3 4 5 6 7 8 9 A B C D E F
     ste: 0 8 1 9 2 A 3 B 4 C 5 D 6 E 7 F */
  0x0,0x8,0x1,0x9,0x2,0xA,0x3,0xB,
  0x4,0xC,0x5,0xD,0x6,0xE,0x7,0xF
};

static void set_color_mode(int mode)
{
  const uint8_t * col_used;
  int i;
  static const char * lbf_names[] = {
    "zero fill", "left bit replication", "full range"
  };

  switch(mode) {
  default:
    assert( !"unexpected color mode" );
  case CQ_TBD:
  case CQ_STF | CQ_LBR:
    mode = CQ_STF | CQ_LBR;
    col_used = stf_replicated;
    break;

  case CQ_STF | CQ_000:
    col_used = stf_zerofill;
    break;

  case CQ_STF | CQ_FDR:
    col_used = stf_fullrange;
    break;

  case CQ_STE | CQ_000:
    col_used = ste_zerofill;
    break;

  case CQ_STE | CQ_LBR:
    col_used = ste_replicated;
    break;

  case CQ_STE | CQ_FDR:
    col_used = ste_fullrange;
    break;
  }
  memcpy(col_4to8, col_used, 16);
  opt_col = mode;
  amsg("Using ST%s colors with %s\n",
       (mode&3) == CQ_STF ? "":"e", lbf_names[ (mode>>2)-1 ]);

  for ( i=0; i < 256; ++i ) {
    const uint_t c = std_to_ste[i>>4];
    rgb_8to4[i] = c|(c<<4)|(c<<8);
    assert( (rgb_8to4[i] & 0xFFF) == rgb_8to4[i] );
  }
}

static inline uint16_t rgb444(uint8_t r, uint8_t g, uint8_t b)
{
  return 0
    | ( rgb_8to4[r] & 0xF00 )
    | ( rgb_8to4[g] & 0x0F0 )
    | ( rgb_8to4[b] & 0x00F )
    ;
}

/* ----------------------------------------------------------------------
 |
 | Pixel functions.
 |
 * ---------------------------------------------------------------------- */


#define PXL_CHECK(D,T,C)                        \
  do {                                          \
    assert ( png );                             \
    assert( (uint_t)x < (uint_t)png->w );       \
    assert( (uint_t)y < (uint_t)png->h );       \
    assert(png->d == (D));                      \
    assert(png->t == (T));                      \
    assert(png->c == (C));                      \
  } while (0)

#define LUT_CHECK(I)             \
  do {                           \
    assert( png->lut );          \
    assert( (I) < png->lutsz );  \
  } while (0)

typedef uint16_t (*get_f)(const mypng_t *, int, int);

static uint16_t get_st_pixel(const mypix_t * const pix, int x, int y)
{
  const int w = ( pix->w + 15 ) & -16;
  const int bytes_per_line = ( w << pix->d ) >> 3;
  const int tile = x >> 4;
  const int log2_bytes_per_tile = pix->d+1;
  const int bitnum = ~x & 7;
  const int nbplans = 1 << pix->d;
  int col, p;

  assert( (uint_t)x < (uint_t)pix->w );
  assert( (uint_t)y < (uint_t)pix->h );

  /* Using y as offset */
  y *= bytes_per_line;                  /* line offset */
  y += tile << log2_bytes_per_tile;     /* tile offset */
  y += ( x >> 3 ) & 1;                  /* byte offset */
  y += 34;                              /* skip header */

  for ( col = p = 0; p < nbplans; ++p, y += 2 )
    col |= ( ( pix->bits[ y ] >> bitnum ) & 1 ) << p;
  return col;
}

/* ----------------------------------------------------------------------
 *  Gray scale methods
 **/

static uint16_t get_gray1(const mypng_t * png, int x, int y)
{
  PXL_CHECK(1,PNG_COLOR_TYPE_GRAY,1);
  return rgb_8to4[(uint8_t)-(1 & ( png->rows[y][x>>3] >> (~x&7) ))];
}

static uint16_t get_gray2(const mypng_t * png, int x, int y)
{
  int g2;
  PXL_CHECK(2,PNG_COLOR_TYPE_GRAY,1);
  g2 = 3 & ( ( png->rows[y][x>>2] ) >> ((~x&3)<<1) );
  g2 |= (g2<<2);
  g2 |= (g2<<4);
  return rgb444(g2,g2,g2);
}

static uint16_t get_gray4(const mypng_t * png, int x, int y)
{
  int g4;
  PXL_CHECK(4,PNG_COLOR_TYPE_GRAY,1);
  g4 = 15 & ( png->rows[y][x>>1] >> (((~x&1)) << 2));
  g4 |= g4 << 4;
  return rgb444(g4,g4,g4);
}

static uint16_t get_gray8(const mypng_t * png, int x, int y)
{
  PXL_CHECK(8,PNG_COLOR_TYPE_GRAY,1);
  return rgb_8to4[(uint8_t)png->rows[y][x]];
}


/* ----------------------------------------------------------------------
 *  Indexed methods
 **/

static uint16_t get_indexed2(const mypng_t * png, int x, int y)
{
  png_color * rgb; int idx;

  PXL_CHECK(2,PNG_COLOR_TYPE_PALETTE,1);
  idx = 3 & ( ( png->rows[y][x>>2] ) >> ((~x&3)<<1) );
  LUT_CHECK(idx);
  rgb = &png->lut[idx];
  return rgb444(rgb->red,rgb->green,rgb->blue);
}

static uint16_t get_indexed4(const mypng_t * png, int x, int y)
{
  png_color * rgb; int idx;

  PXL_CHECK(4,PNG_COLOR_TYPE_PALETTE,1);
  idx = 15 & ( png->rows[y][x>>1] >> ((~x&1) << 2) );
  LUT_CHECK(idx);
  rgb = &png->lut[idx];
  return rgb444(rgb->red,rgb->green,rgb->blue);
}

static uint16_t get_indexed8(const mypng_t * png, int x, int y)
{
  png_color * rgb;

  PXL_CHECK(8,PNG_COLOR_TYPE_PALETTE,1);
  LUT_CHECK(png->rows[y][x]);
  rgb = &png->lut[png->rows[y][x]];
  return rgb444(rgb->red,rgb->green,rgb->blue);
}

/* ----------------------------------------------------------------------
 *  Direct colors
 **/

static uint16_t get_rgb(const mypng_t * png, int x, int y)
{
  PXL_CHECK(8,PNG_COLOR_TYPE_RGB,3);

  const png_byte r = png->rows[y][x*3+0];
  const png_byte g = png->rows[y][x*3+1];
  const png_byte b = png->rows[y][x*3+2];

  return rgb444(r,g,b);
}

static uint16_t get_rgba(const mypng_t * png, int x, int y)
{
  PXL_CHECK(8,PNG_COLOR_TYPE_RGBA,4);

  const png_byte r = png->rows[y][x*4+0];
  const png_byte g = png->rows[y][x*4+1];
  const png_byte b = png->rows[y][x*4+2];

  return rgb444(r,g,b);
}

/* ----------------------------------------------------------------------
 |
 | Image functions.
 |
 * ---------------------------------------------------------------------- */

static void myimg_free(myimg_t ** img)
{
  assert( img );
  if (*img) {
    free(*img);
    *img = 0;
  }
}

static myimg_t * mypng_init(char * path)
{
  myimg_t * img = mf_calloc(sizeof(img->png));
  if (img) {
    strcpy((char*)img->png.magic, "PNG");
    img->png.path = path ? path : "<mypng>";
    img->png.type = PNG;
  }
  return img;
}

static myimg_t * read_img_file(char * ipath)
{
  png_byte header[8];
  myimg_t * img = 0;
  int y;
  myfile_t mf;

  if (-1 == mf_open(&mf, ipath, 1))
    goto error;

  if (-1 == mf_read(&mf, header, 8))
    goto error;

  if (png_sig_cmp(header, 0, 8)) {

    /* -------------------
     * Trying Degas format
     **/
    if (img = mypix_from_file(&mf), !img)
      goto exit;
  }

  else {
    /* ---------------
     * Trying with PNG
     */

    mypng_t * png;

    img = mypng_init(ipath);
    if (!img)
      goto error;
    png = & img->png;

    png->png = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
    if (!png->png)
      goto png_error;

    png->inf = png_create_info_struct(png->png);
    if (!png->inf)
      goto png_error;

    if (setjmp(png_jmpbuf(png->png)))
      goto png_error;

    png_init_io(png->png,mf.file);
    png_set_sig_bytes(png->png,8);
    png_read_info(png->png, png->inf);

    png->w = png_get_image_width(png->png, png->inf);
    png->h = png_get_image_height(png->png, png->inf);
    png->d = png_get_bit_depth(png->png, png->inf);
    png->t = png_get_color_type(png->png, png->inf);
    png->i = png_get_interlace_type(png->png, png->inf);
    png->z = png_get_compression_type(png->png, png->inf);
    png->f = png_get_filter_type(png->png,png->inf);
    png->c = png_get_channels(png->png, png->inf);
    png->p = png_set_interlace_handling(png->png);
    png_read_update_info(png->png, png->inf);

    /* read file */
    if (setjmp(png_jmpbuf(png->png)))
      goto png_error;

    png_get_PLTE(png->png, png->inf, &png->lut, &png->lutsz);
    if (png->lutsz) {
      const png_byte m = (opt_col&3) == CQ_STE ? ~0xF0 : ~0xE0;
      int i;

      amsg("PNG color look-up table has %d entries:\n", png->lutsz);
      for (i=0; i<png->lutsz; ++i)
        amsg(
          "%3d #%02X%02X%02X $%03x #%02X%02X%02X\n",
          i,png->lut[i].red,png->lut[i].green,png->lut[i].blue,
          rgb444(png->lut[i].red,png->lut[i].green,png->lut[i].blue),
          png->lut[i].red&m, png->lut[i].green&m, png->lut[i].blue&m);
    }

    png->rows = (png_bytep *)
      png_malloc(png->png, png->h*(sizeof (png_bytep)));

    for (y=0; y<png->h; ++y)
      png->rows[y] = (png_byte *)
        png_malloc(png->png, png_get_rowbytes(png->png,png->inf));

    png_read_image(png->png, png->rows);
  }

exit:
  mf_close(&mf);
  return img;

png_error:
  pngerror(ipath);
error:
  myimg_free(&img);
  goto exit;
}

static const char * mypng_typestr(int type)
{
# define CASE_COLOR_TYPE(A) case PNG_COLOR_TYPE_##A: return #A
# define CASE_COLOR_MASK(A) case PNG_COLOR_MASK_##A: return #A
  static char s[8];
  switch (type) {
    CASE_COLOR_TYPE(GRAY);           /* (bit depths 1, 2, 4, 8, 16) */
    CASE_COLOR_TYPE(GRAY_ALPHA);     /* (bit depths 8, 16) */
    CASE_COLOR_TYPE(PALETTE);        /* (bit depths 1, 2, 4, 8) */
    CASE_COLOR_TYPE(RGB);            /* (bit_depths 8, 16) */
    CASE_COLOR_TYPE(RGB_ALPHA);      /* (bit_depths 8, 16) */
    /* CASE_COLOR_MASK(PALETTE); */
    /* CASE_COLOR_MASK(COLOR); */
    /* CASE_COLOR_MASK(ALPHA); */
  }
  sprintf(s,"?%02x?", type & 255);
  return s;
}

static int cc_cmp(const void * _a, const void * _b)
{
  const colcnt_t * const a = _a;
  const colcnt_t * const b = _b;
  return b->cnt - a->cnt;
}

static int lumi(int x)
{
  int r,g,b;
  assert( (x & 0xfff) == x );
  r = ste_to_std[x>>8];
  g = ste_to_std[(x>>4)&15];
  b = ste_to_std[x&15];
  return r*2 + g*4 + b;
}

static int cl_cmp(const void * _a, const void * _b)
{
  const colcnt_t * const a = _a;
  const colcnt_t * const b = _b;
  return lumi(a->rgb) - lumi(b->rgb);
}

void sort_colorcount(colcnt_t * cc, int n)
{
  qsort(cc, n, sizeof(colcnt_t), cc_cmp);
}

void sort_colorbright(colcnt_t * cc, int n)
{
  qsort(cc, n, sizeof(colcnt_t), cl_cmp);
}

static myimg_t * mypix_alloc(int id, char * path)
{
  myimg_t * img;

  assert( (uint_t) id < 6u );

  img = mf_malloc(sizeof(mypix_t));
  if (img) {
    strcpy((char*)img->pix.magic, degas[id].name);
    img->pix.w = degas[id].w;
    img->pix.h = degas[id].h;
    img->pix.d = degas[id].d;
    img->pix.c = degas[id].c;
    img->pix.path = path ? path :"<mypix>";
    img->pix.type = degas[id].rle ? PCX : PIX;
  }
  return img;
}


static int rle_read(myfile_t * mf, myimg_t * img)
{
  const int tiles_per_line = img->pix.w >> 4;
  const int bytes_per_plan = tiles_per_line << 1;
  const int bytes_per_line = bytes_per_plan << img->pix.d;
  const int bytes_per_tile = 2 << img->pix.d;
  uint8_t * dst = img->pix.bits + 34, raw[80];
  int y,x,z;

  assert( bytes_per_plan <= 80 );

  /* For each line */
  for ( y=0; y < img->pix.h; ++y, dst += bytes_per_line ) {

    /* For each plan */
    for ( z=0; z < 1<<img->pix.d; ++z ) {
      uint8_t * row = dst + (z<<1);

      /* decode an entire bitplan line */
      for (x=0; x<bytes_per_plan; ) {
        uint8_t rle[2];

        /* read code + 1 */
        if ( -1 == mf_read(mf, rle, 2 ) )
          return -1;

        if (*rle >= 128) {
          /* Fill 257-rle[0] with rle[1] */
          const uint8_t v = rle[1];
          int n = 257 - *rle;

          if (x+n > bytes_per_plan) {
            emsg("rle-fill overflow line:%d plan:%d byte:%d\n",y,z,x);
            return -1;
          }
          do {
            raw[x++] = v;
          } while (--n);

        } else {
          /* Copy rle[1] and rle[0] more bytes */
          const int n = *rle + 1;
          if (x+n > bytes_per_plan) {
            emsg("rle-copy overflow line:%d plan:%d byte:%d\n",y,z,x);
            return -1;
          }
          if ( -1 == mf_read(mf, raw+x+1, n-1) )
            return -1;
          raw[x] = rle[1];
          x += n;
        }
      }
      assert( x == bytes_per_plan );

      /* For each tile (interlacing) */
      for (x=0; x<bytes_per_plan; x += 2) {
        row[0] = raw[x+0];
        row[1] = raw[x+1];
        row += bytes_per_tile;
      }
    }
  }
  return 0;
}


static myimg_t * mypix_from_file(myfile_t * const mf)
{
  uint8_t hd[34];
  int id, i;
  myimg_t * img = 0;

  assert( mf->file );
  assert( mf->path );
  assert( mf->mode == 1 );

  if (-1 == mf_seek(mf,0,SEEK_SET))
    goto error;
  if (-1 == mf_read(mf,hd,34))
    goto error;

  id = (hd[0]<<8) | hd[1];
  for (i=0; i<6; ++i) {
    if (id == degas[i].id) {
      if (mf->len < degas[i].minsz) {
        emsg("file length (%u) is too short for %s image -- %s\n",
             (uint_t) mf->len, degas[i].name, mf->path);
        return 0;
      }
      break;
    }
  }
  if (i == 6) {
    notpng(mf->path);
    return 0;
  }
  dmsg("%s detected\n", degas[i].name);

  img = mypix_alloc(i,mf->path);
  if (!img)
    return 0;

  memcpy(img->pix.bits,hd,34);          /* copy header */
  if ( ! degas[i].rle ) {
    /* Uncompressed Degas image */
    if ( -1 == mf_read(mf, img->pix.bits+34, 32000) )
      goto error;
  } else {
    /* uncompress on the fly */
    if ( -1 == rle_read(mf, img))
      goto error;
  }

  return img;

error:
  myimg_free(&img);
  return 0;
}


static myimg_t * mypix_from_png(mypng_t * png)
{
  static struct {
    int d,c,t;                          /* depth,channel,type */
    get_f get;                          /* get pixel function */
  } *s, supported[] = {
    /* 320 x 200 */
    { 1, 1, PNG_COLOR_TYPE_GRAY,    get_gray1    },
    { 2, 1, PNG_COLOR_TYPE_GRAY,    get_gray2    },
    { 4, 1, PNG_COLOR_TYPE_GRAY,    get_gray4    },
    { 8, 1, PNG_COLOR_TYPE_GRAY,    get_gray8    },
    { 2, 1, PNG_COLOR_TYPE_PALETTE, get_indexed2 },
    { 4, 1, PNG_COLOR_TYPE_PALETTE, get_indexed4 },
    { 8, 1, PNG_COLOR_TYPE_PALETTE, get_indexed8 },
    { 8, 3, PNG_COLOR_TYPE_RGB,     get_rgb      },
    { 8, 4, PNG_COLOR_TYPE_RGBA,    get_rgba     },
    /**/
    { 0,0,0,0 }
  };

  myimg_t * img = 0;
  uint8_t * bits;
  int x, y, z, log2plans, lutsiz, lutmax, ncolors;

  uint16_t lut[16];
  colcnt_t * const colcnt = g_colcnt;

  int id;

  for (id=0; id<6; id += 2)
    if (png->w == degas[id].w && png->h == degas[id].h)
      break;

  if (id == 6) {
    emsg("incompatible image dimension <%dx%d> -- %s\n",
         png->w,png->h,png->path);
    return 0;
  }

  log2plans = degas[id].d;
  lutmax    = 1<<(1<<log2plans);
  assert( lutmax <= 16 );

  dmsg("search for d:%2d c:%2d %s(%d)\n",
       png->d,png->c,mypng_typestr(png->t),png->t);
  for (s=supported; s->d; ++s) {
    dmsg("    versus d:%2d c:%2d %s(%d)\n",
         s->d,s->c,mypng_typestr(s->t),s->t);
    if (s->d == png->d && s->c == png->c && s->t == png->t)
      break;
  }

  if (!s->d) {
    emsg("incompatible image format -- %s\n",png->path);
    return 0;
  }

  /* count color occurrences */
  for ( x=0; x < 0x1000; ++x ) {
    colcnt[x].rgb = x;
    colcnt[x].cnt = 0;
  }
  for ( y=0; y < png->h; ++y )
    for ( x=0; x < png->w; ++x )
      ++ colcnt[ s->get(png,x,y) ].cnt;

#ifdef DEBUG
  ncolors = 0;
  for (y=0; y<0x1000; ++y) {
    if (colcnt[y].cnt) {
      assert( y == colcnt[y].rgb );
      dmsg(" #%02d $%03X is used %5d times\n",
           ncolors, y, colcnt[y].cnt);
      ncolors++;
    }
  }
#endif

  sort_colorcount(colcnt, 0x1000);

  for ( x=0; x<0x1000 && colcnt[x].cnt; ++x )
    dmsg(" #%02d $%03X %+6d\n", x, colcnt[x].rgb, colcnt[x].cnt);

  if (x > lutmax) {
    emsg("too many colors -- %d > %d -- %s", x, lutmax,png->path);
    return 0;
  }

  if (x < lutmax)
    amsg("using only %d colors out of %d\n", x, lutmax);

  /* Supporting by brightness is only necessary for P?3 images */
  sort_colorbright(colcnt, x);

#ifdef DEBUG
  dmsg("Sorted by lumi\n");
  for (y=0; y < x; ++y) {
    dmsg(" #%02d $%03X %+6d\n", y, colcnt[y].rgb, colcnt[y].cnt);
  }
#endif

  lut[0] = 0;
  for (y=0; y < x; ++y)
    lut[y] = colcnt[y].rgb;
  ncolors = y;
  for (lutsiz = degas[id].c ; y < 16; ++y)
    lut[y] = 0x0F0;
  y = ncolors;

  assert( (( (((15+png->w)>>4)<<1) << log2plans) * png->h) == 32000 );

  if (img = mypix_alloc(id, png->path), !img)
    return 0;

  bits = img->pix.bits;

  /* Degas signature */
  *bits++ = degas[id].id >> 8;
  *bits++ = degas[id].id;

  /* Copy palette */
  for (y=0; y<lutsiz; ++y) {
    const uint_t col = lut[ y ];

    if ( (col & 0xFFF) != col ) {
      dmsg( "#%d/%d %x\n",y,lutsiz, col);
    }

    assert( (col & 0xFFF) == col );
    *bits++ = col >> 8;
    *bits++ = col;
  }
  if (!y)
    *bits++ = 0xF, *bits++ = 0xFF, ++y;
  for (; y<16; ++y) {
    *bits++ = 0;
    *bits++ = 0;
  }

  assert( bits == (img->pix.bits + 34) );

  /* Blit pixels */

  /* Use color occurence array as reverse table */
  dmsg("Reverse LUT\n");
  for (y=0; y < ncolors; ++y) {
    int rgb = lut[y];
    assert( (rgb & 0xFFF) == rgb );
    colcnt[rgb].rgb = y;
  }

  /* Per row */
  for (y=0; y < img->pix.h; ++y) {
    /* Per 16-pixels block */
    for (x=0; x < img->pix.w; x+=16) {
      /* Per bit-plan */
      for (z=0; z<(1<<img->pix.d); ++z) {
        uint_fast16_t bm, bv;
        /* Per pixel in block */
        for (bv=0, bm=0x8000; bm; ++x, bm >>= 1) {
          const unsigned rgb = s->get(png,x,y);
          assert(rgb < 0x1000);
          const unsigned idx = colcnt[rgb].rgb;
          assert(idx < ncolors);
          if (idx & (1<<z))
            bv |= bm;
        }
        x -= 16;
        *bits++ = bv>>8;
        *bits++ = bv;
      }
    }
  }
  assert( bits == img->pix.bits+32034 );

  return img;
}


static void
print_buffer(const uint8_t * b, int n, const char * label)
{
#ifdef DEBUG
  int i, l = strlen(label);

  for (i=0; i<n; ++i) {
    if (!i)
      dmsg("\n%s:",label);
    else if( ! (i & 15) )
      dmsg("\n%*s",l+1,"");
    dmsg(" %02x",b[i]);
  }
  dmsg("\n");
#endif
}

/*
 * RLE coding: code: 00..7f copy code+1 byte [1..128]
 *                   80..ff fill next byte 257-code times [2..129]
 */

static int
enc_copy(uint8_t * d, const uint8_t * s, int l)
{
  const uint8_t * const d0 = d;
  while (l > 0) {
    int n = l;
    if (n > 128) n = 128;
    l -= n;
    if (d0) {
      assert( n >= 1 && n <= 128 );
      d[0] = n-1;
      memcpy(d+1,s,n);
      print_buffer(d, n+1, "CPY");
    }
    d += 1+n;
    s += n;
  }
  return d-d0;
}

static int
enc_fill(uint8_t * d, const uint8_t v, int l)
{
  const uint8_t * const d0 = d;
  while (l >= 2) {
    int n = l;
    if (n > 129)
      /* GB: Ensure at least 2 bytes remaining for the last pass. */
      n = 129 - (n==130);
    l -= n;
    if (d0) {
      assert( n >= 2 && n <= 129 );
      d[0] = 257 - n;
      d[1] = v;
      print_buffer(d, 2, "RPT");
    }
    d += 2;
  }
  assert(l == 0);
  return d-d0;
}

static int
pcx_encode_row(uint8_t * dst, const uint8_t * src, int len)
{
  int i, j, o;

  assert(src);
  assert(len > 0);

  print_buffer(src,len,"RAW");

  for (i=j=o=0; i<len; ) {
    const uint8_t c = src[i];
    int k,l;

    /* Count repeat */
    for (k=i+1; k<len && src[k] == c; ++k)
      ;
    l = k-i;                            /* number of repeat 1..N */

    if (l >= 2) {                       /* have some repeat ? */
      if (i > o) {                      /* have something to copy first ? */
        j += enc_copy(dst+j, src+o, i-o);
        o = i;
      }
      j += enc_fill(dst+j, c, l);
      o = i = k;
    }
    i = k;
  }
  j += enc_copy(dst+j, src+o, i-o);

  print_buffer(dst, j, "ENC");

  return j;
}

#ifdef DEBUG

static int
rle_decode(uint8_t * d, int dl, const uint8_t * s, int sl)
{
  int i,j;

  for (i=j=0; i<sl && j<dl; ) {
    int c = s[i++];
    if (c < 128) {
      /* Copy c+1 byte */
      if (j+c+1 > dl) {
        return -1;
      }
      for (++c; c; --c)
        d[j++] = s[i++];
    } else {
      /* Fill 257-c times next byte */
      const uint8_t v = s[i++];
      if (j+257-c > dl) {
        return -1;
      }
      for (c=257-c; --c >= 0;)
        d[j++] = v;
    }
  }
  print_buffer(d,j,"DEC");
  assert( i <= sl );
  assert( j <= dl );
  return j;
}

#endif

static int save_as_pcx(myfile_t * out, mypix_t * pic)
{
  const int bpr = (pic->w>>4) << (pic->d+1); /* bytes per row */
  const int off = 2 << pic->d;       /* offset to next word in plan */
  int x,y,z;

  uint8_t rle[128], raw[80], *r;
  const uint8_t * pix = pic->bits+34;

  /* Write header */
  pic->type = PCX;
  pic->magic[1] = 'C';
  pic->bits[0] = DEGAS_PC1 >> 8;
  if (-1 == mf_write(out, pic->bits, 34))
    return -1;

  /* De-interleave into raw[] and RLE encode into rle[] */

  /* For each line */
  for (y=0; y<pic->h; ++y, pix += bpr) {
    /* For each plan */
    for (z=0; z<(1<<pic->d); ++z) {
      int l;
      const uint8_t * row = pix + (z<<1);
      /* For each 16-pixels tile */
      for (x=0, r=raw; x<pic->w>>4; ++x) {
        *r++ = row[0];
        *r++ = row[1];
        row += off;
      }
      l = pcx_encode_row(rle, raw, r-raw);

#ifdef DEBUG
      if (1) {
        uint8_t xxx[80];
        int lx = rle_decode(xxx, 80, rle, l);
        assert(lx == bpr >> pic->d) ;
        assert(!memcmp(raw,xxx,lx));
      }
#endif
      if (-1 == mf_write(out, rle,l))
        return -1;
    }
  }
  return (int) out->len;
}

static int save_as_pix(myfile_t * out, mypix_t * pic)
{
  const int l = pic->h * ( (pic->w>>4) << (pic->d+1) );

  assert( l==32000 );
  pic->type = PIX;
  pic->magic[1] = 'I';
  return -1 == mf_write(out, pic->bits, 34+l)
    ? -1
    : out->len
    ;
}

static int save_pix_as(mypix_t * pix, char * path, int type)
{
  myfile_t mf;
  int n;

  assert( path );
  assert( pix );

  if ( -1 == mf_open(&mf, path, 2))
    return -1;

  n = (type == PCX)
    ? save_as_pcx(&mf,pix)
    : save_as_pix(&mf,pix)
    ;

  assert( n == mf.len );

  if ( mf_close(&mf) == -1 || n == -1)
    return -1;

  imsg("output: \"%s\" %dx%dx%d (%s) size:%d\n",
       path, pix->w, pix->h, 1<<(1<<pix->d),
       pix->magic, (int)n);

  return 0;
}

static int save_png_as(mypix_t * pix, char * path)
{
  png_structp png_ptr = 0;
  png_infop info_ptr = 0;
  png_bytep row;
  png_byte tmp[160];
  png_color lut[16];
  int png_type;

  int ret=-1, y, x, ste_detect;
  myfile_t mf;

  if (-1 == mf_open(&mf,path,2))
    goto error;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0,0,0);
  if (!png_ptr)
    goto png_error;

  if (info_ptr = png_create_info_struct(png_ptr), !info_ptr)
    goto png_error;

  if (setjmp(png_jmpbuf(png_ptr)))
    goto png_error;

  png_init_io(png_ptr, mf.file);

  /* Set color #0 to black */
  lut->red = lut->green = lut->blue = 0;

  assert (sizeof(lut)/sizeof(*lut) == 16 );
  assert ( pix->c <= 16 );
  for ( ste_detect = y = 0; y < pix->c; ++y ) {
    const uint8_t * const st_lut = &pix->bits[2+(y<<1)];
    const uint16_t st_rgb = (st_lut[0]<<8) | st_lut[1];
    ste_detect += !!(st_rgb & 0x888);
    lut[y].red   = col_4to8[15 & (st_rgb>>8)];
    lut[y].green = col_4to8[15 & (st_rgb>>4)];
    lut[y].blue  = col_4to8[15 & (st_rgb>>0)];
    dmsg("#%X %03X %02X-%02X-%02X\n",
         (uint_t)y, (uint_t)st_rgb & 0xFFF,
         (uint_t)lut[y].red,(uint_t)lut[y].green,(uint_t) lut[y].blue);
  }

  /* Skip color #0, fill the rest with White */
  for ( y += !y ; y < 16; ++y )
    lut[y].red = lut[y].green = lut[y].blue = 255;

  switch ( pix->magic[2] ) {
  case '1':
    assert( pix->w == 320 && pix->h == 200 && pix->d == 2 && pix->c == 16 );
    png_type = PNG_COLOR_TYPE_PALETTE;
    png_set_IHDR(png_ptr, info_ptr, pix->w, pix->h,
                 1<<pix->d, png_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_PLTE(png_ptr,info_ptr,lut,pix->c);
    png_write_info(png_ptr, info_ptr);

    for (y=0, row = pix->bits+34; y<200 ; y++, row += 160) {
      for (x=0; x<320; x += 2)
        tmp[x >> 1] = ( get_st_pixel(pix,x,y) << 4) | get_st_pixel(pix,x+1,y);
      png_write_row(png_ptr, tmp);
    }
    break;

  case '2':
    assert( pix->w == 640 && pix->h == 200 && pix->d == 1 && pix->c == 4 );
    png_type = PNG_COLOR_TYPE_PALETTE;
    png_set_IHDR(png_ptr, info_ptr, pix->w, pix->h,
                 1<<pix->d, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_PLTE(png_ptr,info_ptr,lut,pix->c);
    png_write_info(png_ptr, info_ptr);

    for (y=0, row = pix->bits+34; y<200 ; y++, row += 160) {
      for (x=0; x<640; x += 4)
        tmp[x >> 2] = 0
          | ( get_st_pixel(pix,x+0,y) << 6)
          | ( get_st_pixel(pix,x+1,y) << 4)
          | ( get_st_pixel(pix,x+2,y) << 2)
          | ( get_st_pixel(pix,x+3,y) << 0)
          ;
      png_write_row(png_ptr, tmp);
    }
    break;

  case '3':
    assert( pix->w == 640 && pix->h == 400 && pix->d == 0 && pix->c == 0 );
    png_type = PNG_COLOR_TYPE_GRAY;
    png_set_IHDR(png_ptr, info_ptr, pix->w, pix->h,
                 1, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);

    row = pix->bits+34;
    for (y=0 ; y<400 ; y++, row += 80)
      png_write_row(png_ptr, row);
    break;

  default:
    emsg("internal: Invalid image format -- %s\n",pix->magic);
    goto error;
    break;
  }

  imsg("output: \"%s\" %dx%dx%d (PNG/%s) size:%d\n",
       mf.path, pix->w, pix->h,1<<(1<<pix->d),
       mypng_typestr(png_type),
       (int) ftell(mf.file));

  png_write_end(png_ptr, 0);
  ret = 0;

error:
  mf_close(&mf);
  if (!info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  if (!png_ptr)  png_destroy_write_struct(&png_ptr, (png_infopp)0);

  return ret;

png_error:
  if (errno)
    syserror(path,"png error");
  else
    emsg("libpng error -- %s\n",mf.path);
  goto error;
}

int main(int argc, char *argv[])
{
  int ecode = E_OK;
  char *ipath = 0, *opath = 0;
  myimg_t *src = 0, *cvt = 0;

  int option_index = 0, c, itype;
  static char me[] = PROGRAM_NAME;

  argv[0] = me;
  opterr = 0;                           /* Report error */
  optind = 1;                           /* Where arguments start */
  ecode = E_ARG;

  for (;;) {
    static const char sopts[] = "hV" "vq" "c:ezr" "d";
    static struct option lopts[] = {
      {"help",    no_argument,      0, 'h'},
      {"usage",   no_argument,      0, 'h'},
      {"version", no_argument,      0, 'V'},
      /**/
      {"verbose", no_argument,      0, 'v'},
      {"quiet",   no_argument,      0, 'q'},
      /**/
      {"color",   required_argument,0, 'c'},
      {"ste",     no_argument,      0, 'e'},
      {"pcx",     no_argument,      0, 'z'},
      {"pix",     no_argument,      0, 'r'},
      /**/
      {"same-dir",no_argument,      0, 'd'},
      /**/
      {0, 0, 0, 0}
    };

    /* getopt_long stores the option index here. */

    c = getopt_long(argc, argv, sopts, lopts, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;
    switch (c)
    {
    case 'h': print_usage(opt_bla); ecode = E_OK; goto exit;
    case 'V': print_version(); ecode = E_OK; goto exit;
      /**/
    case 'v': opt_bla++; break;
    case 'q': opt_bla--; break;
      /**/
    case 'c': {
      int i;
      static struct { char s[3], m; } modes[] = {
        { "3z", CQ_STF|CQ_000 },
        { "3r", CQ_STF|CQ_LBR },
        { "3f", CQ_STF|CQ_FDR },
        { "4z", CQ_STE|CQ_000 },
        { "4r", CQ_STE|CQ_LBR },
        { "4f", CQ_STE|CQ_FDR },
      };

      for ( i=0; i<sizeof(modes)/sizeof(*modes); ++i )
        if ( ! strcasecmp(modes[i].s, optarg ) ) {
          opt_col = modes[i].m; i = -1; break;
        }
      if (i>0) {
        emsg("invalid argument for -c/--color -- `%s'\n",optarg);
        goto exit;
      }
    } break;
    case 'e': opt_col = CQ_STE|CQ_LBR; break;

    case 'z':
      if (opt_out == PXX || opt_out == PCX)
        opt_out = PCX;
      else {
        emsg("option `-z' and `-r' are exclusive\n");
        goto exit;
      }
      break;

    case 'r': opt_out = PIX; break;
      if (opt_out == PXX || opt_out == PIX)
        opt_out = PIX;
      else {
        emsg("option `-z' and `-r' are exclusive\n");
        goto exit;
      }
      break;

      /**/
    case 'd': opt_dir = 1; break;
    case 000: break;
    case '?':
      if (!opterr) {
        if (optopt)
          emsg("unknown option -- `%c' (%d)\n",
               isgraph(optopt)?optopt:'.', optopt);
        else
          emsg("unknown option -- `%s'\n", argv[optind-1]);
      }
      goto exit;
      break;

    default:
      emsg("unexpected option -- `%c' (%d)\n",
           isgraph(c)?c:'.', c);
      ecode = E_INT;
      goto exit;
    }
  }

  if (optind < argc)
    ipath = argv[optind++];
  else {
    emsg("too few arguments. Try --help.\n");
    goto exit;
  }

  if (optind < argc)
    opath = argv[optind++];

  if (optind < argc) {
    emsg("too many arguments. Try --help.\n");
    goto exit;
  }

  set_color_mode(opt_col);

  /* ----------------------------------------
     Read the input image file.
     ---------------------------------------- */

  ecode = E_INP;
  if (src = read_img_file(ipath), !src)
    goto exit;
  itype = src->png.type;

  amsg("Loaded as %dx%dx%d(%d) %s/%s(%d) \"%s\"\n",
       src->png.w, src->png.h, src->png.d,
       src->png.c, src->png.magic,
       type_names[itype&3], itype,
       src->png.path );
  assert( !memcmp(src->png.magic, type_names[itype&3], 2) );

  if (itype == PNG) {
    mypng_t * const png = &src->png;

    assert( !memcmp(src->png.magic,"PNG",3) );
    imsg("input: \"%s\" %dx%dx%d PNG-%s(%d)\n",
         basename(png->path), png->w, png->h, 1<<png->d,
         mypng_typestr(png->t), png->t);
    ecode = E_PNG;
    if (cvt = mypix_from_png(png), !cvt)
      goto exit;

  }
  else {
    mypix_t * const pix = &src->pix;

    imsg("input: \"%s\" %dx%dx%d (%s)\n",
         basename(pix->path), pix->w, pix->h, 1<<pix->d, pix->magic);

    /* Default operation for P[IC][123] is PNG */
    if (opt_out == PXX)
      opt_out = PNG;
  }

  ecode = E_OUT;
  if ( save_img_as(cvt?cvt:src, opath, opt_out) )
    goto exit;

  ecode = E_OK;
exit:
  myimg_free(&src);
  myimg_free(&cvt);

  dmsg("%s: exit %d\n",PROGRAM_NAME,ecode);
  return ecode;
}


static char *create_output_path(char * ipath, const char * ext)
{
  const char * ibase = basename(ipath);
  const int l = strlen(ibase), le = strlen(ext);
  char *opath = 0, *dot = 0;

  assert( ipath );
  assert( ext );
  assert( *ext == '.' && ext[1] == 'p' );
  assert( *ipath );

  dmsg("Create output from \"%s\" (%s)\n", ipath, ext);

  if (!opt_dir) {
    if (opath = mf_strdup(ibase,l+le), !opath) return 0;
    dot = strrchr(opath, '.');
    if (dot == opath) dot = 0;        /* extension can not be start */
    if (dot == 0) dot = opath + l;    /* no extension ? dot is end */
  } else {
    char * obase;
    const int fl = strlen(ipath);
    if (opath = mf_strdup(ipath, fl+le), !opath) return 0;
    assert( fl >= l );
    obase = opath + fl - l;           /* basename() by rewinding */
    dot = strrchr(obase, '.');
    if (dot == obase) dot = 0;        /* extension can not be start */
    if (dot == 0) dot = opath + fl;   /* no extension ? dot is end */
  }
  assert( dot > opath );
  strcpy( dot, ext);


  dmsg("automatic output: \"%s\"\n", opath);
  return opath;
}

static const char * native_extension(int type, int subtype)
{
  switch (type) {
  case PNG:
    return ".png";
  case PIX:
    switch(subtype) {
    case '1': return ".pi1";
    case '2': return ".pi2";
    case '3': return ".pi3";
    }
    break;
  case PCX:
    switch(subtype) {
    case '1': return ".pc1";
    case '2': return ".pc2";
    case '3': return ".pc3";
    }
    break;

  default:
    assert( !"unexpected image type" );
    break;
  }
  return "";
}

static int guess_type_from_path(char * path)
{
  const char * ext;

  if (path && (ext = strrchr(basename(path),'.')) && strlen(ext) == 4)
    if (tolower(ext[1]) == 'p') {
      if (tolower(ext[2]) == 'n' && tolower(ext[3]) == 'g')
        return PNG;
      else if (ext[3] >= '1' && ext[3] <= '3') {
        if (tolower(ext[2]) == 'i')
          return PIX;
        else if ( tolower(ext[2]) == 'c')
          return PCX;
      }
    }
  return PXX;
}

static int save_img_as(myimg_t * img, char * path, int type)
{
  mypix_t * const pix = &img->pix;
  char * opath;
  int err = -1, guess_type;

  assert( img );
  assert( pix->type == PIX || pix->type == PCX );

  dmsg("save_img_as("
       "%dx%dx%d/%s,"
       "\"%s\","
       "%s(%d)\n",
       pix->w,pix->w,1<<(1<<pix->d),type_names[pix->type],
       path?path:"(nil)",type_names[type],type);

  guess_type = guess_type_from_path(path);
  dmsg("guessed type: %s(%d)\n",type_names[guess_type],guess_type);

  if (guess_type != PXX)
    amsg("provided output suggests %s\n", type_names[guess_type]);

  if ( type == PXX )
    /* Input was a PNG and no operation was specified. Trying to guess
     * from filename (if any) or default to PIX. */
    type = guess_type != PXX ? guess_type : PIX;

  if ( guess_type != PXX && guess_type != type )
      wmsg("provided output (%s) mismatched (%s)\n",
           type_names[guess_type], type_names[type]);

  assert( type != PXX );
  opath = path ? path :
    create_output_path(pix->path,
                       native_extension(type, img->pix.magic[2]));
  if (!opath)
    return -1;

  if (type == PNG) {
    if (save_png_as(pix, opath))
      goto exit;
  } else {
    if (save_pix_as(pix, opath, type) )
      goto exit;
  }

  err = 0;
exit:
  if (opath != path)
    free(opath);

  return err;
}

/* ----------------------------------------------------------------------
 * Version and Copyright.
 **/

static void print_version(void)
{
  puts(
    PACKAGE_STRING "\n"
    "\n"
    COPYRIGHT ".\n"
    "License GPLv3+ or later <http://gnu.org/licenses/gpl.html>\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n"
    "\n"
    "Written by Benjamin Gerard <https://github.com/benjihan>\n"
    );
}

static void print_usage(int verbose)
{
  puts(
    "Usage: " PROGRAM_NAME " [OPTION] <input> [<output>]\n"
    "\n"
    "  PNG/Degas image file converter.\n"
    "\n"
    "  Despite its name the program can use PNG,PC?,PI?\n"
    "  as both input and output. Any conversion is possible\n"
    "  as long as the image formats are compatible.\n"
    "\n"
    "OPTIONS\n"
    " -h --help --usage   Print this help message and exit.\n"
    " -V --version        Print version message and exit.\n"
    " -q --quiet          Print less messages.\n"
    " -v --verbose        Print more messages.\n"
    " -c --color=XY       Select color conversion method (see below).\n"
    " -e --ste            Alias for --color=4r.\n"
    " -z --pcx            Force output as a pc1, pc2 or pc3.\n"
    " -r --pix            Force output as a pi1, pi2 or pi3.\n"
    " -d --same-dir       Automatic save path includes <input> path.\n"
    );
  if (!verbose) {
    puts("  Add -v/--verbose prior to -h/--help for details.\n");
  } else {
    puts(
      "When creating Degas image the <input> image resolution is used to\n"
      "select the <output> type.\n"
      "\n"
      " - PI1 / PC1 images are 320x200x16 colors\n"
      " - PI2 / PC2 images are 640x200x4 colors\n"
      " - PI3 / PC3 images are 640x400x2 monochrome (B&W)\n"
      "\n"
      "Automatic output name:\n"
      "\n"
      " - If <output> is omitted the file path is created automatically.\n"
      " - If the --same-dir option is omitted the output path is the\n"
      "   current working directory. Otherwise it is the same as the input\n"
      "   file.\n"
      " - The filename part of the <output> path is the <input> filename\n"
      "   with its dot extension replaced by the output format natural dot\n"
      "   extension.\n"
      );
    puts(
      "Output type:\n"
      "\n"
      " - If --pix or --pcx is specified the <output> is respectively\n"
      " a raw (PI?) or rle compressed (PC?) Degas image whatever the\n"
      " <input>.\n"
      " - If <input> is a PNG image the default is to create a PI?\n"
      "   image unless a provided <output> suggest otherwise.\n"
      " - If <input> is a Degas  image the default is to create a PNG\n"
      "   image unless a provided <output> suggest otherwise.\n"
      " - If pngtopi1 detects a discrepancy between a provided <output>\n"
      "   filename extension and what is really going to be written then it\n"
      "   issues a warning but still process as requested. Use -q to\n"
      "   remove the warning.\n"
      );
    puts(
      "Color conversion mode:\n"
      "\n"
      " - The X parameter decides if a Degas image will use 3 or 4 bits\n"
      "   per color component.  The consequence might be the lost of a\n"
      "   precious colormap entry in some (rare) cases if the provided input\n"
      "   image was not created accordingly.\n"
      "\n"
      " - The Y parameter picks the method used to upscale 3/4 bits color\n"
      "   component to 8 bits.\n"
      "\n"
      "   | Y |       Name |          Description |           Example |\n"
      "   |---|------------|----------------------|-------------------|\n"
      "   | z | Zero-fill  | Simple Left shift.   | $3 -> 3:$60 4:$60 |\n"
      "   | r | Replicated | Replicate left bits  | $3 -> 3:$6C 4:$66 |\n"
      "   | f | Full-range | Ensure full range    | $3 -> 3:$6D 4:$66 |\n"
      );
  }
  puts(
    COPYRIGHT ".\n"
#ifdef PACKAGE_URL
    "Visit <" PACKAGE_URL ">.\n"
#endif
#ifdef PACKAGE_BUG
    "Report bugs to <" PACKAGE_BUG ">\n"
#endif
    );
}
