Embedthis GoAhead(TM)

Welcome to GoAhead -- the world's most popular embedded web server.

This document contains details regarding the installation procedure for the binary Embedthis GoAhead package.  This package contains pre-built stand-alone web server and an embeddable HTTP library with headers. You can read the documentation for the latest release online at:

https://embedthis.com/goahead/doc/

This software is copyrighted and distributed under license. Please read the LICENSE.TXT for details.


Table of Contents

* System Requirements
* Installation Package Formats
* Development Environment Platform Support
* Apple MAC OS X  Release Details
* Windows Release Details
* Removing GoAhead
* Running GoAhead
* Building from the Source Code
* License and Copyright Information


System Requirements

Operating System Support

    * Windows Windows-7,8 (x86,x64)
    * Mac OS X 10.6+ (x64)
    * Linux 2.6 (x86,x64,arm,ppc,mips)
    * VxWorks (x86,arm,ppc,mips)

To install GoAhead, your development system will need at least the following:

    * 5 MB Disk
    * 512 GB RAM

To deploy GoAhead in an embedded environment, you will need at least the following:

    * 1 MB Disk / ROM space
    * 1 MB RAM

To rebuild GoAhead from source code on your development system, you will need at least:

    * 100 MB disk space

Development Environment Platform Support

If you wish to embed GoAhead in your application or build GoAhead from source code, you will need to use a C compiler and associated development tools.  Several development environments are supported.  You may choose any of the following:

    * Linux/Unix GNU Tools

    * Windows Visual Studio 2013 or later

    * WindRiver Development Tools

To build GoAhead, please download the source code distribution from the https://embedthis.com/goahead/ and read "Building from Source" in the GoAhead product documentation.

Running GoAhead

    goahead --home /etc/goahead -v

To get all the options for GoAhead, read the GoAhead man page via

    man goahead

or get the command help via:

    goahead --help

GoAhead is configured by default to listen on port 80. To test that it is serving pages, point your browser at http://localhost/. 


License and Copyright Information

This software is supplied under license. Both a GNU and commercial license are available. Please read LICENSE.TXT for details. This documentation and the software described in this document may be used and copied only in accordance with the terms of the accompanying license agreement. Product and technical information in this document is subject to change without notice and does not represent a commitment on the part of Embedthis Software LLC.

Copyright (c) Embedthis Software. All Rights Reserved.

Embedthis is a trademark of Embedthis Software LLC. Embedthis, GoAhead and Ejscript are trademarks of Embedthis Software. All other brand or product names are the trademarks or registered trademarks of their respective holders.


Embedthis Software, LLC.
4616 25th Ave NE, #733
Seattle, WA 98105.
+1 (408) 905-7017
embedthis.com
info@embedthis.com
