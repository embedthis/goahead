Embedthis GoAhead
===

The most popular little embedded web server.

GoAhead is a small, fast, and secure web server that is easy to embed in your products. It is used in millions of devices worldwide.

## Status

GoAhead is actively supported and receives security updates as required, but will not have new features added. 

If you are creating a new device or planning your upgrade path for the future, we recommend you consider the [Ioto Device Agent](https://www.embedthis.com/ioto/). It incorporates everything we've learned from GoAhead over 20 years of developing device management software. Talk to us about how to upgrade to Ioto at [Support](mailto:support@embedthis.com).

## Security for Devices

The European Union has introduced the Cyber Resilience Act (CRA), a regulation aimed at enhancing cybersecurity for IoT products. This legislation mandates that manufacturers ensure their products are secure throughout their entire lifecycle, from design to decommissioning. This requires that software updates are provided for the lifetime of the device.

To meet this need, the standalone [Embedthis Updater](https://github.com/embedthis/updater) library and command line utility can be used to automatically download and apply software updates to all your GoAhead-enabled devices. The [Embedthis Builder](https://www.embedthis.com/builder/) can be used to publish, distribute, manage and track software updates for your GoAhead devices.

## EU Cyber Resilience Act

The open source release of GoAhead, published in the public GitHub repository, does not carry CRA conformity documentation. A manufacturer that integrates GoAhead into a product placed on the EU market is responsible for that product's conformity with the Cyber Resilience Act (Regulation (EU) 2024/2847).

CRA conformity is offered with the commercial GoAhead product. Its source distribution includes the EU Declaration of Conformity, the Annex II user information and a software bill of materials, backed by Embedthis vulnerability handling over a stated support period. Contact [sales@embedthis.com](mailto:sales@embedthis.com).

## Licensing

See [LICENSE.md](LICENSE.md) and https://www.embedthis.com/goahead/licensing.html for details.

## Documentation

  See https://www.embedthis.com/goahead/doc/index.html.

  Before deploying GoAhead in a product, read [SECURITY-DEPLOYMENT.md](SECURITY-DEPLOYMENT.md).

## Building from Source

You can build GoAhead with make, Visual Studio or Xcode.

The supplied project files and Makefiles build with SSL using OpenSSL. GoAhead also supports the
[MbedTLS](https://github.com/ARMmbed/mbedtls) TLS stack via `src/mbedtls.c`, but MbedTLS is not
included in this distribution; to use it, supply your own MbedTLS and regenerate the project files
from the `projects` directory with `premake5 --tls=mbedtls gmake2` (or `vs2022`, `xcode4`).

## To Build with Make

### Linux or MacOS

    make

or to see the commands as they are invoked:

    make SHOW=1

You can pass make variables to tailor the build. For a list of variables:

	make help

See [To run](#to-run) below for running the server once it is built.

### Windows

    make

The make.bat script runs projects/windows.bat to locate the Visual Studio compiler. If you have setup
your CMD environment for Visual Studio by running the Visual Studio vsvarsall.bat, then that edition of
Visual Studio will be used. If not, windows.bat will attempt to locate the most recent Visual Studio version.

## To Build with Visual Studio

To build with Visual Studio, you will need to install the [vcpkg](https://vcpkg.io/en/) dependency manager and install openssl.

    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    .\vcpkg integrate install
    .\vcpkg install openssl

Then open the Visual Studio solution file at:

    projects/vs2022/goahead.sln

Then select Build -> Solution.

GoAhead links the dynamic Microsoft C runtime, so the library, the programs and any application that embeds them share one heap and descriptor table. Install the Microsoft Visual C++ Redistributable on each target, or copy the runtime DLLs beside the binaries. Link OpenSSL libraries built for the same runtime: `MD` for release, `MDd` for debug.

To run the debugger, right-click on the "goahead" project and set it as the startup project. Then modify the project properties and set the Debugging configuration properties. Set the working directory to be:

    $(ProjectDir)\..\..\test

Set the arguments to be
    -v

Then start debugging.

You may need to install the Windows Power Shell if not already installed on your system.

    winget install --id Microsoft.PowerShell --source winget


## To Build with Xcode.

Open the solution file:

    projects/xcode/goahead.xcworkspace

Choose Product -> Scheme -> Edit Scheme, and select "Build" on the left of the dialog. Click the "+" symbol at the bottom in the center and then select all targets to be built. Before leaving this dialog, set the debugger options by selecting "Run/Debug" on the left hand side. Under "Info" set the Executable to be "goahead", set the launch arguments to be "-v --no-strict" and set the working directory to be an absolute path to the "./test" directory in the goahead source. The click "Close" to save.

Click Product -> Build to build.

Click Product -> Run to run.

## To run

Run the server from the `test` directory, which supplies the sample documents, route configuration
and authentication file:

    cd test
    ../build/bin/goahead -v --no-strict web 127.0.0.1:8080

Then browse to http://127.0.0.1:8080/.

The test route table protects directories that its public catch-all route can also serve, so the test suite can exercise the server's defences. GoAhead refuses to start such a table unless given `--no-strict`, which turns the refusal into warnings. Do not use `--no-strict` in a product.

The `test` directory is for development only. Its `auth.txt` holds test accounts whose passwords are published, including an administrator. Never deploy it: start a product from `src/auth.txt` and `src/route.txt`, create accounts with `gopass`, and read `SECURITY-DEPLOYMENT.md`.

The command line is `goahead [options] [documents] [[IPaddress][:port] ...]`. The first non-option
argument is the documents directory, so it must be given whenever a listen endpoint is. With no
arguments GoAhead serves `web` on the build's default endpoints, ports 80 and 443, which usually requires privilege. The https endpoint needs a certificate and key, given with `--cert` and `--key` or configured at build time; without them GoAhead refuses to listen on it and exits. Use `--help`
for the full option list.

The build products are left under `build/bin`. GoAhead does not install itself into the system;
embed the library and copy the binaries into your product image as required.

## Testing

The test suite is located in the `test/` directory and uses the [TestMe](https://www.embedthis.com/testme/) framework. 

The test suite requires the following prerequisites:

- **Bun**: v1.2.23 or later
- **TestMe**: Test runner, provided by the `@embedthis/testme` package
- **Ejscript**: HTTP client library used by the tests, provided by the `@embedthis/ejscript` package

Install Bun by following the instructions at: 

    https://bun.com/docs/installation

Then install the test dependencies from the top level of the source tree:

    npm install

This installs [TestMe](https://www.npmjs.com/package/@embedthis/testme) and
[Ejscript](https://www.npmjs.com/package/@embedthis/ejscript). The TestMe post-install script builds
the `tm` command into `~/.bun/bin`, so ensure that directory is on your PATH.

If you prefer Bun, use `bun install` instead. `package.json` lists TestMe in `trustedDependencies`,
so Bun runs the TestMe post-install script.

Run the tests with:

    make test

or manually via the `tm` command. 

    tm

To run a specific test or group of tests, use the `tm` command with the test name.

    tm basic/

## Resources
---
  - [GoAhead web site](https://www.embedthis.com/goahead/)
  - [Embedthis web site](https://www.embedthis.com/)

