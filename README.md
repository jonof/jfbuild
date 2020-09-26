Build Engine Port
=================
by Jonathon Fowler, Ken Silverman, and others

 * First Release: 9 March 2003
 * Email: jf@jonof.id.au
 * Website: http://www.jonof.id.au
 * Source code: https://github.com/jonof/jfbuild
 * Ken's site: http://www.advsys.net/ken

This is the source code for my port of [Ken Silverman's Build game
engine](http://www.advsys.net/ken/buildsrc) to make the engine functional on modern
hardware and operating systems.

Minimum system requirements
---------------------------

* 32 or 64-bit CPU. These have been tried first-hand:
  * Intel x86, x86_64
  * PowerPC 32-bit (big-endian)
  * ARM 32-bit hard-float, 64-bit
* A modern operating system:
  * Linux, BSD, possibly other systems supported by [SDL 2.0](http://libsdl.org/).
  * macOS 10.8+
  * Windows Vista, 7, 8/10+
* Optional: 3D acceleration with OpenGL 2.0 or OpenGL ES 2.0 capable hardware.

Compilation
-----------

Before you begin, clone this repository or unpack the source archive.

Now, based on your chosen OS and compiler:

### Linux and BSD

1. Install the compiler toolchain and SDL2 development packages, e.g.
   * Debian 9: `sudo apt-get install build-essential libsdl2-dev`
   * FreeBSD 11: `sudo pkg install gmake sdl2 pkgconf`
2. Install GTK+ 3 development packages if you want launch windows and editor file choosers, e.g.
   * Debian 9: `sudo apt-get install libgtk-3-dev`
   * FreeBSD 11: `sudo pkg install gtk3`
3. Open a terminal, change into the source code directory, and compile the engine and
   test game with: `make` or `gmake` (BSD)
4. Assuming that was successful, run the test game with: `./kenbuild-data/game`

### macOS

1. [Install Xcode from the Mac App Store](https://itunes.apple.com/au/app/xcode/id497799835?mt=12).
2. Fetch and install the SDL 2.0 development package:
   1. Fetch _SDL2-2.0.x.dmg_ from http://libsdl.org/download-2.0.php.
   2. Copy _SDL2.framework_ found in the DMG file to `~/Library/Frameworks`. Create the
      _Frameworks_ directory if it doesn't exist on your system.
3. Open _game.xcodeproj_ from within the JFBuild source code's _xcode_ folder.
4. From the Product menu choose Run.

### Windows using Microsoft Visual C++ 2015 (or newer) and NMAKE

1. If needed, [install Visual Studio Community 2017 for free from
   Microsoft](https://docs.microsoft.com/en-us/visualstudio/install/install-visual-studio).
   Terms and conditions apply. Install at minimum these components:
   * VC++ 2015.3 v140 toolset for desktop (x86,x64)
   * Windows Universal CRT SDK
   * Windows 8.1 SDK
2. Open the command-line build prompt. e.g. _VS2015 x64 Native Tools Command Prompt_
   or _VS2015 x86 Native Tools Command Prompt_.
3. Change into the JFBuild source code folder, then compile the engine and test game
   with: `nmake /f Makefile.msvc`
5. Assuming success, run the test game with: `kenbuild-data\game`

Compilation options
-------------------

Some engine features may be enabled or disabled at compile time. These can be passed
to the MAKE tool, or written to a Makefile.user (Makefile.msvcuser for MSVC) file in
the source directory.

These options are available:

 * `RELEASE=1` – build with optimisations for release.
 * `RELEASE=0` – build for debugging.
 * `USE_POLYMOST=1` – enable the true 3D renderer.
 * `USE_POLYMOST=0` – disable the true 3D renderer.
 * `USE_OPENGL=1` – enable use of OpenGL 2.0 acceleration.
 * `USE_OPENGL=USE_GL2` – enable use of OpenGL 2.0 acceleration. (GCC/clang syntax.)
 * `USE_OPENGL=USE_GLES2` – enable use of OpenGL ES 2.0 acceleration. (GCC/clang syntax.)
 * `USE_OPENGL=0` – disable use of OpenGL acceleration.
 * `WITHOUT_GTK=1` – disable use of GTK+ to provide launch windows and load/save file choosers.

Test game configuration
-----------------------

Settings for the KenBuild test game and its editor can be found in these locations
depending on your operating system:

 * Windows 7, 8/10: `C:\Users\xxx\AppData\Local\KenBuild`
 * macOS: `/Users/xxx/Library/Application Support/KenBuild`
 * Linux: `~/.kenbuild`

Credits and Thanks
------------------
* [Ken Silverman](http://www.advsys.net/ken) for his patience, help, and guidance.
* [Ryan Gordon](http://icculus.org) for inspiring me to try and match his port.
* Pär Karlsson for his contributions.


Enjoy!

Jonathon Fowler


