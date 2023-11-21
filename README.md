# pngtopi1 - PNG/Degas image file converter

-------------------------------------------

### Usage

     pngtopi1 [OPTIONS] <input> [<output>]


#### Options

| Short|      Long options|                                 Description|
|------|------------------|--------------------------------------------|
| `-h` | `--help --usage` | Print this help message and exit           |
| `-V` | `--version`      | Print version message and exit             |
| `-q` | `--quiet`        | Print less messages                        |
| `-v` | `--verbose`      | Print more messages                        |
| `-c` | `--color=XY`     | Select color conversion method (see below) |
| `-e` | `--ste`          | STE colors (short for `--color=4r`)        |
| `-z` | `--pcx`          | Force output as a pc1, pc2 or pc3          |
| `-r` | `--pix`          | Force output as a pi1, pi2 or pi3          |
| `-d` | `--same-dir`     | automatic save path includes `input` path  |

  When creating Degas image the `<input>` image resolution is used to
  select the `<output>` type.
  
  - `PI1` / `PC1` images are `320x200x16` colors
  - `PI2` / `PC2` images are `640x200x4` colors
  - `PI3` / `PC3` images are `640x400x2` monochrome (black and white)


#### Automatic output name

  - If `<output>` is omitted the file path is created automatically.
  - If the `--same-dir` option is omitted the output path is the
    current working directory. Otherwise it is the same as the input
    file.
  - The filename part of the `<output>` path is the `<input>` filename
    with its dot extension replaced by the output format natural dot
    extension.


#### Output type

  - If `--pix` or `--pcx` is specified the `<output>` is respectively
    a raw (`PI?`) or RLE compressed (`PC?`) Degas image whatever the
    `<input>`.
  - If `<input>` is a `PNG` image the default is to create a `PI?`
    image unless a provided `<output>` suggest otherwise.
  - If `<input>` is a Degas  image the default is to create a `PNG`
    image unless a provided `<output>` suggest otherwise.
  - If `pngtopi1` detects a discrepancy between a provided `<output>`
    filename extension and what is really going to be written then it
    issues a warning but still process as requested. Use `-q` to
    remove the warning.


#### Color conversion mode (`--color`)

  - The `X` parameter decides if a Degas image will use 3 or 4 bits
    per color component.  The consequence might be the lost of a
    precious colormap entry in some (rare) cases if the provided input
    image was not created accordingly.
  - The `Y` parameter picks the method used to upscale 3/4 bits color
    component to 8 bits.
    
	| `Y` |       Name |         Description |             Example |
    |-----|------------|---------------------|---------------------|
    | `z` | Zero-fill  | Simple Left shift   | `$5 -> 3:$40 4:$50` |
    | `r` | Replicated | Replicate left bits | `$5 -> 3:$49 4:$55` |
    | `f` | Full-range | Ensure full range   | `$5 -> 3:$48 4:$55` |


---------------------------------------------------------------------

### Build

    make D=0

Or alternatively have a look at the _build directory:

    cd _build/i686-w64-mingw32
    ../compile

#### Makefile variables

|   Variable   | Description         | Default value                      |
|--------------|---------------------|------------------------------------|
| `PKGCONFIG`  | pkg-config program  | `pkg-config`                       |
| `PKGFLAGS`   | pkg-config flags    | *undefined*                        |
| `PNGVERSION` | libpng version      | `$(PKGCONFIG) libpng --modversion` |
| `PNGCFLAGS`  | libpng CFLAGS       | `$(PKGCONFIG) libpng --cflags`     |
| `PNGLIBS`    | libpng LDLIBS       | `$(PKGCONFIG) libpng --libs`       |
| `prefix`     | install location    | *undefined*                        |
| `datadir`    | data files location | `$(prefix)/share`                  |
| `D=1`        | Compile with debug  | `0`
