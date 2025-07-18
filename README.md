efivar
======

Tools and libraries to manipulate EFI variables
---------------------------------------------

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library.  If not, see [http://www.gnu.org/licenses/].

There is an ABI tracker for this project at [ABI Laboratory].

[http://www.gnu.org/licenses/]: http://www.gnu.org/licenses/
[ABI Laboratory]: https://abi-laboratory.pro/tracker/timeline/efivar/

WARNING
=======
You should probably not run "make a brick" *ever*, unless you're already
reasonably sure it won't permanently corrupt your firmware.  This is not a
joke.

Building from source
====================

You can build efivar’s source code by following these instructions:

1. Make sure that you have the following installed:

    * [GCC](https://gcc.gnu.org)
    * [GNU Bash](https://www.gnu.org/software/bash)
    * [GNU Grep](https://www.gnu.org/software/grep)
    * [GNU Make](https://www.gnu.org/software/make)
    * [GNU sed](https://www.gnu.org/software/sed)
    * [Gawk](https://www.gnu.org/software/gawk)
    * [The GNU Core Utilities](https://www.gnu.org/software/coreutils)
    * [mandoc](https://mandoc.bsd.lv)

1. Make sure that you have a copy of efivar’s source code on your computer.
   You can download the latest stable release of efivar’s source code
   [here](https://github.com/rhboot/efivar/releases/latest).

1. Open a terminal.

1. Change directory into efivar’s source code by running this command:

    ```bash
    cd <path to efivar source code>
    ```

1. Build efivar by running this command:

    ```bash
    make
    ```

You can then run the efivar binary that you just built by running this command:

```bash
LD_PRELOAD="$PWD/src/libefivar.so" ./src/efivar --help
```
