Embedthis GoAhead
===

The most popular little embedded web server.

## Upgrading

If you are creating a new device or planning your upgrade path for the future,
we recommend you consider the [Ioto Device
Agent](https://www.embedthis.com/ioto/). It incorporates everything we've
learned from GoAhead over 20 years of developing device management software.
Talk to us about how to upgrade to Ioto at
[Support](mailto:support@embedthis.com).

## Licensing

See [LICENSE.md](LICENSE.md) and
https://www.embedthis.com/goahead/licensing.html for details.

## Documentation

  See https://www.embedthis.com/goahead/doc/index.html.

## Building from Source

You can build GoAhead with make, Visual Studio or Xcode.

The IDE projects and Makefiles will build with SSL using the
[MbedTLS](https://github.com/ARMmbed/mbedtls) TLS stack. To build with CGI,
OpenSSL or other modules, read the [projects/README.md](projects/README.md) for
details.

## To Build with Make

### Linux or MacOS

    make

or to see the commands as they are invoked:

    make SHOW=1

You can pass make variables to tailor the build. For a list of variables:

	make help

To run

	make run

### Windows

    make

The make.bat script runs projects/windows.bat to locate the Visual Studio
compiler. If you have setup
your CMD environment for Visual Studio by running the Visual Studio
vsvarsall.bat, then that edition of
Visual Studio will be used. If not, windows.bat will attempt to locate the most
recent Visual Studio version.

## To Build with Visual Studio

To build with Visual Studio, you will need to install the
[vcpkg](https://vcpkg.io/en/) dependency manager and install openssl.

    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    .\vcpkg integrate install
    .\vcpkg install openssl

Then open the Visual Studio solution file at:

    projects/goahead-windows-default.sln

Then select Build -> Solution.

To run the debugger, right-click on the "goahead" project and set it as the
startup project. Then modify the project properties and set the Debugging
configuration properties. Set the working directory to be:

    $(ProjectDir)\..\..\test

Set the arguments to be
    -v

Then start debugging.

You may need to install the Windows Power Shell if not already installed on
your system.

    winget install --id Microsoft.PowerShell --source winget


## To Build with Xcode.

Open the solution file:

    projects/goahead-macosx-default.sln

Choose Product -> Scheme -> Edit Scheme, and select "Build" on the left of the
dialog. Click the "+" symbol at the bottom in the center and then select all
targets to be built. Before leaving this dialog, set the debugger options by
selecting "Run/Debug" on the left hand side. Under "Info" set the Executable to
be "goahead", set the launch arguments to be "-v" and set the working directory
to be an absolute path to the "./test" directory in the goahead source. The
click "Close" to save.

Click Project -> Build to build.

Click Project -> Run to run.

## To run

    make run

## To install

If you have built from source using Make, you can install the software using:

    sudo make install

## To uninstall

    sudo make uninstall

Resources
---
  - [GoAhead web site](https://www.embedthis.com/goahead/)
  - [Embedthis web site](https://www.embedthis.com/)

