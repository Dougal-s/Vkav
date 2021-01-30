# Building on MacOS

Once you have installed the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) and compiled vkav, run the command
```
$ ./build.sh VULKAN_SDK_PATH
```
where `VULKAN_SDK_PATH` is the absolute path to the vulkan sdk. The script will build a .app bundle which can
then be installed to the applications directory.
*Note: the script does not package application dependencies*

You may want to create a symlink from `/usr/local/bin` to `Vkav.app/Contents/Macos/start.sh` to allow vkav to be run from the command line.
