# GodMode9 Lua documentation

> [!IMPORTANT]
> WORK IN PROGRESS! A list of functions is available at the [Lua support pull request](https://github.com/d0k3/GodMode9/pulls).

GodMode9 includes a Lua 5.4.7 implementation.

## Running scripts

There are four ways to run Lua scripts:

* Select it in the file browser and choose "`Execute Lua script`"
* Place it in `0:/gm9/luascripts`, then open the HOME menu, choose "`Lua scripts...`", then choose the script
* Place it in `data/luascripts`, then build GodMode9 from source
	* This takes precedence over `0:/gm9/luascripts`
* Place it at `data/autorun.lua`, then build GodMode9 from source with `SCRIPT_RUNNER=1` to automatically run it at launch

## Packages

Lua scripts can load custom modules. The default search path (defined as `package.path`) is:

```
0:/gm9/luapackages/?.lua;0:/gm9/luapackages/?/init.lua;V:/luapackages/?.lua;V:/luapackages/?/init.lua
```

For example, when a script calls `require("foobar")`, the package searcher will look for the module in this order:

* `0:/gm9/luapackages/foobar.lua`
* `0:/gm9/luapackages/foobar/init.lua`
* `V:/luapackages/foobar.lua`
* `V:/luapackages/foobar/init.lua`

## Comparison with GM9Script

These tables are provided to assist with converting from GM9Script to Lua.

### Commands

GM9 | Lua | Notes
-- | -- | --
goto | http://lua-users.org/wiki/GotoStatement |  
labelsel | ui.ask_selection |  
for | fs.find_all | Recursive searching does not work exactly the same, it will search for the filename in all directories
keychk | ui.check_key |  
echo | ui.echo |  
qr | ui.show_qr |  
ask | ui.ask |  
input | ui.ask_input |  
filesel | fs.ask_select_file |  
dirsel | fs.ask_select_dir |  
set | local var = value | More details on variables and scoping: https://www.lua.org/pil/4.2.html
strsplit | string.find and string.sub |  
strrep | string.gsub | https://www.lua.org/manual/5.4/manual.html#pdf-string.gsub
allow | fs.allow |  
cp | fs.copy |  
mv | fs.move |  
inject | fs.write_file |  
fill | fs.fill_file |  
fdummy | fs.make_dummy_file |  
rm | fs.remove |  
mkdir | fs.mkdir |  
mount | fs.img_mount |  
umount | fs.img_umount |  
find | fs.find |  
findnot | fs.find_not |  
fget | fs.write_file |  
fset | fs.write_file |  
sha | fs.hash_file OR fs.verify_with_sha_file | hash_file simply returns a hash, verify_with_sha_file compares it with a corresponding .sha file
shaget | fs.hash_file |  
dumptxt | fs.write_file | Use "end" for offset to append data
fixcmac | fs.fix_cmacs |  
verify | fs.verify |  
decrypt | title.decrypt |  
encrypt | title.encrypt |  
buildcia | title.build_cia |  
install | title.install |  
extrcode | title.extract_code |  
cmprcode | title.compress_code |  
sdump | fs.key_dump |  
applyips | target.apply_ips |  
applybps | target.apply_bps |  
applybpm | target.apply_bpm |  
textview | fs.show_file_text_viewer | fs.show_text_viewer can be used to show text from a variable
cartdump | fs.cart_dump |  
isdir | fs.is_dir |  
exist | fs.exists |  
boot | sys.boot |  
switchsd | fs.sd_switch |  
nextemu | sys.next_emu |  
reboot | sys.reboot |  
poweroff | sys.power_off |  
bkpt | bkpt |  

### PREVIEW_MODE variable

Unlike the `PREVIEW_MODE` GM9Script variable, this has been split into multiple functions.

Setting | Lua
-- | --
“quick" and “full" | (There is no alternative to view a Lua script as it’s running.)
“off" | ui.clear
text | ui.show_text
png file | ui.show_png
game icon | ui.show_game_info

### Other constants

GM9 | Lua | Notes
-- | -- | --
DATESTAMP | util.get_datestamp() | Formatted like “241202", equivalent to os.date("%y%m%d")
TIMESTAMP | util.get_timestamp() | Formatted like “010828", equivalent to os.date("%H%M%S")
SYSID0 | sys.sys_id0 |  
EMUID0 | sys.emu_id0 |  
EMUBASE | sys.emu_base |  
SERIAL | sys.serial |  
REGION | sys.region |  
SDSIZE | fs.stat_fs("0:/").total | int instead of string (use util.format_bytes to format it)
SDFREE | fs.stat_fs("0:/").free | int instead of string (use util.format_bytes to format it)
NANDSIZE | NANDSIZE | int instead of string (use util.format_bytes to format it)
GM9OUT | GM9OUT |  
CURRDIR | CURRDIR | nil instead of “(null)" if it can’t be found
ONTYPE | CONSOLE_TYPE | “O3DS" or “N3DS"
RDTYPE | IS_DEVKIT | boolean instead of a string
HAX | HAX |  
GM9VER | GM9VER |  

## Comparisons with standard Lua

These original Lua 5.4 modules are fully available:
* [Basic functions](https://www.lua.org/manual/5.4/manual.html#6.1)
	* `print` will replace the top screen with an output log. Currently it does not implement some features such as line wrapping.
	* dofile and loadfile don't work yet.
* [coroutine](https://www.lua.org/manual/5.4/manual.html#6.2)
* [debug](https://www.lua.org/manual/5.4/manual.html#6.10)
* [math](https://www.lua.org/manual/5.4/manual.html#6.7)
* [string](https://www.lua.org/manual/5.4/manual.html#6.4)
* [table](https://www.lua.org/manual/5.4/manual.html#6.6)
* [utf8](https://www.lua.org/manual/5.4/manual.html#6.5)

These modules are partially available:
* [os](https://www.lua.org/manual/5.4/manual.html#6.9)
	* Only `os.clock`, `os.time`, `os.date`, `os.difftime`, and `os.remove`
* [io](https://www.lua.org/manual/5.4/manual.html#6.8)
	* Only `io.open`; for open files, all but `file:setvbuf` and `file:lines`
	* This is a custom compatibility module that uses `fs` functions. If there are differences compared to the original `io` implementation, please report them as issues.

These modules work differently:
* [package](https://www.lua.org/manual/5.4/manual.html#6.3)
	* `package.cpath` and `package.loadlib` are nonfunctional due to GM9 having no ability to load dynamic libraries.

## Third-party libraries

rxi's [json.lua](https://github.com/rxi/json.lua) is pre-included. To use it, first load it:

```lua
local json = require("json")
```

---

## API reference

### Constants

#### GM9VER
The version such as `"v2.1.1-159-gff2cb913"`, the same string that is shown on the main screen.

#### SCRIPT
Path to the executed script, such as `"0:/gm9/luascripts/myscript.lua"`.

#### CURRDIR
Directory of the executed script, such as `"0:/gm9/luascripts"`.

#### GM9OUT
The value `"0:/gm9/out"`.

#### HAX
> [!WARNING]
> This needs checking if it's accurate.
One of three values:
* "ntrboot" if started from an ntrboot cart
* "sighax" if booted directly from a FIRM partition
* Empty string otherwise

#### NANDSIZE
Total size of SysNAND in bytes.

#### CONSOLE_TYPE
The string `"O3DS"` or `"N3DS"`.

#### IS_DEVKIT
`true` if the console is a developer unit.

---

### `ui` module

> [!NOTE]
> This assumes the default build is used, where the bottom screen is the main screen. If GodMode9 is compiled with `SWITCH_SCREENS=1`, then every instance where something appears on the bottom screen will actually be on the top screen and vice versa.

#### ui.echo

* `void ui.echo(string text)`

Display text on the bottom screen and wait for the user to press A.

* **Arguments**
	* `text` - Text to show the user

#### ui.ask

* `bool ui.ask(string text)`

Prompt the user with a yes/no question.

* **Arguments**
	* `text` - Text to ask the user
* **Returns:** `true` if the user accepts

#### ui.ask_hex

* `int ui.ask_hex(string text, int initial, int n_digits)`

Ask the user to input a hex number.

* **Arguments**
	* `text` - Text to ask the user
	* `initial` - Starting value
	* `n_digits` - Amount of hex digits allowed
* **Returns:** the number the user entered, or `nil` if canceled

#### ui.ask_number

* `int ui.ask_number(string text, int initial)`

Ask the user to input a number.

* **Arguments**
	* `text` - Text to ask the user
	* `initial` - Starting value
* **Returns:** the number the user entered, or `nil` if canceled

#### ui.ask_text

* `string ui.ask_text(string prompt, string initial, int max_length)`

Ask the user to input text.

* **Arguments**
	* `prompt` - Text to ask the user
	* `initial` - Starting value
	* `max_length` - Maximum length of the string
* **Returns:** the text the user entered, or `nil` if canceled

#### ui.ask_selection

* `int ui.ask_selection(string prompt, array options)`

Ask the user to choose an option from a list. A maximum of 256 options are allowed.

* **Arguments**
	* `prompt` - Text to ask the user
	* `options` - Table of options
* **Returns:** index of selected option, or `nil` if canceled

#### ui.clear

* `void ui.clear()`

Clears the top screen.

#### ui.show_png

* `void ui.show_png(string path)`

Displays a PNG file on the top screen.

The image must not be larger than 400 pixels horizontal or 240 pixels vertical. If `SWITCH_SCREENS=1` is used, it must not be larger than 320 pixels horizontal.

* **Arguments**
	* `path` - Path to PNG file
* **Throws** 
	* `"Could not read (file)"` - file does not exist or there was another read error
	* `"Invalid PNG file"` - file is not a valid PNG
	* `"PNG too large"` - too large horizontal or vertical, or an out-of-memory error

#### ui.show_text

* `void ui.show_text(string text)`

Displays text on the top screen.

* **Arguments**
	* `text` - Text to show the user

#### ui.show_game_info

* `void ui.show_game_info(string path)`

Shows game file info. Accepts any files that include an SMDH, a DS game file, or GBA file.

* **Arguments**
	* `path` - Path to game file (CIA, CCI/".3ds", SMDH, TMD, TIE (DSiWare export), Ticket, NDS, GBA)
* **Throws**
	* `"ShowGameFileIcon failed on <path>"` - failed to get game info from path

#### ui.show_qr

* `void ui.show_qr(string text, string data)`

Displays a QR code on the top screen, and a prompt on the bottom screen, and waits for the user to press A.

* **Arguments**
	* `text` - Text to show the user
	* `data` - Data to encode into the QR code
* **Throws**
	* `"could not allocate memory"` - out-of-memory error when attempting to generate the QR code

#### ui.show_text_viewer

* `void ui.show_text_viewer(string text)`

Display a scrollable text viewer.

* **Arguments**
	* `text` - Text to display
* **Throws**
	* `"text validation failed"` - given string contains invalid characters
	* `"failed to run MemTextViewer"` - internal memory viewer error

#### ui.show_file_text_viewer

* `void ui.show_file_text_viewer(string path)`

Display a scrollable text viewer from a text file.

* **Arguments**
	* `path` - Path to text file
* **Throws**
	* `"could not allocate memory"` - out-of-memory error when attempting to create the text buffer
	* `"text validation failed"` - text file contains invalid characters
	* `"failed to run MemTextViewer"` - internal memory viewer error

#### ui.format_bytes

* `string ui.format_bytes(int bytes)`

Format a number with `Byte`, `kB`, `MB`, or `GB`.

> [!NOTE]
> This is affected by localization and may return different text if the language is not English.

* **Arguments**
	* `bytes` - Size to format
* **Returns:** formatted string

#### ui.check_key

* `bool ui.check_key(string key)`

Checks if the user is holding down a key.

* **Arguments**
	* `key` - A button string: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"RIGHT"`, `"LEFT"`, `"UP"`, `"DOWN"`, `"R"`, `"L"`, `"X"`, `"Y"`
* **Returns:** `true` if currently held, `false` if not

---

### `fs` module

#### fs.move

* `void fs.move(string src, string dst[, table opts {bool no_cancel, bool silent, bool overwrite, bool skip_all}])`

Moves or renames a file or directory.

* **Arguments**
	* `src` - Item to move
	* `dst` - Destination name
	* `opts` (optional) - Option flags
		* `no_cancel` - Don’t allow user to cancel
		* `silent` - Don’t show progress
		* `overwrite` - Overwrite files
		* `skip_all` - Skip existing files
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"destination already exists on <src> -> <dst> and {overwrite=true} was not used"` - attempted to move an item over an existing one without using `overwrite`
	* `"PathMoveCopy failed on <src> -> <dst>"` - error when moving, or user canceled

#### fs.remove

* `void fs.remove(string path[, table opts {bool recursive}])`

Delete a file or directory.

* **Arguments**
	* `path` - Path to delete
	* `opts` (optional) - Option flags
		* `recursive` - Remove directories recursively
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"requested directory remove without {recursive=true} on <path>"` - attempted to delete a directory without using `recursive`
	* `"PathDelete failed on %s"` - error when deleting

#### fs.copy

* `void fs.copy(string src, string dst[, table opts {bool calc_sha, bool sha1, bool no_cancel, bool silent, bool overwrite, bool skip_all, bool append, bool recursive}])`

Copy a file or directory.

* **Arguments**
	* `src` - Item to copy
	* `dst` - Destination name
	* `opts` (optional) - Option flags
		* `calc_sha` - Write `.sha` files containing a SHA-256 (default) or SHA-1 hash
		* `sha1` - Use SHA-1
		* `no_cancel` - Don’t allow user to cancel
		* `silent` - Don’t show progress
		* `overwrite` - Overwrite files
		* `skip_all` - Skip existing files
		* `append` - Append to the end of existing files instead of overwriting
		* `recursive` - Copy directories recursively
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"requested directory copy without {recursive=true} on <src> -> <dst>"` - attempted to copy a directory without using `recursive`
	* `"destination already exists on <src> -> <dst> and {overwrite=true} was not used"` - attempted to copy an item over an existing one without using `overwrite`
	* `"PathMoveCopy failed on <src> -> <dst>"` - error when copying, or user canceled

#### fs.mkdir

* `void fs.mkdir(string path)`

Create a directory. This creates intermediate directories as required, so `fs.mkdir("a/b/c")` would create `a`, then `b`, then `c`.

* **Arguments**
	* `path` - Directory to create
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"could not mkdir (<path>)"` - error when creating directories

#### fs.stat

* `array fs.stat(string path)`

Get information about a file or directory. The result is a stat table with these keys:

* **Arguments**
	* `path` - Directory to stat
* **Returns:** A stat table with keys:
	* `name` (string)
	* `type` (string) - `"dir"` or `"file"`
	* `size` (number)
	* `read_only` (bool)
* **Throws**
	* `"could not stat <path> (##)"` - error when attempting to stat item, with FatFs error number

#### fs.list_dir

* `array fs.list_dir(string path)`

Get the contents of a directory. The result is a list of stat tables with these keys:
* `name` (string)
* `type` (string) - `"dir"` or `"file"`
* `size` (number)
* `read_only` (bool)

* **Arguments**
	* `path` - Directory to list
* **Returns:** A list of stat tables, each with keys:
	* `name` (string)
	* `type` (string) - `"dir"` or `"file"`
	* `size` (number)
	* `read_only` (bool)
* **Throws**
	* `"could not opendir <path> (##)"` - error when attempting to open directory, with FatFs error number
	* `"could not readdir <path> (##)"` - error when attempting to read directory, with FatFs error number

#### fs.stat_fs

* `array fs.stat_fs(string path)`

Get information about a filesystem.

> [!NOTE]
> This function can take several seconds before it returns an answer.

* **Arguments**
	* `path` - Filesystem to stat
* **Returns:** A stat table with keys:
	* `free` (number)
	* `total` (number)
	* `used` (number)

#### fs.dir_info

* `array fs.dir_info(string path)`

Get information about a directory.

> [!NOTE]
> This function can take several seconds before it returns an answer.

* **Arguments**
	* `path` - Directory to check
* **Returns:** An info table with keys:
	* `size` (number)
	* `dirs` (number)
	* `files` (number)
* **Throws**
	* `"error when running DirInfo"` - error when scanning directory

#### fs.ask_select_file

* `string fs.ask_select_file(string prompt, string path[, bool opts {bool include_dirs, bool explorer}])`

Prompt the user to select a file based on a pattern. Accepts a wildcard pattern like `"0:/gm9/in/*_ctrtransfer_n3ds.bin"`.

* **Arguments**
	* `prompt` - Text to ask the user
	* `path` - Wildcard pattern to search for
	* `opts` (optional) - Option flags
		* `include_dirs` - Include directories in selection
		* `explorer` - Use file explorer, including navigating subdirectories
* **Returns:** path selected, or `nil` if user canceled
* **Throws**
	* `"forbidden drive"` - attempted search on `Z:`
	* `"invalid path"` - could not find `/` in path

#### fs.ask_select_dir

* `string fs.ask_select_dir(string prompt, string path[, bool opts {bool explorer}])`

Prompt the user to select a directory.

* **Arguments**
	* `prompt` - Text to ask the user
	* `path` - Directory to search
	* `opts` (optional) - Option flags
		* `explorer` - Use file explorer, including navigating subdirectories
* **Returns:** path selected, or `nil` if user canceled
* **Throws**
	* `"forbidden drive"` - attempted search on `Z:`

#### fs.find

* `string fs.find(string pattern[, bool opts {bool first}])`

Searches for a file based on a wildcard pattern. Returns the last result, unless `first` is specified, or `nil` if nothing was found.

Pattern can use `?` for search values, for example `00017?02` will match `00017002`, `00017102`, etc. Wildcards are also accepted.

* **Arguments**
	* `pattern` - Pattern to search for
	* `opts` (optional) - Option flags
		* `first` - Return first result instead of last
* **Returns:** found file, or `nil` if nothing is found
* **Throws**
	* `"failed to find <path> (##)"` - error when attempting to find path, with FatFs error number

#### fs.find_not

* `string fs.find(string pattern)`

Searches for a free filename based on a pattern.

Pattern can use `?` for search values, for example `nand_??.bin` will check to see if `nand_00.bin` exists. If it doesn't, it returns this string. Otherwise, it checks if `nand_01.bin` exists and keeps going until an unused filename can be found.

* **Arguments**
	* `pattern` - Pattern to search for
* **Returns:** found file
* **Throws**
	* `"failed to find <path> (##)"` - error when attempting to find path, with FatFs error number

#### fs.find_all

* `string fs.find_all(string dir, string pattern[, table opts {bool recursive}])`

Search for all files that match a pattern.
* **Arguments**
	* `dir` - Directory to search
    * `pattern` - Filename pattern
	* `opts` (optional) - Option flags
		* `recursive` - Remove directories recursively
* **Throws**
	* `"could not open directory"` - failed to open directory

#### fs.allow

* `bool fs.allow(string path[, table flags {bool ask_all}])`

Check for and request permission to write to a sensitive path.

* **Arguments**
	* `path` - Path to request permission for
	* `opts` (optional) - Option flags
		* `ask_all` - Request to write to all files in directory
* **Returns:** `true` if granted, `false` if user declines

#### fs.img_mount

* `void fs.img_mount(string path)`

Mount an image file. Can be anything mountable through the file browser.

* **Arguments**
	* `path` - Path to image file
* **Throws**
	* `"failed to mount <path>"` - not a valid image file

#### fs.img_umount

* `void fs.img_umount()`

Unmount the currently mounted image file.

#### fs.get_img_mount

* `string fs.get_img_mount()`

Get the currently mounted image file.

* **Returns:** path to file, or `nil` if none is mounted

#### fs.hash_file

* `string fs.hash_file(string path, int offset, int size[, table opts {bool sha1}])`

Calculate the hash for a file. Uses SHA-256 unless `sha1` is specified. To hash an entire file, `size` should be `0`.

> [!TIP]
> * Use `fs.verify_with_sha_file` to compare with a corresponding `.sha` file.
> * Use `util.bytes_to_hex` to convert the result to printable hex characters.

> [!NOTE]
> Using an offset that is not `0`, with a size of `0` (to hash to end of file), is currently undefined behavior. In the future this should work properly.

* **Arguments**
	* `path` - File to hash
	* `offset` - Data offset
	* `size` - Amount of data to hash, or `0` to hash to end of file
	* `opts` (optional) - Option flags
		* `sha1` - Use SHA-1
* **Returns:** SHA-256 or SHA-1 hash as byte string
* **Throws**
	* `"failed to stat <path>"` - could not stat file to get size
	* `"FileGetSha failed on <path>"` - could not read file or user canceled

#### fs.hash_data

* `string fs.hash_data(string data[, table opts {bool sha1}])`

Calculate the hash for some data. Uses SHA-256 unless `sha1` is specified.

> [!TIP]
> * Use `util.bytes_to_hex` to convert the result to printable hex characters.

* **Arguments**
	* `data` - Data to hash
	* `opts` (optional) - Option flags
		* `sha1` - Use SHA-1
* **Returns:** SHA-256 or SHA-1 hash as byte string

#### fs.verify

* `bool fs.verify(string path)`

Verify the integrity of a file.

> [!NOTE]
> This is for files that have their own hashes built-in. For verifying against a corresponding `.sha` file, use `fs.verify_with_sha_file`.

* **Arguments**
	* `path` - File to verify
* **Returns:** `true` if successful, `false` if failed or not verifiable

#### fs.verify_with_sha_file

* `bool fs.verify_with_sha_file(string path)`

Calculate the hash of a file and compare it with a corresponding `.sha` file.

> [!IMPORTANT]
> This currently assumes SHA-256. In the future this may automatically use SHA-1 when appropriate, based on the `.sha` file size.

TODO: add errors for fs.read_file here

* **Argumens**
	* `path` - File to hash
* **Returns:** `true` if successful, `false` if failed, `nil` if `.sha` file could not be read
* **Throws**
	* `"failed to stat <path>"` - could not stat file to get size
	* `"FileGetSha failed on <path>"` - could not read file or user canceled

#### fs.exists

* `bool fs.exists(string path)`

Check if an item exists.

* **Arguments**
	* `path` - Path to file or directory
* **Returns:** `true` if exists, `false` otherwise

#### fs.is_dir

* `bool fs.is_dir(string path)`

Check if an item exists, and is a directory.

* **Arguments**
	* `path` - Path to directory
* **Returns:** `true` if exists and is a directory, `false` otherwise

#### fs.is_file

* `bool fs.is_file(string path)`

Check if an item exists, and is a file.

* **Arguments**
	* `path` - Path to file
* **Returns:** `true` if exists and is a file, `false` otherwise

#### fs.sd_is_mounted

* `bool fs.sd_is_mounted()`

Check if the SD card is mounted.

* **Returns:** `true` if SD card is mounted

#### fs.sd_switch

* `void fs.sd_switch([string message])`

Prompt the user to remove and insert an SD card.

* **Arguments**
	* `message` (optional) - Text to prompt the user, defaults to `"Please switch the SD card now."`
* **Throws**
	* `"user canceled"` - user canceled the switch

#### fs.fix_cmacs

* `void fs.fix_cmacs(string path)`

Fix CMACs for a directory.

* **Arguments**
	* `path` - Path to recursively fix CMACs for
* **Throws**
	* `fixcmac failed` - user denied permission, or fixing failed

#### fs.read_file

* `string fs.read_file(string path, int offset, int/string size)`

Read data from a file.

* **Arguments**
	* `path` - File to read
	* `offset` - Data offset
	* `size` - Amount of data to read
* **Returns:** string of data
* **Throws**
	* `"could not allocate memory to read file"` - out-of-memory error when attempting to create the data buffer
	* `"could not read <path> (##)"` - error when attempting to read file, with FatFs error number

#### fs.write_file

* `int fs.write_file(string path, int offset, string data)1

Write data to a file.

* **Arguments**
	* `path` - File to write
	* `offset` - Offset to write to, or the string `"end"` to write at the end of file
	* `data` - Data to write
* **Returns:** amount of bytes written
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"error writing <path> (##)"` - error when attempting to write file, with FatFs error number

#### fs.fill_file

* `void fs.fill_file(string path, int offset, int size, int byte)`

Fill a file with a specified byte.

* **Arguments**
	* `path` - File to write
	* `offset` - Offset to write to
	* `size` - Amount of data to write
	* `byte` - Number between `0x00` and `0xFF` (`0` and `255`) indicating the byte to write
	* `opts` (optional) - Option flags
		* `no_cancel` - Don’t allow user to cancel
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"byte is not between 0x00 and 0xFF (got: ##)"` - byte value is not a single byte
	* `"FileSetByte failed on <path>"` - writing failed or user canceled

#### fs.make_dummy_file

* `void fs.make_dummy_file(string path, int size)`

Create a dummy file.

> [!NOTE]
> The file will contain data from the unused parts of the filesystem. If you need to ensure it's clean, use `fs.fill_file`.

* **Arguments**
	* `path` - File to create
	* `size` - File size to set
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"FileCreateDummy failed on <path>"` - dummy creation failed

#### fs.truncate

* `void fs.truncate(string path, int size)`

Truncate a file to a specific size.

> [!IMPORTANT]
> Does not work for virtual filesystems.

* **Arguments**
	* `path` - File to create
	* `size` - File size to set
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"failed to open <path> (note: this only works on FAT filesystems, not virtual)"` - opening file failed, or a virtual filesystem was used
	* `"failed to seek on <path>"` - seeking file failed
	* `"failed to truncate on <path>"` - truncating file failed

#### fs.key_dump

* `void fs.key_dump(string file[, table opts {bool overwrite}])`

Dumps title keys or seeds. Taken from both SysNAND and EmuNAND. The resulting file is saved to `0:/gm9/out`.

* **Arguments**
	* `file` - One of three supported filenames: `seeddb.bin`, `encTitleKeys.bin`, `decTitleKeys.bin`
	* `opts` (optional) - Option flags
		* `overwrite` - Overwrite files
* **Throws**
	* `"building <file> failed"` - building failed or file already exists and `overwrite` was not used

#### fs.cart_dump

* `void fs.cart_dump(string path, int size[, table opts {bool encrypted}])`

Dump the raw data from the inserted game card. No modifications are made to the data. This means for example, Card2 games will not have the save area cleared.

* **Arguments**
	* `path` - File to write data to
	* `size` - Amount of data to read
	* `opts` (optional) - Option flags
		* `encrypted` - Dump game encrypted, only for DS/DSi games?
* **Throws**
	* `"out of memory"` - out-of-memory error when attempting to create the data buffer
	* `"cart init fail"` - card is not inserted or some other failure when attempting to initialize
	* `"cart dump failed or canceled"` - cart read failed or used canceled

---

### `title` module

#### title.decrypt, title.encrypt

* `void title.decrypt(string path)`
* `void title.decrypt(string path)`

Decrypt or encrypt a title or key database, done in-place.

* **Arguments**
	* `path` - Path to title or key database
* **Throws**
	* `"CryptAesKeyDb failed on <path>"` - could not crypt key database
	* `"CryptGameFile failed on <path>"` - could not crypt title

#### title.install

* `void title.install(string path[, table opts {bool to_emunand}])`

Install a title.

* **Arguments**
	* `path` - File to install
	* `opts` (optional) - Option flags
		* `to_emunand` - Install to EmuNAND
* **Throws**
	* `"InstallGameFile failed on <path>"` - install failed or user canceled

#### title.build_cia

* `void title.build_cia(string path[, table opts {bool legit}])`

Build title as a CIA. Resulting file is put in `0:/gm9/out`.

* **Arguments**
	* `path` - File to build as CIA
	* `opts` (optional) - Option flags
		* `legit` - Build as legit CIA if possible
* **Throws**
	* `"BuildCiaFromGameFile failed on <path>"` - build failed or user canceled

#### title.extract_code

* `void title.extract_code(string src, string dst)`

Extracts the `.code` from a title and decompresses it.

* **Arguments**
	* `src` - File containing `.code`
	* `dst` - Destination to write decompressed `.code`
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"<path> does not have code"` - invalid file type
	* `"failed to extract code from <path>"` - extraction failed

#### title.compress_code

* `void title.compress_code(string src, string dst)`

Compress a `.code` file.

* **Arguments**
	* `src` - Extracted `.code`, like from `title.extract_code`
	* `dst` - Destination to write compressed `.code`
* **Throws**
	* `"writing not allowed: <path>"` - user denied permission
	* `"failed to compress code from <path>"` - compression failed

#### title.apply_ips, title.apply_bps, title.apply_bpm

* `void title.apply_ips(string patch, string src, string target)`
* `void title.apply_bps(string patch, string src, string target)`
* `void title.apply_bpm(string patch, string src, string target)`

Apply an IPS, BPS, or BPM patch.

* **Arguments**
	* `patch` - IPS, BPS, or BPM patch file
	* `src` - File to patch
	* `target` - Destination to write patched file to
* **Throws**
	* `"ApplyIPSPatch failed"` - failed to apply IPS patch
	* `"ApplyBPSPatch failed"` - failed to apply BPS patch
	* `"ApplyBPMPatch failed"` - failed to apply BPM patch

---

### `sys` module

#### sys.boot

* `void sys.boot(string path)`

Boot a FIRM file.

* **Arguments**
	* `path` - FIRM file
* **Throws**
	* `"out of memory"` - out-of-memory error when attempting to create the data buffer
	* `"not a bootable firm"` - file cannot be booted

#### sys.reboot

* `void sys.reboot()`

Reboots the console.

#### sys.power_off

* `void sys.power_off()`

Powers off the console.

#### sys.region

* `string sys.region`

System region, based on SysNAND's `SecureInfo_[AB]` file. Not affected by `SecureInfo_C`.

Possible values: `"JPN"`, `"USA"`, `"EUR"`, `"AUS"`, `"CHN"`, `"KOR"`, `"TWN"`, `nil` (if it does not exist, or the region byte is invalid)

#### sys.serial

* `string sys.serial`

Serial number, based on SysNAND's `SecureInfo_[AB]` file. Not affected by `SecureInfo_C`.

Can be `nil` if the file does not exist.

#### sys.secureinfo_letter

* `string sys.secureinfo_letter`

The letter at the end of the console's `SecureInfo_[AB]` file.

Possible values: `"A"`, `"B"`, `nil` (if it does not exist)

#### sys.sys_id0

* `string sys.sys_id0`

ID0 of SysNAND as a printable hex string.

Can be `nil` if the file does not exist.

#### sys.emu_id0

* `string sys.emu_id0`

ID0 of EmuNAND as a printable hex string.

Can be `nil` if the file does not exist.

#### sys.emu_base

* `int sys.emu_base`

Current EmuNAND base.

#### sys.refresh_info

* `void sys.refresh_info()`

Refresh the following variables:

* `sys.region`
* `sys.serial`
* `sys.secureinfo_letter`
* `sys.sys_id0`
* `sys.emu_id0`
* `sys.emu_base`

This function is automatically called at the beginning of Lua's execution, but errors are caught to prevent a premature exit. This could leave some of the variables at `nil`.

* **Throws**
	* `"could not read SecureInfo"` - `SecureInfo_[AB]` is missing
	* `"SecureInfo region byte is invalid"` - `SecureInfo_[AB]` is not between 0 and 6 inclusive

#### sys.next_emu

* `void sys.next_emu()`

Switch to the next available EmuNAND.

This will automatically call `sys.refresh_info`.

#### sys.check_embedded_backup

* `bool sys.check_embedded_backup()`

Check if `essential.exefs` is embedded into SysNAND, and prompts the user if it isn't. This is the same as the check that is done at boot for GodMode9, but this can be called in a build compiled with `SCRIPT_RUNNER=1` for autorun scripts that require `essential.exefs` to exist.

* **Returns:** `true` if it exists or was created, `false` if user declines, `nil` if console fails genuine NCSD check (modified partition table)

#### sys.check_raw_rtc

* `bool sys.check_raw_rtc()`

Check if the Raw RTC is set correctly, and prompts the user if it isn't. This is the same as the check that is done at boot for GodMode9, but this can be called in a build compiled with `SCRIPT_RUNNER=1` for autorun scripts that require `essential.exefs` to exist.

* **Returns:** `true` if set, `false` if user declines

---

### `util` module

#### util.bytes_to_hex

* `string util.bytes_to_hex(string data)`

Convert a byte string to printable hex characters.

Example: `"test\xaa\xbb\xcc\xdd"` -> `"74657374aabbccdd"`

* **Arguments**
	* `data` - Data to convert
* **Returns:** Hex characters

#### util.hex_to_bytes

* `string util.hex_to_bytes(string hexstring)`

Convert hex characters to a byte string.

Example: `"74657374aabbccdd"` -> `"test\xaa\xbb\xcc\xdd"`

* **Arguments**
	* `hexstring` - Hex characters to convert
* **Returns:** Byte string
* **Throws**
	* `"bad argument #1 to 'char' (number expected, got nil)"` - invalid hex characters

#### util.get_datestamp

* `string util.get_datestamp()`

Returns a date stamp formatted like "241202" for 2024 December 02. Equivalent to `os.date("%y%m%d")`.

* **Returns:** Date stamp

#### util.get_timestamp

* `string util.get_timestamp()`

Returns a date stamp formatted like "010828" for 01:08:28. Equivalent to `os.date("%H%M%S")`.

* **Returns:** Time stamp

#### util.running_as_module

* `bool util.running_as_module()`

Determines if the currently executing script was directly run, or was imported by another script. Useful for scripts that want to be usable directly, while also importable.

> [!NOTE]
> This is not well tested.

* **Returns:** `true` if the current script was imported as a module
