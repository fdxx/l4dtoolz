## About
Valve server plugin, unlock the max players limit and tickrate limit on L4D2. Make sure you have deleted the other versions of **l4dtoolz** and **tickrate_enabler** before using.

- `-tickrate`: Command Line Options, Set the game's tickrate. default value is 30.
- `-maxplayers`: Command Line Options, Set the maxplayers (bot and human players), default value is 31.
- `sv_maxplayers`: Console Variables, Set the max human players allowed to join the server. when the lobby is full, you still need to remove the lobby reservation and set `sv_allow_lobby_connect_only 0`
- `sv_lobby_unreserve`: Console Commands, Set lobby reservation cookie to 0.

## Build manually

```bash
## Debian as an example.
apt update && apt install -y apt-transport-https lsb-release wget curl software-properties-common gnupg g++-multilib git make
bash <(curl -fsSL https://apt.llvm.org/llvm.sh) 18

echo 'export PATH=/usr/lib/llvm-18/bin:$PATH' >> /etc/profile
echo 'export CC=clang' >> /etc/profile
echo 'export CXX=clang++' >> /etc/profile
echo 'export XMAKE_ROOT=y' >> /etc/profile
source /etc/profile

wget https://xmake.io/shget.text -O - | bash
source ~/.xmake/profile

mkdir temp && cd temp
git clone --depth=1 -b l4d2 https://github.com/alliedmodders/hl2sdk hl2sdk-l4d2
git clone --depth=1 https://github.com/fdxx/l4dtoolz

cd l4dtoolz
xmake f -c --HL2SDKPATH=../hl2sdk-l4d2
xmake -rv l4dtoolz

## Check the l4dtoolz/release folder.
```

## Related plugins (used together with this plugin)
- [l4d2_tick_network_tweaks](https://github.com/fdxx/l4d2_plugins) : Tweaks server related network parameters to cooperate high tickrate.
- [l4d2_lobby_match_manager](https://github.com/fdxx/l4d2_plugins) : Auto unreserve when lobby full. and other lobby reservation related modifications.

## Credits
- Original `l4dtoolz` plugin and other forked versions
- Original `tickrate_enabler` plugin and other forked versions
- [AlliedModders](https://github.com/alliedmodders)
- [SourceScramble](https://github.com/nosoop/SMExt-SourceScramble)
