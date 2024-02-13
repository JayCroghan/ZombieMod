# ZombieMod CS2

ZombieMod CS2 is a Metamod plugin that brings CS:S ZombieMod to CS2. With thanks to the CS2Fixes team.

## Installation

- Install [Metamod](https://cs2.poggu.me/metamod/installation/)
- Download the [latest release package](https://github.com/JayCroghan/ZombieMod/releases/latest) for your OS
- Extract the package contents into `game/csgo` on your server
- Configure the plugin cvars as desired in `cfg/zombiemod/zombiemod.cfg`, many features are disabled by default
- OPTIONAL: If you want to setup admins, rename `admins.cfg.example` to `admins.cfg` which can be found in `addons/zombiemod/configs` and follow the instructions within to add admins

## Features
- Windows and Linux support
- Schema system
  - Property access
  - Property updating
- Event hooking
- Custom resource precaching system
- Console chat message management
  - Coloring messages
  - Chat timer
- Movement unlocker
- VScript unlocker
- Preventing console logs
- Buying weapons through chat
  - Limit weapon buy count
- Unlocking commands and convars
- Dumping all convar values
- Memory patching system
- Detour system
- Player Manager
  - Storing players
  - Player authorization
  - Userid Target lookup
  - Admin system
    - Admin commands
    - Admin chat
    - Infractions
      - Kick
      - Ban
      - Mute
      - Gag
      - Removing infractions
- Timers
- Blocking weapon sounds/decals
  - Hooking fire bullets tempent
  - Stopsound
  - Silenced sounds
  - Toggledecals
- Legacy event listener
- trigger_push fix
- Remove player collisions
- Ztele
- Water fix
- Hide command
  - Transmit hook
- Rcon
- HTTP REST API
  - Discord webhook API
- EntFire
- Fix blast damage crashes
- Block self molotov damage
- Anti chat flood
- Full map cycle system
  - Nominations
  - Map vote fix
  - Rock the vote
  - Vote extends
- Top defenders
- Custom map configs
- Navmesh lookup lag fix
- Dummy bot spawning without navmesh
- Zombie:Reborn
  - Full implementation of the Zombie Escape gamemode
  - Player classes
  - Weapon config

## Compilation

### Requirements

- [Metamod:Source](https://www.sourcemm.net/downloads.php/?branch=master) (build 1219 or higher)
- [AMBuild](https://wiki.alliedmods.net/Ambuild)

### Instructions

Follow the instructions below to compile ZombieMod.

```bash
git clone https://github.com/JayCroghan/ZombieMod/ && cd ZombieMod
git submodule update --init --recursive

export MMSOURCE112=/path/to/metamod/
export HL2SDKCS2=/path/to/sdk/submodule

mkdir build && cd build
python3 ../configure.py --enable-optimize --symbol-files --sdks cs2
ambuild
```

Copy the contents of package/ to your server's csgo/ directory.
