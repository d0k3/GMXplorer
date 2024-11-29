# ![GodMode9](https://github.com/d0k3/GodMode9/blob/master/resources/logo.png)
_A full access file browser for the 3DS console_ :godmode:

GodMode9 is a full access file browser for the Nintendo 3DS console, giving you access to your SD card, to the FAT partitions inside your SysNAND and EmuNAND and to basically anything else. Among other functionality (see below), you can copy, delete, rename files and create folders.


## Warning
__This is powerful stuff__, it provides you with the means to do basically any thinkable modification to any system data available on the 3DS console. However, precautions are taken so you don't accidentally damage the data of your console. The write permissions system protects you by providing warnings and forces you to enter an unlock sequence for enabling write permissions. It is not possible to overwrite or modify any important stuff without such unlock sequences and it is not possible to accidentally unlock something.

__As always, be smart, keep backups, just to be safe__.


## Quick start guide
The recommended bootloader for use with GodMode9 is [fastboot3DS](https://github.com/derrekr/fastboot3DS). There are [known issues for some users](https://github.com/d0k3/GodMode9/issues/466) when using the standard setup based on [boot9strap](https://github.com/SciresM/boot9strap) and [Luma3DS](https://github.com/AuroraWright/Luma3DS). If you insist on using that setup follow the instructions found in a [certain guide](https://3ds.hacks.guide). Here's how to set up GodMode9 (and fastboot3DS) up quickly:
* Download [OpenFirmInstaller](https://github.com/d0k3/OpenFirmInstaller/releases/tag/v0.0.9) and follow the quick setup instructions found there.
* Copy the `gm9` folder from the release archive to your SD card. Then, get good versions of `seeddb.bin` and `encTitleKeys.bin` from somewhere (don't ask me!) and put these two files into `sd:/gm9/support` (optional but recommended for full functionality).
* It is also recommended you setup the RTC clock if you're running GodMode9 for the first time. Find the option via HOME button -> `More...`. Also keep in mind that you should fix your system OS clock afterwards. While you're in the `More...` menu, you may also set screen brightness to a fixed value of your choosing and manually calibrate the touch screen (*not recommended* - try the automatic configuration first).
* Helpful hint #1: Go [here](https://3ds.hacks.guide/godmode9-usage) for step by steps on doing some common tasks in GodMode9. Especially users coming from Decrypt9WIP or Hourglass9 may find this to be helpful.
* Helpful hint #2: __Never unlock the red write permission level unless you know exactly what you're doing__. You will notice that prompt when it comes up, it features a completely red screen. It is recommended you stay on the yellow permission level or below at all times to be completely safe. Also read more on the write permissions system below.

You may now run GodMode9 via holding the X Button (or any other button you chose) at startup. See below for a list of stuff you can do with it.


## Buttons in GodMode9
GodMode9 is designed to be intuitive, buttons leading to the results you'd expect. However, some stuff may not be obvious at first glance. So, here's a quick, incomplete rundown of what each button / button combo does.
* __\<A> button__: The \<A> button is the 'confirm' / 'choose' button. It confirms prompts and selects entries in menus. In the main file view, it pulls up a submenu for files and opens directories (use \<R+A> on directories for a submenu, also including the invaluable title search). In the hexviewer, \<A> switches into edit mode.
* __\<B> button__: The \<B> button is the 'cancel' / 'return' button. Use it to leave menus without action, hold it on file operations to cancel said file operations.
* __\<X> button__: In the main file view, the \<X> button deletes (marked) files. With \<R+X> files are renamed.
* __\<Y> button__: In the main file view, the \<Y> button copies and pastes files. With \<R+Y> you can create folders and dummy files.
* __\<L> button__: The \<L> button is the 'mark' button. Use it with \<LEFT> / \<RIGHT> to mark / unmark all files in a folder, hold it and use \<UP> / \<DOWN> to select multiple files.
* __\<R> button__: The \<R> button is the 'switch' button. It switches buttons to their secondary function. Notable exceptions are \<R+L> for a screenshot (works almost anywhere), \<R+LEFT> / \<R+RIGHT> to switch panes and \<R+DOWN> to reload the file listing.
* __\<START> button__: Use the \<START> button to reboot from GodMode9. Use \<R+START> to poweroff your 3DS.
* __\<SELECT> button__: The \<SELECT> button clears or restores the clipboard (depending on if it's empty or not).
* __\<HOME> button__: The \<HOME> button enters the HOME menu, including the scripts / payloads submenus, options for formatting the SD, setting the RTC, and more. The \<POWER> button is an alternative way of entering the HOME menu.
* __\<R+UP> combo__: This little known keycombo, when held at startup, pauses the GodMode9 boot so that you can stare at the splash screen for a little longer.
* __\<R+LEFT> combo__: If you have installed GodMode9 as your bootloader, this keycombo enters the bootmenu. Hold on startup! If you built GodMode9 as SALTMODE and have it as a bootloader, the keycombo is simply the \<START> button.


## How to build this / developer info
Build `GodMode9.firm` via `make firm`. This requires [firmtool](https://github.com/TuxSH/firmtool), [Python 3.5+](https://www.python.org/downloads/) and [devkitARM](https://sourceforge.net/projects/devkitpro/) installed).

You may run `make release` to get a nice, release-ready package of all required files. To build __SafeMode9__ (a bricksafe variant of GodMode9, with limited write permissions) instead of GodMode9, compile with `make FLAVOR=SafeMode9`. To switch screens, compile with `make SWITCH_SCREENS=1`. For additional customization, you may choose the internal font by replacing `font_default.frf` inside the `data` directory. You may also hardcode the brightness via `make FIXED_BRIGHTNESS=x`, whereas `x` is a value between 0...15.

Further customization is possible by hardcoding `aeskeydb.bin` (just put the file into the `data` folder when compiling). All files put into the `data` folder will turn up in the `V:` drive, but keep in mind there's a hard 223.5KiB limit for all files inside, including overhead. A standalone script runner is compiled by providing `autorun.gm9` (again, in the `data` folder) and building with `make SCRIPT_RUNNER=1`. There's more possibility for customization, read the Makefiles to learn more.

To build a .firm signed with SPI boot keys (for ntrboot and the like), run `make NTRBOOT=1`. You may need to rename the output files if the ntrboot installer you use uses hardcoded filenames. Some features such as boot9 / boot11 access are not currently available from the ntrboot environment.


## Bootloader mode
Same as [boot9strap](https://github.com/SciresM/boot9strap) or [fastboot3ds](https://github.com/derrekr/fastboot3DS), GodMode9 can be installed to the system FIRM partition ('FIRM0'). When executed from a FIRM partition, GodMode9 will default to bootloader mode and try to boot, in order, `FIRM from FCRAM` (see [A9NC](https://github.com/d0k3/A9NC/releases)), `0:/bootonce.firm` (will be deleted on a successful boot), `0:/boot.firm`, `1:/boot.firm`. In bootloader mode, hold R+LEFT on boot to enter the boot menu. *Installing GodMode9 to a FIRM partition is only recommended for developers and will overwrite [boot9strap](https://github.com/SciresM/boot9strap) or any other bootloader you have installed in there*.


## Write permissions system
GodMode9 provides a write permissions system, which will protect you from accidentally damaging your system, losing data and/or modifying important system data. To unlock a write permission, an unlock sequence must be entered. This is not possible by accident. The write permission system is based on colors and the top bar on the top screen will change color according to the current write permission level. No permission above the yellow level can be unlocked on SafeMode9.
* __Green:__ Modification to system files is not possible on this permission level. You can't edit or delete savegames and installed data. However, keep in mind that any non-system related or custom stuff on your SD card is not protected.
* __Yellow:__ You can modify system files on this permission level. Data that is unique to your console and cannot be gotten from anywhere else is still not modifiable. Any damages you introduce can be fixed in software, but loss of savegames and installed data is possible if you are not careful. __A NAND backup is highly recommended starting at this level.__
* __Orange:__ This is similar to the yellow permission level, but, in addition, allows edits to console unique data. Any damages you introduce are still fixable in software, but if you play around with this, __having a NAND backup is an absolute requirement__.
* __Red:__ The highest regular permission level. There are no limits to system file edits, and if you are not careful enough, you can brick your console and/or remove your A9LH/B9S installation. Bricks on this level may only be fixable in hardware. __If you don't have a NAND backup at this point, you seem to have a deathwish for your console__.
* __Blue:__ This permission level is reserved for edits to system memory. While, most likely, nothing bad at all will happen, consequences of edits can be unforeseen. There is even a (albeit very small) chance of bricking your console, maybe even permanently. __Tread with caution on this level__.


## Support files
For certain functionality, GodMode9 may need 'support files'. Support files should be placed into either `0:/gm9/support` or `1:/gm9/support`. Support files contain additional information that is required in decryption operations. A list of support files, and what they do, is found below. Please don't ask for support files - find them yourself.
* __`aeskeydb.bin`__: This should contain 0x25keyX, 0x18keyX and 0x1BkeyX to enable decryption of 7x / Secure3 / Secure4 encrypted NCCH files, 0x11key95 / 0x11key96 for FIRM decrypt support and 0x11keyOTP / 0x11keyIVOTP for 'secret' sector 0x96 crypto support. Entrypoints other than [boot9strap](https://github.com/SciresM/boot9strap) or [fastboot3ds](https://github.com/derrekr/fastboot3DS) may require a aeskeydb.bin file. This is now included in standard releases of GM9. No need to hunt down the file!
* __`seeddb.bin`__: This file is optional and required to decrypt and mount seed-encrypted NCCHs and CIAs (if the seed in question is not installed to your NAND). Note that your seeddb.bin must also contain the seed for the specific game you need to decrypt.
* __`encTitleKeys.bin`__ / __`decTitleKeys.bin`__: These files are optional and provide titlekeys, which are required to decrypt and install contents downloaded from CDN (for DSi and 3DS content).

### Fonts and translations
GodMode9 also supports custom fonts and translations as support files. These both use custom formats, fonts use FRF (Font RIFF) files which can be created using the `fontriff.py` Python script in the 'utils' folder. Translations use TRF (Translation RIFF) files from the `transriff.py` script. Examples of the inputs to these scripts can be found in the 'fonts' and 'languages' folders of the 'resources' folder respectively.

TRF files can be placed in `0:/gm9/languages` to show in the language menu accessible from the HOME menu and shown on first load. Official translations are provided from the community via the [GodMode9 Crowdin](https://crowdin.com/project/GodMode9). Languages can use a special font by having an FRF with the same name, for example `en.trf` and `en.frf`.

## Drives in GodMode9
GodMode9 provides access to system data via drives, a listing of what each drive contains and additional info follows below. Some of these drives are removable (such as drive `7:`), some will only turn up if they are available (drive `8:` and everything associated with EmuNAND, f.e.). Information on the 3DS console file system is also found on [3Dbrew.org](https://3dbrew.org/wiki/Flash_Filesystem).
* __`0: SDCARD`__: The SD card currently inserted into the SD card slot. The `0:/Nintendo 3DS` folder contains software installs and extdata and is specially protected via the write permission system. The SD card can be unmounted from the root directory via the R+B buttons, otherwise the SD card is always available.
* __`1: SYSNAND CTRNAND`__: The CTRNAND partition on SysNAND. This contains your 3DS console's operating system and system software installs. Data in here is protected by the write permissions system.
* __`2: SYSNAND TWLN`__: The TWLN partition on SysNAND. This contains your 3DS console's TWL mode operating system and system software installs. Data in here is protected by the write permissions system.
* __`3: SYSNAND TWLP`__: The TWLP partition on SysNAND. This contains photos taken while in TWL mode.
* __`A: SYSNAND SD`__: This drive is used for special access to data on your SD card. It actually links to a subfolder inside `0:/Nintendo 3DS` and contains software and extdata installed to SD from SysNAND. Crypto in this folder is handled only when accessed via the `A:` drive (not from `0:`). This is protected by the write permissions system.
* __`S: SYSNAND VIRTUAL`__: This drive provides access to all partitions of the SysNAND, some of them critical for base system functionality. This is protected by the write permissions system, but, when unlocked, modifications can brick the system.
* __`4: EMUNAND CTRNAND`__: Same as `1:`, but handles the CTRNAND on EmuNAND. For multi EmuNAND setups, the currently active EmuNAND partition can be switched via the HOME menu.
* __`5: EMUNAND TWLN`__: Same as `2`, but handles TWLN on EmuNAND. No write protection here, cause this partition is never used on EmuNAND.
* __`6: EMUNAND TWLP`__: Same as `3`, but handles TWLP on EmuNAND.
* __`B: EMUNAND SD`__: Same as `A:`, but handles the `0:/Nintendo 3DS` subfolder associated with EmuNAND. In case of linked NANDs, this is identical with `A:`. This is also protected by the write permissions system.
* __`E: EMUNAND VIRTUAL`__: Same as `S:`, but handles system partitions on EmuNAND. No bricking risk here as EmuNAND is never critical to system functionality.
* __`7: FAT IMAGE / IMGNAND CTRNAND`__: This provides access to mounted FAT images. When a NAND image is mounted, it provides access to the mounted NAND image's CTRNAND.
* __`8: BONUS DRIVE / IMGNAND TWLN`__: This provides access to the bonus drive on SysNAND. The bonus drive can be setup via the HOME menu on 3DS consoles that provide the space for it. When a NAND image is mounted, this provides access to the mounted NAND image's TWLN.
* __`9: RAM DRIVE / IMGNAND TWLP`__: This provides access to the RAM drive. All data stored inside the RAM drive is temporary and will be wiped after a reboot. When a NAND image is mounted, this provides access to the mounted NAND image's TWLP.
* __`I: IMGNAND VIRTUAL`__: When a NAND image is mounted, this provides access to the partitions inside the NAND image.
* __`C: GAMECART`__: This is read-only and provides access to the game cartridge currently inserted into the cart slot. This can be used for dumps of CTR and TWL mode cartridges. Flash cards are supported only to a limited extent.
* __`G: GAME IMAGE`__: CIA/NCSD/NCCH/EXEFS/ROMFS/FIRM images can be accessed via this drive when mounted. This is read-only.
* __`K: AESKEYDB IMAGE`__: An `aeskeydb.bin` image can be mounted and accessed via this drive. The drive shows all keys inside the aeskeydb.bin. This is read-only.
* __`T: TICKET.DB IMAGE / BDRI IMAGE`__: Ticket database files can be mounted and accessed via this drive. This provides easy and quick access to all tickets inside the `ticket.db`. This drive also provides access to other BDRI images, such as the Title database (`title.db`).
* __`M: MEMORY VIRTUAL`__: This provides access to various memory regions. This is protected by a special write permission, and caution is advised when doing modifications inside this drive. This drive also gives access to `boot9.bin`, `boot11.bin` (boot9strap only) and `otp.mem` (sighaxed systems only).
* __`V: VRAM VIRTUAL`__: This drive resides in part of ARM9 internal memory and contains files essential to GodMode9. The font (in FRF format), the splash logo (in PNG format) and the readme file are found there, as well as any file that is provided inside the `data` folder at build time. This is read-only.
* __`Y: TITLE MANAGER`__: The title manager is accessed via the HOME menu and provides easy access to all installed titles.
* __`Z: LAST SEARCH`__: After a search operation, search results are found inside this drive. The drive can be accessed at a later point to return to the former search results.


## Digital preservation
GodMode9 is one of the most important tools for digital preservation of 3DS content data. Here's some stuff you should know:
* __Dumping game cartridges (size < 4GiB)__: Game cartridges turn up inside the `C:` drive (see above). For most carts all you need to do is copy the `.3DS` game image to some place of your choice. Game images dumped by GodMode9 contain no identifying info such as private headers or savegames. Private headers can be dumped in a separate image.
* __Dumping game cartridges (size = 4GiB)__: Everything written above applies here as well. However, the FAT32 file system (which is what the 3DS uses) is limited to _4GiB - 1byte_. Take note that the `.3DS` game image, as provided by GodMode9 actually misses the last byte in these cases. That byte is 0xFF and unused in all known cases. It is not required for playing the image. If you need to check, we also provide split files (`.000`, `.001)`, which contain all the data. If you need a valid checksum for the `.3DS` game image, append a 0xFF byte before checking.
* __Building CIAs (all types)__: You may convert compatible file types (game images, installed content, CDN content) to the CIA installable format using the A button menu. To get a list of installed content, press HOME, select `Title manager` and choose a drive. Take note that `standard` built CIAs are decrypted by default (decryption allows better compression by ZIP and 7Z). If you should need an encrypted CIA for some reason, apply the encryption to the CIA afterwards.
* __Building CIAs (legit type)__: Installed content can be built as `legit` or `standard` CIA. Legit CIAs preserve more of the original data and are thus recommended for preservation purposes. When building legit CIAs, GodMode9 keeps the original crypto and tries to find a genuine, signature-valid ticket. If it doesn't find one on your system, it will use a generic ticket instead. If it only finds a personalized one, it still offers to use a generic ticket. It is not recommended to use personalized tickets - only choose this if you know what you're doing.
* __Checking CIAs__: You may also check your CIA files with the builtin `CIA checker tool`. Legit CIAs with generic tickets are identified as `Universal Pirate Legit`, which is the recommended preservation format where `Universal Legit` is not available. Note: apart from system titles, `Universal Legit` is only available for a handful of preinstalled games from special edition 3DS consoles.


## What you can do with GodMode9
With the possibilites GodMode9 provides, not everything may be obvious at first glance. In short, __GodMode9 includes improved versions of basically everything that Decrypt9 has, and more__. Any kind of dumps and injections are handled via standard copy operations and more specific operations are found inside the A button menu. The A button menu also works for batch operations when multiple files are selected. For your convenience a (incomplete!) list of what GodMode9 can do follows below.

### Basic functionality
* __Manage files on all your data storages__: You wouldn't have expected this, right? Included are all standard file operations such as copy, delete, rename files and create folders. Use the L button to mark multiple files and apply file operations to multiple files at once.
* __Make screenshots__: Press R+L anywhere. Screenshots are stored in PNG format.
* __Use multiple panes__: Press R+left|right. This enables you to stay in one location in the first pane and open another in the second pane.
* __Search drives and folders__: Just press R+A on the drive / folder you want to search.
* __Compare and verify files__: Press the A button on the first file, select `Calculate SHA-256`. Do the same for the second file. If the two files are identical, you will get a message about them being identical. On the SDCARD drive (`0:`) you can also write an SHA file, so you can check for any modifications at a later point.
* __Hexview and hexedit any file__: Press the A button on a file and select `Show in Hexeditor`. A button again enables edit mode, hold the A button and press arrow buttons to edit bytes. You will get an additional confirmation prompt to take over changes. Take note that for certain files, write permissions can't be enabled.
* __View/edit text files in a text editor__: Press the A button on a file and select `Show in Text Editor` (only shows up for actual text files). You can enable wordwrapped mode via R+Y, and navigate around the file via R+X and the dpad.
* __Chainload FIRM payloads__: Press the A button on a FIRM file, select `FIRM options` -> `Boot FIRM`. Keep in mind you should not run FIRMs from dubious sources and that the write permissions system is no longer in place after booting a payload.
* __Chainload FIRM payloads from a neat menu__: The `payloads` menu is found inside the HOME button menu. It provides any FIRM found in `0:/gm9/payloads` for quick chainloading.
* __Inject a file to another file__: Put exactly one file (the file to be injected from) into the clipboard (via the Y button). Press A on the file to be injected to. There will be an option to inject the first file into it.

### Scripting functionality
* __Run .gm9 scripts from anywhere on your SD card__: You can run scripts in .gm9 format via the A button menu. .gm9 scripts use a shell-like language and can be edited in any text editor. For an overview of usable commands have a look into the sample scripts included in the release archive. *Don't run scripts from untrusted sources.*
* __Run .gm9 scripts via a neat menu__: Press the HOME button, select `More...` -> `Scripts...`. Any script you put into `0:/gm9/scripts` (subdirs included) will be found here. Scripts ran via this method won't have the confirmation at the beginning either.

### SD card handling
* __Format your SD card / setup an EmuNAND__: Press the HOME button, select `More...` -> `SD format menu`. This also allows to setup a RedNAND (single/multi) or GW type EmuNAND on your SD card. You will get a warning prompt and an unlock sequence before any operation starts.
* __Handle multiple EmuNANDs__: Press the HOME button, select `More...` -> `Switch EmuNAND` to switch between EmuNANDs / RedNANDs. (Only available on multi EmuNAND / RedNAND systems.)
* __Run it without an SD card / unmount the SD card__: If no SD card is found, you will be offered to run without the SD card. You can also unmount and remount your SD card from the file system root at any point.
* __Direct access to SD installed contents__: Just take a look inside the `A:`/`B:` drives. On-the-fly-crypto is taken care for, you can access this the same as any other content.
* __Set (and use) the RTC clock__: For correct modification / creation dates in your file system, you need to setup the RTC clock first. Press the HOME Button and select `More...` to find the option. Keep in mind that modifying the RTC clock means you should also fix system OS time afterwards.

### Game file handling
* __List titles installed on your system__: Press HOME and select `Title manager`. This will also work via R+A for `CTRNAND` and `A:`/`B:` drives. This will list all titles installed in the selected location.
* __Install titles to your system__: Just press A on any file you want installed and select `Install game image` from the submenu. Works with NCCH / NCSD / CIA / DSiWare SRLs / 3DS CDN TMDs / DSi CDN TMDs / NUS TMDs.
* __(Batch) Uninstall titles from your system__: Most easily done via the HOME menu `Title manager`. Just select one or more titles and find the option inside the `Manage title...` submenu.
* __Build CIAs from NCCH / NCSD (.3DS) / SRL / TMD__: Press A on the file you want converted and the option will be shown. Installed contents are found most easily via the HOME menu `Title manager`. Where applicable, you will also be able to generate legit CIAs. Note: this works also from a file search and title listing.
* __Dump CXIs / NDS from TMD (installed contents)__: This works the same as building CIAs, but dumps decrypted CXIs or NDS rom dumps instead. Note: this works also from a file search and title listing.
* __Decrypt, encrypt and verify NCCH / NCSD / CIA / BOSS / FIRM images__: Options are found inside the A button menu. You will be able to decrypt/encrypt to the standard output directory or (where applicable) in place.
* __Decrypt content downloaded from CDN / NUS__: Press A on the file you want decrypted. For this to work, you need at least a TMD file (`encTitlekeys.bin` / `decTitlekeys.bin` also required, see _Support files_ below) or a CETK file. Either keep the names provided by CDN / NUS, or rename the downloaded content to `(anything).nus` or `(anything).cdn` and the CETK to `(anything).cetk`.
* __Batch mode for the above operations__: Just select multiple files of the same type via the L button, then press the A button on one of the selected files.
* __Access any file inside NCCH / NCSD / CIA / FIRM / NDS images__: Just mount the file via the A button menu and browse to the file you want. For CDN / NUS content, prior decryption is required for full access.
* __Rename your NCCH / NCSD / CIA / NDS / GBA files to proper names__: Find this feature inside the A button menu. Proper names include title id, game name, product code and region.
* __Trim NCCH / NCSD / NDS / GBA / FIRM / NAND images__: This feature is found inside the A button menu. It allows you to trim excess data from supported file types. *Warning: Excess data may not be empty, bonus drives are stored there for NAND images, NCSD card2 images store savedata there, for FIRMs parts of the A9LH exploit may be stored there*.
* __Dump 3DS / NDS / DSi type retail game cartridges__: Insert the cartridge and take a look inside the `C:` drive. You may also dump private headers from 3DS game cartridges. The `C:` drive also gives you read/write access to the saves on the cartridges. Note: For 4GiB cartridges, the last byte is not included in the .3ds file dump. This is due to restrictrions of the FAT32 file system.

### NAND handling
* __Directly mount and access NAND dumps or standard FAT images__: Just press the A button on these files to get the option. You can only mount NAND dumps from the same console.
* __Restore NAND dumps while keeping your A9LH / sighax installation intact__: Select `Restore SysNAND (safe)` from inside the A button menu for NAND dumps.
* __Restore / dump NAND partitions or even full NANDs__: Just take a look into the `S:` (or `E:`/ `I:`) drive. This is done the same as any other file operation.
* __Transfer CTRNAND images between systems__: Transfer the file located at `S:/ctrnand_full.bin` (or `E:`/ `I:`). On the receiving system, press A, select `CTRNAND Options...`, then `Transfer to NAND`.
* __Embed an essential backup right into a NAND dump__: This is available in the A button menu for NAND dumps. Essential backups contain NAND header, `movable.sed`, `LocalFriendCodeSeed_B`, `SecureInfo_A`, NAND CID and OTP. If your local SysNAND does not contain an embedded backup, you will be asked to do one at startup. To update the essential SysNAND backup at a later point in time, press A on `S:/nand.bin` and select `NAND image options...` -> `Update embedded backup`.
* __Install an AES key database to your NAND__: For `aeskeydb.bin` files the option is found in `aeskeydb.bin options` -> `Install aeskeydb.bin`. Only the recommended key database can be installed (see above). With an installed key database, it is possible to run the GodMode9 bootloader completely from NAND.
* __Install FIRM files to your NAND__: Found inside the A button menu for FIRM files, select `FIRM options` -> `Install FIRM`. __Use this with caution__ - installing an incompatible FIRM file will lead to a __brick__. The FIRMs signature will automagically be replaced with a sighax signature to ensure compatibility.
* __Actually use that extra NAND space__: You can set up a __bonus drive__ via the HOME menu, which will be available via drive letter `8:`. (Only available on systems that have the extra space.)
* __Fix certain problems on your NANDs__: You can fix CMACs for a whole drive (works on `A:`, `B:`, `S:` and `E:`) via an entry in the R+A button menu, or even restore borked NAND headers back to a functional state (inside the A button menu of borked NANDs and available for `S:/nand_hdr.bin`). Recommended only for advanced users!

### System file handling
* __Check and fix CMACs (for any file that has them)__: The option will turn up in the A button menu if it is available for a given file (f.e. system savegames, `ticket.db`, etc...). This can also be done for multiple files at once if they are marked.
* __Mount ticket.db files and dump tickets__: Mount the file via the A button menu. Tickets are sorted into `eshop` (stuff from eshop), `system` (system tickets), `unknown` (typically empty) and `hidden` (hidden tickets, found via a deeper scan) categories. All tickets displayed are legit, fake tickets are ignored
* __Inject any NCCH CXI file into Health & Safety__: The option is found inside the A button menu for any NCCH CXI file. NCCH CXIs are found, f.e. inside of CIAs. Keep in mind there is a (system internal) size restriction on H&S injectable apps.
* __Inject and dump GBA VC saves__: Find the options to do this inside the A button menu for `agbsave.bin` in the `S:` drive. Keep in mind that you need to start the specific GBA game on your console before dumping / injecting the save. _To inject a save it needs to be in the clipboard_.
* __Dump a copy of boot9, boot11 & your OTP__: This works on sighax, via boot9strap only. These files are found inside the `M:` drive and can be copied from there to any other place.

### Support file handling
* __Build `decTitleKeys.bin` / `encTitleKeys.bin` / `seeddb.bin`__: Press the HOME button, select `More...` -> `Build support files`. `decTitleKeys.bin` / `encTitleKeys.bin` can also be created / merged from tickets, `ticket.db` and merged from other `decTitleKeys.bin` / `encTitleKeys.bin` files via the A button menu.
* __Build, mount, decrypt and encrypt `aeskeydb.bin`__: AES key databases can be merged from other `aeskeydb.bin` or build from legacy `slot??Key?.bin` files. Just select one or more files, press A on one of them and then select `Build aeskeydb.bin`. Options for mounting, decrypting and encrypting are also found in the A button menu.


## License
You may use this under the terms of the GNU General Public License GPL v2 or under the terms of any later revisions of the GPL. Refer to the provided `LICENSE.txt` file for further information.

## Contact info
You can chat directly with us via IRC @ #GodMode9 on [libera.chat](https://web.libera.chat/#GodMode9) or [Discord](https://discord.gg/BRcbvtFxX4)!

## Credits
This tool would not have been possible without the help of numerous people. Thanks go to (in no particular order)...
* **Archshift**, for providing the base project infrastructure
* **Normmatt**, for sdmmc.c / sdmmc.h and gamecart code, and for being of great help on countless other occasions
* **Cha(N)**, **Kane49**, and all other FatFS contributors for [FatFS](http://elm-chan.org/fsw/ff/00index_e.html)
* **Wolfvak** for ARM11 code, FIRM binary launcher, exception handlers, PCX code, Makefile and for help on countless other occasions
* **SciresM** for helping me figure out RomFS and for boot9strap
* **SciresM**, **Myria**, **Normmatt**, **TuxSH** and **hedgeberg** for figuring out sighax and giving us access to bootrom
* **ihaveamac** for first developing the simple CIA generation method and for being of great help in porting it
* **wwylele** and **aspargas2** for documenting and implementing the DISA, DIFF, and BDRI formats
* **dratini0** for savefile management, based on [TWLSaveTool](https://github.com/TuxSH/TWLSaveTool/)
* **Pk11** for unicode and translation support
* **b1l1s** for helping me figure out A9LH compatibility
* **Gelex** and **AuroraWright** for helping me figure out various things
* **stuckpixel** for the new 6x10 font and help on various things
* **Al3x_10m** for help with countless hours of testing and useful advice
* **WinterMute** for helping me with his vast knowledge on everything gamecart related
* **profi200** for always useful advice and helpful hints on various things
* **windows-server-2003** for the initial implementation of if-else-goto in .gm9 scripts
* **Kazuma77** for pushing forward scripting, for testing and for always useful advice
* **TurdPooCharger** for being one of the most meticulous software testers around
* **JaySea**, **YodaDaCoda**, **liomajor**, **Supster131**, **imanoob**, **Kasher_CS** and countless others from freenode #Cakey and the GBAtemp forums for testing, feedback and helpful hints
* **Shadowhand** for being awesome and [hosting my nightlies](https://d0k3.secretalgorithm.com/)
* **Plailect** for putting his trust in my tools and recommending this in [The Guide](https://3ds.guide/)
* **SirNapkin1334** for testing, bug reports and for hosting the original GodMode9 Discord server
* **Lilith Valentine** for testing and helpful advice
* **Project Nayuki** for [qrcodegen](https://github.com/nayuki/QR-Code-generator)
* **Amazingmax fonts** for the Amazdoom font
* **nevumx** for turning the text viewer into a text editor with UTF-8 and LF/CRLF support
* The fine folks on **the official GodMode9 IRC channel and Discord server**
* The fine folks on **freenode #Cakey**
* All **[3dbrew.org](https://www.3dbrew.org/wiki/Main_Page) editors**
* Everyone I possibly forgot, if you think you deserve to be mentioned, just contact us!
