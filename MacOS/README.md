# Building on MacOS

Once you have installed the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) and compiled vkav, run the command
```
$ ./build.sh VULKAN_SDK_PATH
```
where `VULKAN_SDK_PATH` is the absolute path to the vulkan sdk, in order to create the .app bundle.
Once that is done, vkav can be installed by moving the .app bundle into the applications directory.

You may want to create a symlink from `/usr/local/bin` to `Vkav.app/Contents/Macos/start.sh` so that
you can run vkav from the command line and pass command line arguments.
