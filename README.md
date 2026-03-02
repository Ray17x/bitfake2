# Bitfake
Bitfake was originally created to detect fake `.FLAC` files through spectral analysis. It has since grown into a multipurpose CLI tool for handling music more easily and efficiently.

One common problem was that getting a track’s metadata required long `ffprobe` commands with messy output. Converting music with `ffmpeg` was also repetitive. The command itself is easy to remember, but writing scripts to convert entire directories felt inefficient and slow. This project wraps those complex `ffmpeg`/`ffprobe` workflows into a simple, intuitive CLI.

This project is still far from complete, but it is already effective.

## Implemented Features
* Get metadata
* Get ReplayGain info (useful for music players)
* Spectral analysis on 44.1 kHz `.FLAC` files (higher sample rates may be misrepresented, so be careful)
* Lossy diagnosis (banding score)
* File Conversion (Works going TO .mp3 or .ogg)

## Implemented Development Features
These features are meant to make contributing to the project easier:
* Global header file for simpler global variable declarations
* `ConsoleOut` header file for organized output
* Two-stage metadata add/overwrite flow
* Separation of non-user, helper, and core functions for organization
* File checking functions for specific use cases (audio extension checks, magic number checks, and specific format checks)

## Yet to Be Implemented
* Directory conversion
* ReplayGain calculation by track
* Directory tagging
* MusicBrainz functionality

## Dependencies

Build-time dependencies:
* A C++17-capable compiler and build tools (`g++`, `make`)
* TagLib development headers and library
* FFTW3 development headers and library

Run-time dependencies:
* `ffmpeg` and `ffprobe` available in your `PATH`

## Installation
1. Install all dependencies

Ubuntu/Debian-based distributions:
```sh
sudo apt update && sudo apt install -y build-essential libtag1-dev libfftw3-dev ffmpeg
```

Fedora/Fedora-based distributions:
```sh
sudo dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ffmpeg
```

RHEL:
```sh
sudo dnf install -y epel-release dnf-plugins-core && sudo dnf install -y https://download1.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm && sudo dnf config-manager --set-enabled crb && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ffmpeg
```

Arch/Arch-based distributions:
```sh
sudo pacman -Syu --needed base-devel taglib fftw ffmpeg
```

Gentoo:
```sh
sudo emerge --ask sys-devel/gcc sys-devel/make media-libs/taglib sci-libs/fftw media-video/ffmpeg
```

Alpine:
```sh
sudo apk add --no-cache build-base taglib-dev fftw-dev ffmpeg
```

Overall dependency list (for other distros):
```
build-base taglib-dev fftw-dev ffmpeg
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

Note: For some commands, `-i` can also be a directory path, and Bitfake will process supported audio files inside it.

4. Cleaning up project
You can clean the project and prepare for a fresh build with:

```sh
make clean
```

You are now ready to build the project again with any additions or contributions you make.

This project is not yet released in any distribution package manager as a raw binary. If you want to run the binary as a command, copy it to your `/usr/bin` directory like this (NOT RECOMMENDED):

```sh
sudo cp ./bitf /usr/bin/
```

## Troubleshooting

`ffmpeg`/`ffprobe` not found:
* Install `ffmpeg` and make sure `ffmpeg` and `ffprobe` are available in your `PATH`.

Linker errors like `cannot find -ltag` or `cannot find -lfftw3`:
* Install the TagLib/FFTW3 *development* packages for your distro (not just the runtime libs).

Build fails due to missing headers:
* Confirm you installed TagLib and FFTW3 dev packages and are using a C++17 compiler.

1
