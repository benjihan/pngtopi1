/**
 * @file    pngtopi1
 * @author  Benjamin Gerard <https://sourceforge.net/u/benjihan>
 * @date    2017-05-31
 * @brief   a simple png to p[ci]1/2/3 image converter
 */

/*
  ----------------------------------------------------------------------

  pngtopi1 - a simple png to p[ci]1/2/3 image converter
  Copyright (C) 2018  Benjamin Gerard

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
#  define COPYRIGHT "Copyright (c) 2018 Benjamin Gerard"
# endif

# ifndef PROGRAM_NAME
#  define PROGRAM_NAME "pngtopi1"
# endif

# ifndef PACKAGE_STRING
#  define PACKAGE_STRING PROGRAM_NAME " " __DATE__
# endif

#endif

/* ---------------------------------------------------------------------- */

/* libpng */
#include <png.h>

/* std */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ctype.h"
#include <getopt.h>
#include <errno.h>

/* ---------------------------------------------------------------------- */

/* Error codes */
enum {
  E_OK, E_ERR, E_ARG, E_INT, E_INP, E_OUT, E_PNG
};

/* For opt_pcx */
enum {
  PXX = 0, PIX = 1, PCX = 2
};

/* RGB component mask */
enum {
  STF_COL = 0xE0,
  STE_COL = 0xF0,
};

/* Degas magic word */
enum {
  DEGAS_PI1 = 0x0000,
  DEGAS_PI2 = 0x0001,
  DEGAS_PI3 = 0x0002,
  DEGAS_PC1 = 0x8000,
  DEGAS_PC2 = 0x8001,
  DEGAS_PC3 = 0x8002,
};

typedef struct mypng_s mypng_t;
struct mypng_s {
  int w, h, d, i, t, c, f, z, p;
  png_structp png;
  png_colorp  lut;
  int         lutsz;
  png_infop   inf;
  png_bytep  *rows;
};

typedef struct pic_s pic_t;
struct pic_s {
  int w, h, d;                          /* width, height, bit-planes */
  uint8_t bits[1];
};

typedef struct colorcount_s colcnt_t;
struct colorcount_s {
  unsigned int rgb:12, cnt:20;
};
static colcnt_t g_colcnt[0x1000];

/* ---------------------------------------------------------------------- */

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

static void print_usage(void)
{
  puts(
    "Usage: " PROGRAM_NAME " [OPTION] <input.png> [output]\n"
    "\n"
    "  A simple PNG to Atari-ST Degas image converter.\n"
    "  Despite its name this program can create P[IC][123] images.\n"
    "\n"
    "OPTIONS\n"
    " -h --help --usage   Print this help message and exit.\n"
    " -V --version        Print version message and exit.\n"
    " -q --quiet          Print less messages\n"
    " -v --verbose        Print more messages\n"
    " -e --ste            Use STE color quantization (4 bits per component)\n"
    " -z --compress       force RLE compressed image (pc1,pc2 or pc3)\n"
    " -r --raw            force RAW image (pi1,pi2 or pi3)\n"
    "\n"
    "If output is omitted the file path is created automatically.\n"
    "\n"
    COPYRIGHT ".\n"
#ifdef PACKAGE_URL
    "Visit <" PACKAGE_URL ">.\n"
#endif
#ifdef PACKAGE_BUG
    "Report bugs to <" PACKAGE_BUG ">\n"
#endif
    );
}

/* ---------------------------------------------------------------------- */

static int opt_ste = STF_COL;  /* mask used color bits (default STf)*/
static int opt_pcx = PXX;      /* {PXX,PIX,PCX} (see enum) */
static int opt_bla = 0;        /* blah blah level */

/* debug message (-vv) */
static void dmsg(char * fmt, ...)
{
#ifdef DEBUG
  if (opt_bla >= 2) {
    va_list list;
    va_start(list, fmt);
    vfprintf(stdout,fmt,list);
    fflush(stdout);
    va_end(list);
  }
#endif
}

/* additional message (-v) */
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
static void imsg(char * fmt, ...)
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
static void wmsg(char * fmt, ...)
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
static void emsg(char * fmt, ...)
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

static void syserror(const char * iname, const char * alt)
{
  const char * estr = errno ? strerror(errno) : alt;
  if (iname)
    emsg("%s -- %s\n", estr, iname);
  else
    emsg("%s\n", estr);
}

static void notpng(const char * path, const char * reason)
{
  emsg("not a PNG (%s) -- %s\n", reason, path);
}

static void pngerror(const char * path)
{
  syserror(path,"libpng error");
}

/* ----------------------------------------------------------------------
 * STf/STe color conversion
 * ---------------------------------------------------------------------- */

static inline unsigned int ror4(unsigned int c, int shift)
{
  c >>= shift;
  return (((c&1)<<3) | ((c>>1)&7)) << shift ;
}

static inline uint16_t ror444(uint16_t rgb4)
{
  return ror4(rgb4,8) | ror4(rgb4,4)  | ror4(rgb4,0);
}

static inline uint16_t col444(uint8_t r, uint8_t g, uint8_t b)
{
  return ( (r&opt_ste)<<4) | (g&opt_ste) | ((b&opt_ste)>>4);
}

/* ---------------------------------------------------------------------- */

static mypng_t * mypng_init(void)
{
  mypng_t * png = calloc(1, sizeof(*png));
  return png;
}

static void mypng_free(mypng_t * png)
{
  if (!png) return;
  free(png);
}

static mypng_t * read_png_file(char * iname)
{
  png_byte header[8];
  FILE * fp = 0;
  size_t n;
  mypng_t * png = 0;
  int y;

  if (fp = fopen(iname, "rb"), !fp) {
    syserror(iname,"open error");
    goto exit;
  }

  if (n = fread(header, 1, 8, fp), n != 8) {
    if (n == -1)
      syserror(iname,"read error");
    else
      notpng(iname, "too short");
    goto error;
  }

  if (png_sig_cmp(header, 0, 8)) {
    notpng(iname, "no signature");
    goto error;
  }

  png = mypng_init();
  if (!png) {
    syserror(iname,"alloc error");
    goto error;
  }

  png->png = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  if (!png->png)
    goto png_error;

  png->inf = png_create_info_struct(png->png);
  if (!png->inf)
    goto png_error;

  if (setjmp(png_jmpbuf(png->png)))
    goto png_error;

  png_init_io(png->png,fp);
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

  /* GB: not sure if we handle that or not ? */
#if 0
  if (png->i != PNG_INTERLACE_NONE) {
    emsg("invalid PNG interlacing mode -- %d\n", png->i);
    goto error;
  }
 #endif

  /* read file */
  if (setjmp(png_jmpbuf(png->png)))
    goto png_error;

  png_get_PLTE(png->png, png->inf, &png->lut, &png->lutsz);
  if (png->lutsz) {
    int i;
    amsg("PNG color look-up table has %d entries:\n", png->lutsz);
    for (i=0; i<png->lutsz; ++i)
      amsg("%3d #%02X%02X%02X $%03x%s\n",
           i,png->lut[i].red,png->lut[i].green,png->lut[i].blue,
           col444(png->lut[i].red,png->lut[i].green,png->lut[i].blue),
           1 ? "" : " !");
  }

  png->rows = (png_bytep *)
    png_malloc(png->png, png->h*(sizeof (png_bytep)));

  for (y=0; y<png->h; ++y)
    png->rows[y] = (png_byte *)
      png_malloc(png->png, png_get_rowbytes(png->png,png->inf));

  png_read_image(png->png, png->rows);

  goto exit;

png_error:
  pngerror(iname);
  goto error;

error:
  mypng_free(png);
  png = 0;
exit:
  if (fp) fclose(fp);
  return png;
}


static char * mypng_typestr(mypng_t * png)
{
# define CASE_COLOR_TYPE(A) case PNG_COLOR_TYPE_##A: return #A
# define CASE_COLOR_MASK(A) case PNG_COLOR_MASK_##A: return #A
  static char s[8];
  switch (png->t) {

    CASE_COLOR_TYPE(GRAY);           /* (bit depths 1, 2, 4, 8, 16) */
    CASE_COLOR_TYPE(GRAY_ALPHA);     /* (bit depths 8, 16) */
    CASE_COLOR_TYPE(PALETTE);        /* (bit depths 1, 2, 4, 8) */
    CASE_COLOR_TYPE(RGB);            /* (bit_depths 8, 16) */
    CASE_COLOR_TYPE(RGB_ALPHA);      /* (bit_depths 8, 16) */

    /* CASE_COLOR_MASK(PALETTE); */
    /* CASE_COLOR_MASK(COLOR); */
    /* CASE_COLOR_MASK(ALPHA); */
  }
  sprintf(s,"?%02x?", png->t & 255);
  return s;
}

#if 0
static char * mypng_ilacestr(mypng_t * png)
{
  static char s[8];
  switch (png->i) {
  case PNG_INTERLACE_NONE: return "NONE";
  case PNG_INTERLACE_ADAM7: return "ADAM7";
  }
  sprintf(s,"?%02x?", png->i & 255);
  return s;
}
#endif

/* ----------------------------------------------------------------------
 * PNG get pixels functions
 * ---------------------------------------------------------------------- */

typedef uint16_t (*get_f)(mypng_t*, int, int);

static uint16_t get_gray1(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 1);
  assert(png->t == PNG_COLOR_TYPE_GRAY);
  assert(png->c == 1);
  const png_byte g1 = 1 & ( png->rows[y][x>>3] >> ((x&7)^7) );
  const png_byte g8 = -g1;
  return col444(g8,g8,g8);
}

static uint16_t get_gray4(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 8);
  assert(png->t == PNG_COLOR_TYPE_GRAY);
  assert(png->c == 1);
  const png_byte g4 = 15 & ( png->rows[y][x>>1] >> (((x&1)^1) << 2) );
  const png_byte g8 = (g4<<4)|g4;
  return col444(g8,g8,g8);
}

static uint16_t get_gray8(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 8);
  assert(png->t == PNG_COLOR_TYPE_GRAY);
  assert(png->c == 1);
  const png_byte g8 = png->rows[y][x];
  return col444(g8,g8,g8);
}

static uint16_t get_indexed4(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 4);
  assert(png->t == PNG_COLOR_TYPE_PALETTE);
  assert(png->c == 1);
  const png_byte idx = 15 & ( png->rows[y][x>>1] >> (((x&1)^1) << 2) );
  png_color rgb;
  assert (png->lut);
  assert (idx < png->lutsz);
  rgb = png->lut[idx];
  return col444(rgb.red,rgb.green,rgb.blue);
}

static uint16_t get_indexed8(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 8);
  assert(png->t == PNG_COLOR_TYPE_PALETTE);
  assert(png->c == 1);
  const png_byte idx = png->rows[y][x];
  png_color rgb;

  assert (png->lut);
  assert (idx < png->lutsz);

  rgb = png->lut[idx];

  return col444(rgb.red,rgb.green,rgb.blue);
}

static uint16_t get_rgb(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 8);
  assert(png->t == PNG_COLOR_TYPE_RGB);
  assert(png->c == 3);

  const png_byte r = png->rows[y][x*3+0];
  const png_byte g = png->rows[y][x*3+1];
  const png_byte b = png->rows[y][x*3+2];

  return col444(r,g,b);
}

static uint16_t get_rgba(mypng_t * png, int x, int y)
{
  assert(x < png->w);
  assert(y < png->h);
  assert(png->d == 8);
  assert(png->t == PNG_COLOR_TYPE_RGBA);
  assert(png->c == 4);

  const png_byte r = png->rows[y][x*4+0];
  const png_byte g = png->rows[y][x*4+1];
  const png_byte b = png->rows[y][x*4+2];

  return col444(r,g,b);
}


/* ---------------------------------------------------------------------- */

static int cc_cmp(const void * _a, const void * _b)
{
  const colcnt_t * const a = _a;
  const colcnt_t * const b = _b;
  return b->cnt - a->cnt;
}

void sort_colorcount(colcnt_t * cc)
{
  qsort(cc, 0x1000, sizeof(colcnt_t), cc_cmp);
}

static void pic_free(pic_t * pic)
{
  free(pic);
}

static pic_t * pic_init(mypng_t * png)
{
  static struct {
    int d,c,t;                          /* depth,channel,type */
    get_f get;                          /* get pixel function */
  } *s, supported[] = {
    /* 320 x 200 */
    { 1, 1, PNG_COLOR_TYPE_GRAY,    get_gray1    },
    { 4, 1, PNG_COLOR_TYPE_GRAY,    get_gray4    },
    { 8, 1, PNG_COLOR_TYPE_GRAY,    get_gray8    },
    { 4, 1, PNG_COLOR_TYPE_PALETTE, get_indexed4 },
    { 8, 1, PNG_COLOR_TYPE_PALETTE, get_indexed8 },
    { 8, 3, PNG_COLOR_TYPE_RGB,     get_rgb      },
    { 8, 4, PNG_COLOR_TYPE_RGBA,    get_rgba     },
    /**/
    { 0 }
  };

  pic_t * pic = 0;
  uint8_t * bits;
  int x, y, z, log2plans, lutsize, lutmax, bytes_per_row, ncolors;
  const int bytes_per_plan = ( (15+png->w) >> 4 ) << 1;
  uint16_t lut[16], magic;
  colcnt_t * const colcnt = g_colcnt;

  if (png->w == 320 && png->h == 200) {
    log2plans = 2;
    lutsize   = 16;
    magic     = DEGAS_PI1;
  }
  else if (png->w == 640 && png->h == 200) {
    log2plans = 1;
    lutsize   = 4;
    magic     = DEGAS_PI2;
  }
  else if (png->w == 640 && png->h == 400) {
    log2plans = 0;
    lutsize   = 0;
    magic     = DEGAS_PI3;
  }
  else
    return 0;
  lutmax = 1<<(1<<log2plans);

  for (s=supported; s->d; ++s)
    if (s->d == png->d && s->c == png->c && s->t == png->t)
      break;

  if (!s->get)
    return 0;

  /* count color occurrences */
  for (x=0; x < 0x1000; ++x) {
    colcnt[x].rgb = x;
    colcnt[x].cnt = 0;
  }
  for (y=0; y < png->h; ++y)
    for (x=0; x < png->h; ++x)
      ++ colcnt[ s->get(png,x,y) ].cnt;

  sort_colorcount(colcnt);
  for (x=0; x<0x1000 && colcnt[x].cnt; ++x)
    amsg("color #%02d $%03X %+6d\n", x, colcnt[x].rgb, colcnt[x].cnt);

  if (x > lutmax) {
    emsg("too many colors -- %d > %d", x, lutmax);
    return 0;
  }

  for (y=0; y < x; ++y)
    lut[y] = colcnt[y].rgb;
  ncolors = y;
  assert(ncolors == x);
  for ( ; y < lutsize; ++y)
    lut[y] = -1;

  bytes_per_row  = bytes_per_plan << log2plans;
  pic = calloc(1, sizeof( pic_t )
               + 2                      /* PI1 header  */
               + 2*lutsize              /* PI1 palette */
               + (png->h*bytes_per_row) /* PI1 pixels  */
    );
  if (!pic) {
    syserror(0,"out image alloc error");
    return 0;
  }
  pic->w = png->w;
  pic->h = png->h;
  pic->d = log2plans;
  bits   = pic->bits;

  /* Degas signature */
  *bits ++ = magic >> 8;
  *bits ++ = magic;

  /* Copy palette 16 entries for all formats ! */
  for (y=0; y<lutsize; ++y) {
    unsigned col = ror444(lut[y]);
    *bits++ = col >> 8;
    *bits++ = col;
  }
  bits += (16-lutsize) << 1;

  /* Blit pixels */

  /* Use color occurence array as reverse table */
  for (y=0; y < ncolors; ++y) {
    int rgb = lut[y];
    assert(rgb < 0x1000);
    colcnt[rgb].rgb = y;
  }

  /* Per row */
  for (y=0; y < pic->h; ++y) {
    /* Per 16-pixels block */
    for (x=0; x < pic->w; x+=16) {
      /* Per bit-plan */
      for (z=0; z<(1<<pic->d); ++z) {
        unsigned bm, bv;
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
  assert(bits == pic->bits+32034);

  return pic;
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

/**
 *  RLE encode a single plan of a row.
 *
 * @param  dst destination buffer or 0 for counting only.
 * @param  src source buffer (
 * @param  cnt number of 16-bit word to encode
 * @return encoded size in bytes
 *
 * RLE coding:
 *
 * code: 00..7f copy code+1 byte [1..128]
 *       80..ff repeat next byte 257-code times [2..129]
 */
static int
enc_cpy(uint8_t * d, const uint8_t * s, int l)
{
  const uint8_t * const d0 = d;
  while (l > 0) {
    int n = l;
    if (n > 128) n = 128;
    l -= n;
    if (d0) {
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
enc_rpt(uint8_t * d, const uint8_t v, int l)
{
  const uint8_t * const d0 = d;
  while (l >= 2) {
    int n = l;
    if (n > 129)
      n = 129 - (n==130);            /* can't have only 1 remaining */
    l -= n;
    if (d0) {
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
        j += enc_cpy(dst+j, src+o, i-o);
        o = i;
      }
      j += enc_rpt(dst+j, c, l);
      o = i = k;
    }
    i = k;
  }
  j += enc_cpy(dst+j, src+o, i-o);

  print_buffer(dst, j, "ENC");

  return j;
}

static int
rle_decode(uint8_t * d, const uint8_t * s, int l)
{
  int i,j;

  for (i=j=0; i<l; ) {
    int c = s[i++];
    if (c < 128) {
      /* Copy c+1 byte */
      for (++c; c; --c)
        d[j++] = s[i++];
    } else {
      /* Repeat 257-c times next byte */
      uint8_t v = s[i++];
      for (c=257-c; --c >= 0;)
        d[j++] = v;
    }
  }
  print_buffer(d,j,"DEC");
  assert(i==l);
  return j;
}

/**
 * Save PC? image file.
 *
 * @param  out  output FILE
 * @param  pic  picture
 * @retval -1   on error
 * @retval >0   number of bytes saved
 */
static int save_pcx(FILE * out, pic_t * pic)
{
  const int bpr = (pic->w>>4) << (pic->d+1); /* bytes per row */
  const int off = 2 << pic->d;       /* offset to next word in plan */
  int x,y,z;

  uint8_t rle[128], raw[80], *r;
  const uint8_t * pix = pic->bits+34;

  /* Write header */
  pic->bits[0] = DEGAS_PC1 >> 8;        /* MSB is 0x80 for PC?  */
  if (fwrite(pic->bits,1,34,out) != 34)
    return -1;

  /* De-interleave into raw[] and RLE encode into rle[] */

  /* For each row */
  for (y=0; y<pic->h; ++y, pix += bpr) {
    /* For each plan */
    for (z=0; z<(1<<pic->d); ++z) {
      int l;
      const uint8_t * row = pix + (z<<1);
      /* For each 16-pixels block */
      for (x=0, r=raw; x<pic->w>>4; ++x) {
        *r++ = row[0];
        *r++ = row[1];
        row += off;
      }
      l = pcx_encode_row(rle, raw, r-raw);

#ifdef DEBUG
      uint8_t xxx[128];
      int lx = rle_decode(xxx, rle, l);
      assert(lx == (bpr >> pic->d));
      assert(!memcmp(raw,xxx,lx));
#endif
      if (fwrite(rle,1,l,out) != l)
        return -1;
    }
  }
  return (int) ftell(out);
}

/* ---------------------------------------------------------------------- */

/**
 * Save PI? image file.
 *
 * @param  out  output FILE
 * @param  pic  picture
 * @retval -1   on error
 * @retval >0   number of bytes saved
 */
static int save_pix(FILE * out, const pic_t * const pic)
{
  const int l = pic->h * ( (pic->w>>4) << (pic->d+1) );
  assert(l==32000);
  return fwrite(pic->bits,1,34+l,out) != 34+l
    ? -1
    : (int)ftell(out)
    ;
}

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
  int ecode = E_OK;

  char * iname = NULL;
  char * oname = NULL;
  mypng_t * png = 0;
  pic_t * pic = 0;
  FILE * out = 0;
  int option_index = 0, c;

  static char me[] = PROGRAM_NAME;

  argv[0] = me;
  opterr = 0;                         /* Report error */
  optind = 1;                         /* Where arguments start */
  ecode = E_ARG;
  for (;;) {
    static const char sopts[] = "hV" "vq" "ezr";
    static struct option lopts[] = {
      {"help",    no_argument, 0, 'h'},
      {"usage",   no_argument, 0, 'h'},
      {"version", no_argument, 0, 'V'},
      /**/
      {"verbose", no_argument, 0, 'v'},
      {"quiet",   no_argument, 0, 'q'},
      /**/
      {"ste",     no_argument, 0, 'e'},
      {"compress",no_argument, 0, 'z'},
      {"raw",     no_argument, 0, 'r'},
      /* */
      {0, 0, 0, 0}
    };

    /* getopt_long stores the option index here. */

    c = getopt_long(argc, argv, sopts, lopts, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;
    switch (c)
    {
    case 'h': print_usage(); goto exit;
    case 'V': print_version(); goto exit;
      /**/
    case 'v': opt_bla++; break;
    case 'q': opt_bla--; break;
      /**/
    case 'e': opt_ste = STE_COL; break;
    case 'z': opt_pcx = PCX; break;
    case 'r': opt_pcx = PIX; break;
      /**/
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
           PROGRAM_NAME, isgraph(c)?c:'.', c);
      ecode = E_INT;
      goto exit;
    }
  }

  if (optind < argc)
    iname = argv[optind++];
  else {
    emsg("too few arguments. Try --help.\n", PROGRAM_NAME);
    goto exit;
  }

  if (optind < argc)
    oname = argv[optind++];

  if (optind < argc) {
    emsg("too many arguments. Try --help.\n", PROGRAM_NAME);
    goto exit;
  }

  /* ----------------------------------------
     Read the PNG file.
     ---------------------------------------- */

  ecode = E_INP;
  if (png = read_png_file(iname), !png)
    goto exit;
  imsg("input: \"%s\" %dx%dx%d type:%s(%d) chans:%d lut:%d\n",
       iname,
       png->w, png->h, png->d,
       mypng_typestr(png), png->t,
       png->c, png->lutsz
    );

  /* GB: Not sure if we can handle that ? */

  /* Create PI? like bitmap form the PNG */
  ecode = E_PNG;
  pic = pic_init(png);
  if (!pic) {
    emsg("unsupported image conversion\n");
    goto exit;
  }

  ecode = E_OUT;

  /* When given an output filename but compression was not specified
   * try to guess the compression from the filename extension.
   */
  if (oname && opt_pcx == PXX) {
    const char * ext = strrchr(oname,'.');
    if (ext && strlen(ext)==4) {
      const char p = tolower(ext[1]);
      const char c = tolower(ext[2]);
      const char x = ext[3];
      opt_pcx = (p == 'p' && c == 'c' && x >= '1' && x <= '3')
        ? PCX
        : PIX;
    }
  }

  if (!oname) {
    int l = strlen(iname);
    oname = malloc(l+4);
    if (!oname) {
      syserror("output path","alloc error");
      goto exit;
    }
    strcpy(oname,iname);
    if (l < 4 || strcasecmp(oname+l-4,".png")) {
      strcat(oname,".pic");
    }
    oname[l-2] = "ic"[opt_pcx == PCX];
    oname[l-1] = '3'-pic->d;
  }

  out = fopen(oname,"wb");
  if (!out) {
    syserror(oname,"output open error");
    goto exit;
  } else {
    int n = (opt_pcx == PCX)
      ? save_pcx(out,pic)
      : save_pix(out,pic)
      ;
    if (n < 0) {
      syserror(oname,"write error");
      goto exit;
    }
    imsg("output: \"%s\" %dx%dx%d size:%d\n",
         oname, pic->w,pic->h,1<<pic->d,(unsigned)n);
    if (-1 == fflush(out)) {
      syserror(oname,"flush error");
      goto exit;
    }
  }

  ecode = E_OK;
exit:
  if (out) fclose(out);
  mypng_free(png);
  pic_free(pic);
  return ecode;
}
