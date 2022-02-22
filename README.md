# <img src="logo.svg" width="400">

![Github Actions](https://github.com/Dougal-s/Vkav/workflows/build/badge.svg)

<img align="left" src="https://thumbs.gfycat.com/UnconsciousAlarmingGreatwhiteshark-size_restricted.gif" width="160" height="160" />

Vkav is an audio visualizer written in C++ using Vulkan as its rendering backend.<br/>
Shown on the left is the eclipse shader.<br/>
*Windows works to some extent.*
<br/>
<br/>
<br/>
<br/>

## Getting Started
Binaries can be found under [releases](https://github.com/Dougal-s/Vkav/releases) or on the Vkav [website](https://dougal-s.github.io/Vkav/).

### Prerequisites
* GLFW (optional, default included)
* Vulkan
* libpng (optional)
* libjpeg (optional)
* Pulseaudio (Linux only)
* PortAudio (Windows only atm, included)
* Libsoundio (optional)
* X11 (optional)

### Compilation Tools:
* g++ >= 8 or clang++ >= 7
* cmake >= 3.12


#### Debian/Ubuntu:
Install the required dependencies by running:
```
$ sudo apt install libglfw3-dev libvulkan-dev libpulse-dev libpng-dev libjpeg-dev libx11-dev
```

### Installing

#### Linux:
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
$ vkav --install-config
```
This will copy the config files from "/etc/vkav" to "~/.config/vkav".


#### MacOS (>= 10.15):
This assumes you have [brew](https://brew.sh/) installed.

Install the required dependencies by running:
```
$ brew install glfw3 libsoundio libpng jpeg
```
Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) to some permanent location.

In order to compile vkav run:
```
$ git clone https://github.com/Dougal-s/Vkav.git
$ cd Vkav
$ mkdir build && cd build
$ cmake .. -DVULKAN_SDK_PATH="absolute path to vulkan sdk" -DINCLUDE_X11=OFF
$ make
```

Once that is finished, head over to the [MacOS](./MacOS/README.md) directory for instructions on building a .app bundle.

#### Windows 11 (older may work):

Requires Visual Studio 2019+ and KitWare cmake.

```
# add -DPA_USE_ASIO to enable ASIO support (beware Steinberg license agreement!)
cmake -S . -B build -DBUILD_SHARED_LIBS=OFF
start build/Vkav.sln
```
Select the vkav project, activate Debug|x64 config, press F5 and it should come up.

### Usage
To run Vkav simply execute:
```
$ vkav
```

Config files can be located in "\~/.config/vkav" on Linux and "\~/Library/Preferences/vkav" on MacOS
once the user has executed `Vkav --install-config`.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

## Authors
* Dougal Stewart - [Dougal-s](https://github.com/Dougal-s)

## Acknowledgments
This project was inspired by [GLava](https://github.com/wacossusca34/glava).
