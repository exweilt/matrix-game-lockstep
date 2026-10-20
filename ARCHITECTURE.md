# Planetary Battles Engine (MatrixGame) Architecture

This document describes the engine structure for the *Planetary Battles* 3D RTS engine (`MatrixGame`) and beyond that.

---

## 0. The Project Set Up

The game can be built two different ways: 
- As a DLL. This way the entry point is in `MatrixGame/src/MatrixGameDLL.cpp`. The primary game written in Delphi can call the DLL
    and pass some context into it, like Sound Engine, or Fonts etc.
- As a standalone .EXE. This way the entry point is inside `MatrixGame/src/MatrixGame.cpp`

We need to support both ways. To be precise I use .EXE builds for quick testing during the development, while .DLL
is the true end goal. See CMake files if you need it.


Now there is some messy architecture here: we have 2 distinct CMake targets, corresponding to
`MatrixGame` and `MatrixLib` folders. MatrixLib is supposed to be a kind of engine the MatrixGame is written with,
so the game specific code should be inside `MatrixGame`, but actually it seems like there is two way dependency here,
as the main game loop in located inside `L3GRun()` of `MatrixLib/3G/3g.cpp`.

`MatrixLib` contains the basic concepts such as working with files, textures, memory, reading and rendering a custom 3D format, game packaging etc.

`MatrixGame` contains the game specific concepts. Inside `MatrixGame/src` there are directories and files for VFX, GUI, some game logic,
fonts, in-game cameras, robots, in game map, in game objects, water etc. Very important there are also directory `Network` with concepts specific to the Networking mod.

There is also a `ThirdParty` directory with its own CMake, where some libs reside.


One of the most important files are:
- `MatrixLib/3G/3g.cpp`
- `MatrixGame/src/MatrixGame.cpp`
- `MatrixGame/src/MatrixLogic.cpp`
- `MatrixGame/src/MatrixMap.cpp`
- `MatrixGame/src/MatrixMapStatic.cpp`
- `MatrixGame/src/Network/Network.cpp`

---

## 1. Engine Core & Main Loop

The main game loop is located inside `L3GRun()` of `MatrixLib/3G/3g.cpp`.

Inside that while loop, there is importantly:
- `g_Network.process_network_frame()`
- `g_FormCur->Takt(delta);` which leads to `void CFormMatrixGame::Takt(int step)` inside of `MatrixFormGame`, which leads to `void CMatrixMapLogic::Takt(int step)` inside of `MatrixLogic`.
- `g_FormCur->Draw();` not the only place where drawing occurs.

---

## 2. Network Mod architecture so far

Inside `Network` there is `g_Network` manager which holds all context of the mod and multiplayer.

`Message.cpp` encapsulates the logic of "messages", which are kind of atoms of client-server communication. It allows to wrap some data and stream it into bytes so ENet message can be formed and vice versa.

`Snapshot.cpp` is about capturing snapshots of various entities like a robot might have: HP, position, rotation to sync.

Right now the clients are very close to "stupid terminals", their job is to translate the image of the game, that happens on the server.

A server mostly runs the base game and sends the snapshots for all the relevant entities to all the clients every network frame, when Server receives some inputs from a client as a Message it executes it.

Clients are just drawing and interpolating what the Server is sending. If you make an input it is sent to the server for execution. The server sends the results.
