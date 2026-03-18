# Bitfake
Bitfake was originally created to detect fake `.FLAC` files through spectral analysis. It has since grown into a multipurpose CLI tool for handling music more easily and efficiently.

One common problem was that getting a track’s metadata required long `ffprobe` commands with messy output. Converting music with `ffmpeg` was also repetitive. The command itself is easy to remember, but writing scripts to convert entire directories felt inefficient and slow. This project now performs metadata and conversion tasks directly through linked libraries (TagLib/libsndfile/libav*).
<p align="center">
  <img src="bitfakethelogotrust.png" width="500">
</p>

# Latest Version (1.5)

WOW! 1.5 is here. Pretty small update in terms of user space features: one additional feature of being able to generate spectrograms from audio files. (It kind of mimicks how speks formats theirs). Big update actually comes from the fucking lines of code added to the project. Spectrogram should be pretty useful. Enjoy this sample image: 

<p align="center">
  <img src="samplespectrogram.png" width="300">
</p>

Nothing much more was added. Sorry to disappoint those who were expecting a lot more. Even tho this mf has no users.

## Implemented Features
* Get metadata
* Get ReplayGain info (useful for music players)
* Spectral analysis on 44.1 kHz `.FLAC` files (higher sample rates may be misrepresented, so be careful)
* Lossy diagnosis (banding score)
* File Conversion (Works for outputs like `.wav`, `.flac`, `.ogg`, `.mp3`, `.aac`, `.opus`)
* Tagging metadata (Works for single files, but not directories yet)
* Calculating ReplayGain and applying it to files (Works for track replay gain iterating through directories, album replay gain is a bit funky?)
* Directory Conversion (works for all previously mentioned formats!)
* CoverArt+ (Brings along cover art among all conversions!)
* Organzing Files by album! (Give a dir of random music, and bitfake will organize it - ty to uncognic)
* Directory Tagging (YAY!)
* Spectrogram generation (in .png)
* Version info (WOW! BEST YET!)

## Implemented Development Features
These features are meant to make contributing to the project easier:
* Global header file for simpler global variable declarations
* `ConsoleOut` header file for organized output
* Two-stage metadata add/overwrite flow
* Separation of non-user, helper, and core functions for organization
* File checking functions for specific use cases (audio extension checks, magic number checks, and specific format checks)

## Yet to Be Implemented
* MusicBrainz functionality
* Bandcamp functionality/features

## Dependencies

Build-time dependencies:
* A C++17-capable compiler and build tools (`g++`, `make`)
* TagLib development headers and library
* FFTW3 development headers and library
* libebur128 development headers and library
* libsndfile development headers and library
* FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`, `libswresample`)

Run-time dependencies:
* No external `ffmpeg`/`ffprobe` CLI tools are required in `PATH`

## Installation
1. Install all dependencies

Ubuntu/Debian-based distributions:
```sh
sudo apt update && sudo apt install -y build-essential libtag1-dev libfftw3-dev libebur128-dev libsndfile1-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
```

Fedora/Fedora-based distributions:
```sh
sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ebur128-devel libsndfile-devel ffmpeg-devel
```

RHEL:
```sh
sudo dnf install -y epel-release dnf-plugins-core && sudo dnf install -y https://download1.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm && sudo dnf config-manager --set-enabled crb && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ebur128-devel libsndfile-devel ffmpeg-devel
```

Arch/Arch-based distributions:
```sh
sudo pacman -Syu --needed base-devel taglib fftw libebur128 libsndfile ffmpeg
```

Gentoo:
```sh
sudo emerge --ask sys-devel/gcc sys-devel/make media-libs/taglib sci-libs/fftw media-libs/libebur128 media-libs/libsndfile media-video/ffmpeg
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
sudo apk add --no-cache build-base taglib-dev fftw-dev ebur128-dev libsndfile-dev ffmpeg-dev
```

Overall dependency list (for other distros):
```
build-base taglib-dev fftw-dev ebur128-dev libsndfile-dev libavformat libavcodec libavutil libswresample
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
* Install FFmpeg development packages for your distro (`ffmpeg-devel`, `ffmpeg-dev`, or `libav* -dev` packages).

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
