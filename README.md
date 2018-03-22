# pngtopi1

## a simple PNG to Atari-ST Degas image converter

  pngtopi1 supports creating PI1/PI2/PI3/PC1/PC2/PC3 form various png type.

  The original image resolution is used to select the kind of output.
  * `PI1`/`PC1` images are 16 colors 320x200
  * `PI1`/`PC1` images are 4 colors 640x200
  * `PI3`/`PC3` images are black and white 640x400

### Usage

     pngtopi1 [OPTION] <input.png> [output]

#### Options

 | Opts | Long options     | Description                                   |
 |------|------------------|-----------------------------------------------|
 | `-h` | `--help --usage` | Print this help message and exit              |
 | `-V` | `--version`      | Print version message and exit                |
 | `-q` | `--quiet`        | Print less messages                           |
 | `-v` | `--verbose`      | Print more messages                           |
 | `-e` | `--ste`          | Use STE color quantization (4 bits/component) |
 | `-z` | `--compress`     | force RLE compressed image (pc1,pc2 or pc3)   |
 | `-r` | `--raw`          | force RAW image (pi1,pi2 or pi3)              |

If `output` is omitted the file path is created automatically.


### Build

    make

#### Makefile variables

 | Variable   | Description        | Default value                      |
 |------------|--------------------|------------------------------------|
 | PKGCONFIG  | pkg-config program | `pkg-config`                       |
 | PNGVERSION | libpng version     | `$(PKGCONFIG) libpng --modversion` |
 | PNGCFLAGS  | libpng CFLAGS      | `$(PKGCONFIG) libpng --cflags`     |
 | PNGLIBS    | libpng LDLIBS      | `$(PKGCONFIG) libpng --libs`       |
