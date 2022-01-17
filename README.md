# CArkanoid
This is my clone of game Arkanoid written in C using SDL2 library and Vulkan API.\
This game was tested on Windows 10 21H1 and Ubuntu 20.04 LTS 'Focal Fossa'.\
I programmed this simple game because I wanted to learn how to use Vulkan.

### Overview
Default key bindings
| Action      | Binding               |
| ----------- | --------------------- |
| Right arrow | Move player right     |
| Left arrow  | Move player left      |
| R           | Restart (during game) |
| Escape      | Exit (any time)       |

### Building
This assumes your computer supports Vulkan.\
This project only works on Windows and Linux.\
I don't know if these build instructions have to be that precise but I like to know that everything is as unambiguous as I can make it.

* Linux
    * Install following software:
    ```
    make
    cmake (version at least 3.21)
    gcc-11
    libsdl2-dev
    libsdl2-image-dev
    vulkan-sdk
    shaderc
    ```
    * Run CMake and than make to produce executable
    ```c
    cmake -S CArkanoid -B /*Name of target CMade dir.*/
    cd /*That CMade dir.*/
    make
    ./Game
    ```
* Windows
    * Install following software:
    ```
    Download SDL2 (development libraries)
    https://www.libsdl.org/download-2.0.php
    Unpack it where you want and add its location to PATH environmental variable.

    Download SDL2_image (development libraries)
    https://www.libsdl.org/projects/SDL_image/
    Unpack it where you want and add its location to PATH environmental variable.

    cmake (version at least 3.21)
    Visual Studio 2022 (should also work on version 2019 but this is untested)
    Vulkan-SDK (https://vulkan.lunarg.com/sdk/home#windows)
    Make sure environmental variable 'VK_SDK_PATH' (or 'VULKAN_SDK') is set to the location of the Vulkan SDK.
    ```
    * Run CMake to produce Visual Studio Solution
    ```c
    cmake -S CArkanoid -B /*Name of target CMade dir.*/
    ```
    * CMake will produce a Visual Studio Solution that you can run by double clicking on it in Windows Explorer.
### License
This game is licensed under GPLv3.0 license.