# ❓ ABOUT
Follow this project on :bird: [Twitter](https://twitter.com/OpenNetBattle)!

This project aims to be an accurate mmbn6 battle engine that can be used to program custom enemies, chips, navis, or used to make your own mmbn story.
This was originally started to kill some time one summer and I got a little carried away. It's been fun and I hope you have fun with it as I have had making it.

This fork spawned from [Pheelbert](https://github.com/Pheelbert/battlenetwork) who helped me in the very beginning with the tile-based movement. Without him, it wouldn't be here today. For some reason, github fork does not accurately show that I forked from him.

# 🕹️ GET STARTED PLAYING THE GAME

To begin, follow the [Playing Guide](https://github.com/TheMaverickProgrammer/OpenNetBattle/wiki/Playing-Guide).

Below are the default bindings. The engine supports 1 joystick. You can change these bindings from within the game's `Config` screen.

```
ARROWS -> Move / UI options
Z      -> Shoot (hold to charge) / UI Confirm / Interact
X      -> Use chip / UI Cancel / Run
C      -> Special ability / Misc. Action
A      -> Scan Left / Misc. Action
S      -> Scan Right / Bring up chip select GUI / Talk to navi
M      -> Display map
RETURN -> Pause
```

# 🧩 GET STARTED WITH THE CODE
This project uses `master` branch to stage releases  before tagging the final version.
In-progress features and bug fixes go into the `development` branch and can be found [here](https://github.com/TheMaverickProgrammer/OpenNetBattle/tree/development)

You will need:
* [CMAKE](https://cmake.org/download/) 3.19.6 or higher
* [VCPKG](https://vcpkg.io/en/index.html) 
* a C++ compiler and toolchain
* fork/clone this git project

## Build instructions from 7/17/2021
[Build Instructions On YouTube!](https://www.youtube.com/watch?v=5T_kS7DYbvw)

Note that you will need to also install `fluidsynth` with vcpkg and the install guide video is not yet updated.

# 💡 FEATURES
- LUA Scriptable
- Custom players and forms (battle and overworld)
- Custom chips and attacks (timefreeze, etc.)
- Custom enemies and mob arrangements
- Standard battle, liberation missions, and network PVP via lockstep (or make your own battle scenes with the state graph!)
- Overworld
- Navicust
- Supports online play from custom [servers](https://github.com/ArthurCose/Scriptable-OpenNetBattle-Server)
- Servers can script quests, custom battles, PVP, as far as your imagination will let you!
- and so much more

# 🧑🏼‍🤝‍🧑🏼 COMMUNITY 
View the contributor list [here](https://github.com/TheMaverickProgrammer/OpenNetBattle/wiki/Contributing#contributor-list). I could not have made it this far without these special and very talented people!

Join the project official discord [here](https://discord.gg/yAK9MG2)

# SCREENSHOTS
![custom battles](https://m.gjcdn.net/game-screenshot/500/10458364-ll-kgyns54e-v4.webp)
![familiar scenes](https://m.gjcdn.net/game-screenshot/500/10458393-ll-bi6s4dvq-v4.webp)
![navi cust](https://m.gjcdn.net/game-screenshot/500/10459243-ll-adgbvz3h-v4.webp)
![various in-game elements](https://m.gjcdn.net/game-screenshot/500/10459247-ll-btxdhs6y-v4.webp)
![cutscenes](https://m.gjcdn.net/game-screenshot/500/10458367-ll-dch5cyts-v4.webp)
![overworld](https://m.gjcdn.net/game-screenshot/500/10458389-ll-cpxhv9rr-v4.webp)
