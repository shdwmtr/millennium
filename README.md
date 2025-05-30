<div align="center">
<!-- <img src="https://i.imgur.com/9qYPFSA.png" alt="Alt text" width="40">
  ## Millennium for Steam® -->

<h3><img align="center" height="40" src="https://i.imgur.com/9qYPFSA.png"> &nbsp; &nbsp;Millennium for Steam®</h3>

<kbd>
  <a href="https://steambrew.app/discord">
      <img alt="Static Badge" src="https://img.shields.io/badge/discord-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=discord&logoColor=white" href="#">
  </a>
</kbd>
<kbd>
  <a href="https://steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/website-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=firefoxbrowser&logoColor=white" href="#">
  </a>
</kbd>
<kbd>
  <a href="https://docs.steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/documentation-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=readthedocs&logoColor=white" href="#">
  </a>
</kbd>
<kbd>
  <a href="#"  title="Lines of Code">
      <img alt="Static Badge" src="https://img.shields.io/endpoint?url=https%3A%2F%2Floc-counter.onrender.com%2F%3Frepo%3Dshdwmtr%2Fmillennium%26branch%3Dmain%26languages%3DC%252B%252B%2CC%2520Header%26ignored%3Dvendor&style=for-the-badge&labelColor=%23151B23&color=%23151B23&logo=coderwall&label=%20Lines of Code&logoColor=white">
  </a>
</kbd>

<br>
<br>

Millennium is an **open-source low-code modding framework** to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead.

If you enjoy this tool, please consider starring the project ⭐

<br>

<!-- credits to https://github.com/clawdius for this intro video -->

https://github.com/user-attachments/assets/20dec435-ed47-4042-9fb7-38635480b850

<br>
</div>

## Installation

### Automatic Installation (Recommended)

Installing Millennium is only a few steps. See [this page](https://docs.steambrew.app/users/installing#automatic) for a more detailed guide.

### Manual Installation

For normal users, installing via the installers makes the most sense. However when wanting to either develop Millennium, or when the installers do not work, this option can be used. Check our [documentation](https://docs.steambrew.app/users/installing#manual) for a guide on how to do this.

&nbsp;

## Core Features

-   ### [Plugin Loader](/src/)
    -   **TypeScript ([React](https://react.dev/)) frontend** container in Steam
    -   **Python backend** container in [usermode](https://en.wikipedia.org/wiki/User-Mode_Driver_Framework)
    -   **[Foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)** binding from **Python** to **JavaScript** and vice versa
    -   **Hook modules in the Steam web browser**
        -   Overwrite/Modify HTTP requests
        -   Load custom JavaScript (Native) into web browser
        -   Load custom StyleSheets into web browser
-   ### [Core Modules](/assets/)
    -   Manage and load custom user themes into Steam
    -   Manage custom plugins for steam
    -   Maintain theme & plugin versions.
    -   Manage [embedded Python](https://www.python.org/downloads/release/python-3118/) installation
        -   Manage [Millennium Python Developer Tools](https://pypi.org/project/millennium/)
        -   Custom package manager for all plugins

&nbsp;

## Adding Spoken Language Support

Take a look [here](./assets#adding-languages), it quickly covers what files you'll need to edit in order to add your spoken language to Millennium!

&nbsp;

## Creating Plugins & Themes

Creating themes and plugins for Millennium is relatively straight foward. Our [documentation](https://docs.steambrew.app/developers) goes over the basics of both,
and we have examples for both in [examples](./examples)

&nbsp;

## Platform Support

Supported Platforms:

-   Windows (x86/x64/ARM) NT (10 and newer)
-   Linux (x86/x86_64/i686/i386)
-   OSX (Support planned, WIP)

&nbsp;

## Unix File Structure

```bash
/
├─ usr/
│  ├─ lib/
│  │  ├─ millennium/
│  │  │  ├─ libMillennium.so # millennium
│  │  │  ├─ libpython-3.11.8.so # dynamically linked to millennium, allows user plugin backends to run
│  ├─ share/
│  │  ├─ assets/ # builtin plugin that provides base functionality for millennium.
│  │  ├─ shims/
├─ home/
│  ├─ $user/
│  │  ├─ .local/share
│  │  │  ├─ millennium/
│  │  │  │  ├─ .venv/
│  │  │  │  ├─ logs/
│  │  │  │  ├─ plugins/ # user plugins
│  │  ├─ .config/
│  │  │  ├─ millennium/ # config files
```

## Building from source

### Windows 10/11

Building Millennium will require a long list of steps, however everything should work smoothly given you follow the instructions

The following guide includes the installation of the following:

-   [MSYS2](https://repo.msys2.org/distrib/x86_64/msys2-x86_64-20241208.exe) (MinGW32 specifically)
-   [Visual Studio Build Tools ](https://aka.ms/vs/17/release/vs_BuildTools.exe)

### Build Steps

1.  Download and install [MSYS2](https://repo.msys2.org/distrib/x86_64/msys2-x86_64-20241208.exe)
1.  Download and install [Visual Studio Build Tools](https://aka.ms/vs/17/release/vs_BuildTools.exe)
1.  Run `Developer PowerShell for VS 2022` installed from the previous step.
1.  Navigate to somewhere you want to build to
1.  Next, open Powershell to download and build Python 3.11.8 (Win32).

    - Download & Extract Python 3.11.8

        ```bash
        $ curl -o python3.11.8.tgz https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz
        $ tar -xzvf python3.11.8.tgz
        $ cd python-3.11.8
        ```

    - Update Build Configuration to be MultiThreaded

        ```ps1
        $ (Get-Content "PCbuild/pythoncore.vcxproj" -Raw) -replace '</ClCompile>', '<RuntimeLibrary Condition="`$(Configuration)|`$(Platform)"=="Release|Win32">MultiThreaded</RuntimeLibrary><RuntimeLibrary Condition="`$(Configuration)|`$(Platform)"=="Debug|Win32">MultiThreadedDebug</RuntimeLibrary></ClCompile>' | Set-Content "PCbuild/pythoncore.vcxproj"
        ```

    - Bootstrap Python builder

        ```bash
        $ ./PCbuild/get_externals.bat
        ```

    - Build Python

        ```bash
        $ msbuild PCBuild/pcbuild.sln /p:Configuration=Release /p:Platform=Win32 /p:RuntimeLibrary=MT
        $ msbuild PCBuild/pcbuild.sln /p:Configuration=Debug /p:Platform=Win32 /p:RuntimeLibrary=MT
        ```

    - Check Python version

        ```bash
        $ ./PCbuild/win32/python.exe --version
        ```

    - Check the following items have been built, you'll need them later
        ```bash
        # Release binaries, required for building Millennium in release mode
        PCbuild/win32/python311.dll
        PCbuild/win32/python311.lib
        # Debug binaries, required for building Millennium in debug mode
        PCbuild/win32/python311_d.dll
        PCbuild/win32/python311_d.lib
        ```

1.  Now, open MSYS2, any of the shells should work fine.
1.  Run the following and close the shell.
    ```bash
     $ pacman -Syu
     $ pacman -S mingw-w64-i686-cmake
     $ pacman -S --needed base-devel mingw-w64-i686-toolchain
     $ pacman -S git
    ```
1.  Open CMD, and navigate to somewhere you want to build millennium and then run:
    `C:\msys64\msys2_shell.cmd -defterm -no-start -mingw32` this will put you in the MinGW shell
1.  Clone the Millennium repository
    ```cmd
    $ git clone https://github.com/SteamClientHomebrew/Millennium --recursive
    $ cd millennium
    ```
1.  From the previous step, where we built python, copy the files to `%MILLENNIUM_SRC_DIR%/vendor/python`
1.  Build Millennium

    ```bash
    $ cmake --preset="windows-mingw-debug"
    # or for release
    $ cmake --preset="windows-mingw-release"
    $ cmake --build ./build
    ```

1.  Next, you'll need to build Millenniums internal plugin from source.
    You can install NodeJs from MinGW, or you could use your local install from PowerShell/CMD

        ```bash
        $ cd "%MILLENNIUM_SRC_DIR%/assets"
        $ npm install
        $ npm run dev
        ```

        Millennium expects these shim assets to be at `%MILLENNIUM_DIR%/ext/data/assets`

        where `%MILLENNIUM_DIR%` is:
        - Steam path (default `C:\Program Files (x86)\Steam`) on Windows
        - `~/.millennium` on Unix

        You can either symlink them or copy them over.
