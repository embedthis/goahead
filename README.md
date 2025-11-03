Embedthis GoAhead
===

The most popular little embedded web server.

GoAhead is a small, fast, and secure web server that is easy to embed in your
products. It is used in millions of devices worldwide and is the foundation of
the [Ioto Device Agent](https://www.embedthis.com/ioto/).

## Status

GoAhead is in a maintenance phase. It is actively supported and will receive
security updates as required, but it will not have new features added. 

If you are creating a new device or planning your upgrade path for the future,
we recommend you consider the [Ioto Device
Agent](https://www.embedthis.com/ioto/). It incorporates everything we've
learned from GoAhead over 20 years of developing device management software.
Talk to us about how to upgrade to Ioto at
[Support](mailto:support@embedthis.com).

## Security for Devices

The European Union has introduced the Cyber Resilience Act (CRA), a regulation
aimed at enhancing cybersecurity for IoT products. This legislation mandates
that manufacturers ensure their products are secure throughout their entire
lifecycle, from design to decommissioning. This requires that software updates
are provided for the lifetime of the device.

To meet this need, GoAhead now includes the [Embedthis Updater](src/updater/)
library and command line utility that can be used to automatically download and
apply software updates to all your GoAhead-enabled devices. The [EmbedThis
Builder](https://www.embedthis.com/builder/) can be used to publish,
distribute, manage and track software updates for your GoAhead devices.

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

## Testing

The test suite is located in the `test/` directory and uses the
[TestMe](https://www.embedthis.com/testme/) framework. 

The test suite requires the following prerequisites:

- **Bun**: v1.2.23 or later
- **TestMe**: Test runner (installed globally)

Install Bun by following the instructions at: 

    https://bun.com/docs/installation

Install TestMe globally with:

    bun install -g --trust @embedthis/testme

Run the tests with:

    make test

or manually via the `tm` command. 

    tm

To run a specific test or group of tests, use the `tm` command with the test
name.

    tm basic/

## Resources
---
  - [GoAhead web site](https://www.embedthis.com/goahead/)
  - [Embedthis web site](https://www.embedthis.com/)

