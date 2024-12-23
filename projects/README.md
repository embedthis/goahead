Embedthis GoAhead Projects
===

Please read the top level [README.md](../README.md) first.

The projects directory contains pre-built projects for various IDE environments and Make / NMake.

The projects are named according to the pattern:

    goahead-OS-PROFILE.TYPE

Type will be ".mk" for Makefiles, ".nmake" for NMakefiles, ".xcodeproj" for Xcode and ".sln" for Visual Studio solutions.

The generated operating systems projects include: freebsd, linux, macosx, vxworks and windows.

Two profiles are generated for all operating systems:

* static - for a statically linked build
* default - for a dynamically linked build

Windows and Xcode include projects for OpenSSL 1.0 and 1.X SSL stacks. See
<a href="openssl">building with SSL</a> below for details about using the OpenSSL projects.

<a name="openssl"></a>

## Building Projects with Make

The default Makefile will invoke the projects/goahead-OS-default.mk project and pass all arguments to that project. In this manner, you can invoke the default project via:

    make

Alternatively you can run a specific Makefile project via:

    make -f projects/goahead-NAME.mk

## Building with OpenSSL

To build with OpenSSL on Windows or MacOS you must first download OpenSSL (https://www.openssl.org/source/) and build it from source. We support the latest stable and LTS releases only. Once OpenSSL is built, you will need to supply the GoAhead project with the pathname to your OpenSSL source code directory.

Unfortunately on Windows, OpenSSL 1.0 and 1.1 have changed library names and directories. To handle this, we provide dedicated projects for 1.0 and 1.1 on windows.

### Building with Make and OpenSSL

To set the path to your OpenSSL source directory, run make and set the required path:

    make ME_COM_OPENSSL=1 ME_COM_OPENSSL_PATH=/path/to/openssl SHOW=1

### Building with Visual Studio and NMake

With windows, the supplied ./make.bat will invoke NMake and the projects/goahead-windows-default.nmake project. Set the OpenSSL path via the environment before invoking make.

    set ME_COM_OPENSSL=1
    set ME_COM_OPENSSL_PATH=/path/to/openssl
    set SHOW=1
    make

### Building with Visual Studio and OpenSSL

To set the path to your OpenSSL source directory, select "View -> Property Manager". Select "goahead" and edit the "product" property page by expanding Debug. In the "product Property Pages" dialog, add a User Macro definition for the OpenSSL source directory>

    ME_COM_OPENSSL_PATH     path/to/openssl

Then build the solution. Set "goahead" as the default startup project and the "./test" directory as the working directory. Use "-v" as the program arguments.

### Building with Xcode and OpenSSL

To set the path to your OpenSSL source directory, click on "goahead-macosx-default" on the left to display the project settings. Select under "PROJECT" the "goahead-macosx-openssl_10" project and click on "Build Settings" in the panel in the center. Scroll down to define the path to your OpenSSL source code dirctory in the ME_COM_OPENSSL_PATH property.

[Working with Project Properties | Microsoft Docs](https://docs.microsoft.com/en-us/previous-versions/669zx6zc(v=vs.140)#user-defined-macros)

Resources
---
  - [GoAhead web site](https://www.embedthis.com/)
  - [Embedthis web site](https://www.embedthis.com/)
