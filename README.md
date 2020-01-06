# <img src="logo.svg" width="400">

![Github Actions](https://github.com/Dougal-s/Vkav/workflows/build/badge.svg)

<img align="left" src="https://thumbs.gfycat.com/UnconsciousAlarmingGreatwhiteshark-size_restricted.gif" width="160" height="160" />

Vkav is an audio visualizer written in C++ using Vulkan as its rendering backend.<br/>
Shown on the left is the eclipse shader.<br/>
*MacOS and Windows support has not been implemented*
<br/>
<br/>
<br/>
<br/>

## Getting Started

### Prerequisites
* GLFW
* Vulkan
* libpng (optional)
* libjpeg (optional)
* Pulseaudio (Linux only)
* WASAPI (Windows only)\*
* Coreaudio (MacOS only)\*

### Compilation Tools:
* g++ >= 8 or clang++ >= 7
* cmake >= 3

\*Windows and MacOS support has not been implemented.

#### Debian/Ubuntu:
```
$ sudo apt install libglfw3-dev libvulkan-dev libpulse-dev libpng-dev libjpeg-dev
```

### Installing

Clone the repository and compile Vkav using:
```
$ git clone https://github.com/Dougal-s/Vkav.git
$ cd Vkav
$ mkdir build && cd build
$ cmake ..
$ make
```
and finally install Vkav using:
```
$ sudo make install
```
To install the config files to a user accessible location, run:
```
$ Vkav --install-config
```
This will copy the config files from "/etc/Vkav" to "~/.config/Vkav".

### Usage
To run Vkav simply execute:
```
$ Vkav
```

Config files can be located in "~/.config/Vkav" once the user has executed `Vkav --install-config`.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

## Authors
* Dougal Stewart - [Dougal-s](https://github.com/Dougal-s)

## Acknowledgments
This project was inspired by [GLava](https://github.com/wacossusca34/glava).
