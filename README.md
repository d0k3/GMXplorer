# GodMode9 but with lua

Fork of [d0k3/GodMode9](https://github.com/d0k3/GodMode9) with super experimental Lua support.

* Makefile.build edited to support a LIBS variable on build

* Math lib (-lm) now needed for lua

* link.ld on arm9 edited to include .ARM.exidx into .rodata, caused by lua's code. Limit of AHBWRAM max size was increased until address 0x80C0000, 744KiB since 0x8006000

Copy init.lua to the SD card and select it. There is now an "Execute Lua script" option.

Or put your script at `0:/gm9/luascripts/*.lua`. Then press HOME or POWER and choose "Lua scripts...".

The main lua stuff is at `arm9/source/lua`. Custom files are prefixed with `gm9`.

The API here is not at all stable. But there are currently two libraries to play with. This is not set in stone!

Make sure to read this for the API proposal: https://github.com/ihaveamac/GM9-lua-attempt/issues/3

Look at the other branches for older development tests. lua-attempt is where most of the stuff happened.

## Global

* print(...)
  * Calling this will replace the alt screen with an output buffer. It doesn't support newlines or word wrapping properly yet.
