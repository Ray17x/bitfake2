# Bitfake

> [!NOTE]
> We have moved to [Codeberg](https://codeberg.org/Ray18x/bitfake2).

Bitfake was originally created to detect fake `.FLAC` files through spectral analysis. It has since grown into a multipurpose CLI tool for handling music more easily and efficiently.

One common problem was that getting a track's metadata required long `ffprobe` commands with messy output. Converting music with `ffmpeg` was also repetitive. The command itself is easy to remember, but writing scripts to convert entire directories felt inefficient and slow. This project now performs metadata and conversion tasks directly through linked libraries (TagLib/libsndfile/libav*).

<p align="center">
  <img src="bitfakethelogotrust.png" width="400">
</p>

# Latest Version (2.0)

Bitfake2 - 2.0 is released. This update was mainly to make future development easier by refactoring the code base to use easily readable function calls rather than store every operation under one namespace; Users get benefited with many conversion issues being fixed. VBR is now supported rather than forcing CBR on users for the `.MP3` File format.

## Implemented Features
* Get metadata
* Get ReplayGain info (useful for music players)
* Spectral analysis on 44.1 kHz `.FLAC` files (higher sample rates may be misrepresented, so be careful)
* Lossy diagnosis (banding score)
* File Conversion + VBR Support (Works for outputs like `.wav`, `.flac`, `.ogg`, `.mp3`, `.aac`, `.opus`)
* Tagging metadata (Works for single files, but not directories yet)
* Calculating ReplayGain and applying it to files (Works for track replay gain iterating through directories, album replay gain is a bit funky?)
* Directory Conversion (works for all previously mentioned formats!)
* CoverArt+ (Brings along cover art among all conversions!)
* Organizing Files by album! (Give a dir of random music, and bitfake will organize it - ty to uncognic)
* Directory Tagging (YAY!)
* Album folder renaming from tags (Artist - Album (Year))
* Spectrogram generation (in .png)
* Version info (WOW! BEST YET!)
* MusicBrainz Syncing data.
* Downloading lyrics from the internet (Synced Lyrics Preferred, leaves out as seperate file because that mattered for some reason) via LIBLRC ( no additonal dependencies )
* recursion + paralleism released. Everything is faster. `--serial` for single threaded.

> [!IMPORTANT]
> MusicBrainz is fully functional and this message will be removed upon the next tagged release. It is on live versions, please feel free to report any bugs to me after checking that musicbrainz has the metadata you are looking for.

## Implemented Development Features
These features are meant to make contributing to the project easier:
* Global header file for simpler global variable declarations
* `ConsoleOut` header file for organized output
* Two-stage metadata add/overwrite flow
* Separation of non-user, helper, and core functions for organization
* File checking functions for specific use cases (audio extension checks, magic number checks, and specific format checks)

## Yet to Be Implemented
* GPU Accelerated actions (converting, analyzing, etc.) (Likely 2.0)
* Remaking `main.cpp` so the CLI is much easier to use
* Many Fixes to converting songs
* More to be added later

## Dependencies

Build-time dependencies:
* A C++17-capable compiler and build tools (`g++`, `make`)
* TagLib development headers and library
* FFTW3 development headers and library
* libebur128 development headers and library
* libsndfile development headers and library
* libcurl development headers and library
* FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`, `libswresample`)

Run-time dependencies:
* No external `ffmpeg`/`ffprobe` CLI tools are required in `PATH`

## Installation
1. Install all dependencies

Ubuntu/Debian-based distributions:
```sh
sudo apt update && sudo apt install -y build-essential libtag1-dev libfftw3-dev libebur128-dev libsndfile1-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libcurl4-openssl-dev
```

Fedora/Fedora-based distributions:
```sh
sudo dnf install -y gcc-c++ make pkgconf-pkg-config taglib-devel fftw-devel libebur128-devel libsndfile-devel ffmpeg-free-devel libcurl-devel
```
RHEL:
```sh
sudo dnf install -y epel-release dnf-plugins-core && sudo dnf install -y https://download1.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm && sudo dnf config-manager --set-enabled crb && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ebur128-devel libsndfile-devel ffmpeg-devel libcurl-devel
```

Arch/Arch-based distributions:
```sh
sudo pacman -Syu --needed base-devel taglib fftw libebur128 libsndfile ffmpeg curl
```

Gentoo:
```sh
sudo emerge --ask sys-devel/gcc sys-devel/make media-libs/taglib sci-libs/fftw media-libs/libebur128 media-libs/libsndfile media-video/ffmpeg net-misc/curl
```

Gentoo USE flag note (for encoder support):
```sh
echo "media-video/ffmpeg encode mp3 opus vorbis" | sudo tee -a /etc/portage/package.use/bitfake
```

WAITTTTT!! Do you have the GURU enabled? You can download it straight from there instead!! :D
```sh
sudo emaint sync -a # Sync all repos just to be safe!
sudo emerge --ask app-misc/bitfake2
```

Alpine:
```sh
sudo apk add --no-cache build-base taglib-dev fftw-dev ebur128-dev libsndfile-dev ffmpeg-dev curl-dev
```

Windows (MSYS2/MinGW):
```sh
pacman -Syu --needed base-devel mingw-w64-x86_64-taglib mingw-w64-x86_64-fftw mingw-w64-x86_64-libebur128 mingw-w64-x86_64-libsndfile mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain mingw-w64-x86_64-curl

```
NixOS:

Users that are using flakes, add this to your flake.nix:
```nix
inputs.bitfake2.url = "github:Ray17x/bitfake2";
```
Then add to your system packages:
```nix
environment.systemPackages = [ inputs.bitfake2.packages.${pkgs.system}.default ];
```


Chimera Linux:

Clone the bitfake2 repo:
```sh
git clone https://github.com/Ray17x/bitfake2
```

Install the packages:
```sh
doas apk add taglib-devel fftw-devel libsndfile-devel libebur128-devel ffmpeg-devel curl-devel
```
> [!Note]
>
> These additional dependencies are required to build Bitfake2:
>
> ```sh
> doas apk add clang gmake
> ```

Once all dependencies required for Bitfake2 are installed:

```sh
make
doas make install
```

FreeBSD:

Clone the bitfake2 repo:
```sh
git clone https://github.com/Ray17x/bitfake2
```

> [!WARNING]
> `ebur128` conflicts with `libebur128`, make sure to install `libebur128`.
```sh
sudo pkg install taglib fftw3 libebur128 libsndfile ffmpeg curl
```
Once you have installed the deps:
```sh
bmake
sudo bmake install
```

DragonFly BSD:
> [!NOTE]
> Untested, but should work the same as FreeBSD.

Clone the bitfake2 repo:
```sh
git clone https://github.com/Ray17x/bitfake2
```
```sh
sudo pkg install taglib fftw3 libebur128 libsndfile ffmpeg curl
```
```sh
bmake 
sudo bmake install
```

NetBSD:

Clone the bitfake2 repo:
```sh
git clone https://github.com/Ray17x/bitfake2
```

Install the dependencies:
```sh
sudo pkgin install taglib fftw3 libebur128 libsndfile ffmpeg7 curl
```
Compile and install:
```sh
bmake 
sudo bmake install
```

OpenBSD:

Clone the bitfake2 repo:
```sh
git clone https://github.com/Ray17x/bitfake2
```

Install the dependencies:
```sh
pkg_add taglib fftw3 libebur128 libsndfile ffmpeg curl
```
Compile and install:
```sh
bmake
bmake install
```

Overall dependency list (for other distros):
```
build-base taglib-dev fftw-dev ebur128-dev libsndfile-dev libavformat libavcodec libavutil libswresample curl-dev
```

2. Clone the project and compile

Using `git clone`:
```sh
git clone --depth 1 https://github.com/Ray17x/bitfake2.git
cd bitfake2
make
```

3. Running
```sh
./bitf -h
```
(Help command to get you started!)

## Usage Examples

Print help:
```sh
./bitf -h
```

Get metadata (prints to terminal by default):
```sh
./bitf -i /path/to/song.flac -gmd
```

Get metadata and write to a text file (`-o` must be a `.txt` file):
```sh
./bitf -i /path/to/song.flac -gmd -o /tmp/metadata.txt
```

Get ReplayGain info:
```sh
./bitf -i /path/to/song.flac -grg
```

Run spectral analysis:
```sh
./bitf -i /path/to/song.flac -sa
```

Rename album subfolders to `Artist - Album (Year)` based on tags:
```sh
./bitf -i /path/to/albums -raf
```

Sort album subdirectories into artist subdirectories:
```sh
./bitf -i /path/to/albums -oaia
```


Note: For some commands, `-i` can also be a directory path, and Bitfake will process supported audio files inside it.

4. Cleaning up project

You can clean the project and prepare for a fresh build with:
```sh
make clean
```

## Code Style

This project uses `clang-format` with the repository config in `.clang-format` (same-line/K&R braces).

Format all C++ source/header files:
```sh
make format
```

Check formatting without modifying files (useful for CI):
```sh
make check-format
```

You are now ready to build the project again with any additions or contributions you make.

This project is not yet released in any distribution package manager as a raw binary. If you want to run the binary as a command, copy it to your `/usr/bin` directory like this (NOT RECOMMENDED):
```sh
sudo cp ./bitf /usr/bin/
```

## Troubleshooting

Linker errors like `cannot find -lavformat`, `cannot find -lavcodec`, `cannot find -lavutil`, or `cannot find -lswresample`:
* Install FFmpeg development packages for your distro (`ffmpeg-devel`, `ffmpeg-dev`, or `libav*-dev` packages).

Linker errors like `cannot find -ltag`, `cannot find -lfftw3`, `cannot find -lebur128`, or `cannot find -lsndfile`:
* Install the TagLib/FFTW3/libebur128/libsndfile *development* packages for your distro (not just the runtime libs).

Build fails due to missing headers:
* Confirm you installed TagLib, FFTW3, libebur128, libsndfile, and FFmpeg/libav development packages and are using a C++17 compiler.

# Contact me

* Email: ray@atl.tools
* Discord: _kerr0
* Fluxer: _k0y0#2214

# Contributor(s)
want to contribute? contact me, or make yourself seen by making a few pull requests, who knows, you might get it yourself!

<a href="https://github.com/Ray17x/bitfake2/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=Ray17x/bitfake2" />
</a>


