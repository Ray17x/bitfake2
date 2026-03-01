# Bitfake
Bitfake, originally made to only detect fake .FLAC files through spectural analysis, is a multipurpose CLI tool dedicated to handling music much easier and effieicent. A common problem I had was any time i wanted to get a music's metadata, I had to make this large ffprobe command and the output gets messy. Converting music with ffmpeg was a hassle too, command was easy to memorize, but having to create a bash script for converting an entire directory of music felt ineffiecent and slow; hence, this project is dedicated to wrapping those complex ffmpeg/ffprobe commands into a simple and intuitive CLI tool that allows you, the user, to have a much easier time handling music. This project is far from complete, but still effective. 

## Implemented Features
* Getting Metadata
* Getting ReplayGain Info (Useful for music players)
* Spectural Analysis on 44.1kHz .FLACs (Higher quality can be misrepresented, be careful!!)
* Lossy Diagonsis (Banding Score)

## Implemnted DEVELOPMENT Features
These features are for contributing to the project, to make whoever contributes to this project life just a little bit easier:
* Globals Header File to declare global variables easier
* ConsoleOut Header File makes outputs organized
* 2-Stage Metadata adding/overwriting
* Seperation of Non User, Helper, and Core Functions to remain organized
* Already implemnted file checking functions for specfic uses (Ensuring File is Audio by Extension OR by Magic Numbers, checking Specific file type)

## Yet to be implemented
* Conversion of Files / Directories
* Calculating ReplayGain by Track
* Tagging directories
* MusicBrainz Functionality

# Installation
1. Install all dependcies

Ubuntu/Debian-based distrobutions:
```sh
sudo apt update && sudo apt install -y build-essential libtag1-dev libfftw3-dev ffmpeg
```

Fedora/Fedora-based distrobutions:
```sh
sudo dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ffmpeg
```

RHEL:
```sh
sudo dnf install -y epel-release dnf-plugins-core && sudo dnf install -y https://download1.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm && sudo dnf config-manager --set-enabled crb && sudo dnf install -y gcc-c++ make taglib-devel fftw-devel ffmpeg
```

Arch/Arch-Based distrobutions:
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

Overall dependency List (for other distros):
```
build-base taglib-dev fftw-dev ffmpeg
```

2. Pulling the project and compiling

Via gitclone:

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

4. Cleaning up project
You can clean the project up and prepare to compile with your via cleaning:

```sh
make clean
```

You are now ready to build the project again with additions/contributions you make.

This project isn't yet released on any distrobutions package manager as a raw binary, if you want to run the binary as a command, copy the binary to your `/usr/bin` directory like so (NOT RECOMMENDED!!)

```sh
sudo cp ./bitf /usr/bin/
```

