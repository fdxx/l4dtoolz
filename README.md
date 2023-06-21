## About
Valve server plugin, unlock the max players limit and tickrate limit on L4D2. Make sure you have deleted the other versions of `l4dtoolz` and `tickrate_enabler` before using.

- `-maxplayers`: Command Line Options, Set the maxplayers (bot and human players), default value is 31.
- `sv_maxplayers`: Console Variables, Set the max human players allowed to join the server. when the lobby is full, you still need to remove the lobby reservation and set `sv_allow_lobby_connect_only 0`
- `sv_lobby_unreserve`: Console Commands, Set lobby reservation cookie to 0.
- `-tickrate`: Command Line Options, Set the game's tickrate. default value is 30.

## Build manually

Debian:
```shell
dpkg --add-architecture i386
apt update
apt install -y clang g++-multilib git make

mkdir temp && cd temp
git clone --depth=1 -b l4d2 --recurse-submodules https://github.com/alliedmodders/hl2sdk hl2sdk-l4d2
git clone --depth=1 https://github.com/fdxx/l4dtoolz

cd l4dtoolz
make -f makefile_linux all
## Check the l4dtoolz/release folder.
```

Windows:

- Download [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).
- Double-click the installation package, install Desktop development with C++, Make sure tools like MSVC are checked.
- From the start menu find `x86 Native Tools Command Prompt for VS` to open the command prompt window.

```shell
## Create a folder on the desktop, and then use the `cd` command to enter the folder

git clone --depth=1 -b l4d2 --recurse-submodules https://github.com/alliedmodders/hl2sdk hl2sdk-l4d2
git clone --depth=1 https://github.com/fdxx/l4dtoolz

cd l4dtoolz
nmake -f makefile_windows all
## Check the l4dtoolz/release folder.
```
## Credits

- Original `l4dtoolz` plugin and other forked versions
- Original `tickrate_enabler` plugin and other forked versions
- [AlliedModders](https://github.com/alliedmodders)
- [SourceScramble](https://github.com/nosoop/SMExt-SourceScramble)
