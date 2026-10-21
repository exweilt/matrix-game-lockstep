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



Here is a detailed architectural breakdown of the **Planetary Battles (`MatrixGame`)** engine and the current state of your multiplayer state-sync mod.

---

### 1. The Game Loop: Main Update Tick & Execution Order

Despite the conceptual split between `MatrixLib` (engine) and `MatrixGame` (game logic), the main loop lives inside `MatrixLib` in `L3GRun()` (`MatrixLib/3G/3g.cpp`).

```504:639:G:\mgl3\MatrixLib\3G\3g.cpp
    while (true)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg); // Effectively calls L3GWndProc() handler
        }
        if (FLAG(g_Flags, GFLAG_APPCLOSE) || FLAG(g_Flags, GFLAG_EXITLOOP))
        {
            break;
        }
        // ...
        auto cur_takt = clock::now();
        auto delta_time = (cur_takt - prev_takt);
        prev_takt = cur_takt;

        if (g_Network.game_state == GameState::LOBBY)
        {
            // ImGui Lobby rendering & Present()
        }
        else
        {
            if (FLAG(g_Flags, GFLAG_4SPEED))
            {
                delta_time *= 4;
            }

            delta_time *= g_Network.playback_speed;
            g_Network.process_network_frame(0);

            int delta = std::min(100LL, to_milliseconds(delta_time).count());

            if (g_Network.is_authority())
            {
                g_FormCur->Takt(17);
            }
            else
            {
                g_FormCur->Takt(delta);
            }

            g_FormCur->Draw();
            // ...
            g_Network.graphics_frame += 1;
        }

        next_tick += dt;
        std::this_thread::sleep_until(next_tick);
        // ...
    }
```

#### Step-by-Step Execution Order per Frame (~17ms / ~58.8 Hz):

1. **OS Message Pump (`PeekMessage` -> `L3G_WndProc` -> `processInput`)**:
    - Captures all Win32 mouse and keyboard events **before** network or simulation ticks run.
    - Dispatches directly to `g_FormCur` (`CFormMatrixGame::MouseMove`, `MouseKey`, `Keyboard`).
2. **Network Poll (`g_Network.process_network_frame(0)`)**:
    - Calls `enet_host_service(host, &event, 0)` to drain all queued packets without blocking.
    - **On Client**: Deserializes `WORLD_SNAPSHOT` (pushes into `interpolation_buffer`) and `EVENTS` (pushes `EventFire` into `events_queue`).
    - **On Server**: Deserializes incoming client command packets (`COMMAND_MOVE`, `COMMAND_BUILD`, `COMMAND_CAPTURE`) and **immediately executes them** against the server's game state.
3. **Form Update (`g_FormCur->Takt(...)` -> `CFormMatrixGame::Takt(step)`)**:
    - Server passes a fixed `step = 17` ms; Client passes variable `delta` scaled by `g_Network.playback_speed`.
    - Calls `g_MatrixMap->Takt(step)` (which resolves to `CMatrixMapLogic::Takt(step)`), followed by edge-of-screen mouse camera scrolling and keyboard camera pan/rotation.
4. **Logic & Physics Update (`CMatrixMapLogic::Takt(step)` in `MatrixGame/src/MatrixLogic.cpp`)**:
    - Updates selection timers (`CMultiSelection::AddTime`) and developer console (`m_Console.Takt`).
    - **Branch A — Server (`g_Network.is_authority() && g_Network.game_ongoing`)**:
        1. Calls `CMatrixMapLogic::physics_process(step)`:
            - Every 100ms of game time, runs `GatherInfo(0)` and `GatherInfo(1)` (robots scan their surroundings for enemies/targets).
            - Calls `CMatrixMapStatic::ProceedLogic(step)`: iterates over the intrusive doubly-linked list of logic-active statics (`m_FirstLogicTemp` -> `m_NextLogicTemp`), calling `StaticTakt(step)` -> virtual `LogicTakt(step)` on every building, cannon, flyer, and non-arcaded robot.
            - Loops over all sides (`m_Side[i].LogicTakt(step)`): updates player/AI tactical groups (`TaktPL`), turret placement ghost previews, and unit counts.
            - Ticks the player's direct-controlled ("arcaded") robot if one exists.
        2. Increments `g_Network.physics_frame`.
        3. Every 6 physics frames (`physics_frame % 6 == 0`, ~10 Hz), calls `g_Network.broadcast_world_snapshot()` and `g_Network.broadcast_events()`.
    - **Branch B — Client (`g_Network.is_client()`)**:
        - Skips `physics_process(step)` completely.
        - Calls `g_Network.process_playback(step)`: interpolates robots between the two oldest snapshots in `interpolation_buffer`, updates side resources and building capture progress/side, triggers queued `EventFire` weapon effects, and calls `StaticTakt` **only** on robots in `g_Network.robots`.
    - **Common Post-Logic (Both Server & Client)**:
        - Increments `m_Time += step`.
        - Calls `g_IFaceList->LogicTakt(step)` (UI animations, robot construction progress bars, hints).
        - Calls `CMatrixMap::Takt(step)` (**Graphical Takt**):
            - Runs `CMatrixMapStatic::SortEndGraphicTakt(step)` (calls visual `Takt(step)` on objects that were frustum-culled/sorted during the *previous* frame's draw pass).
            - Updates visible map chunks (`GraphicTakt`), skin manager, water, cursor, debug info, effect spawners, active visual effects (`m_EffectsFirst` list — projectiles, explosions, lasers, dust), minimap, and sound.
        - Calls `m_Camera.Takt(step)`.
5. **Render Pass (`g_FormCur->Draw()` -> `CFormMatrixGame::Draw()`)**:
    - **`g_MatrixMap->BeforeDraw()`**:
        - Computes camera matrices and mouse ray (`m_MouseDir`).
        - Performs a full 3D raycast (`Trace(...)`) from the camera through the cursor against `TRACE_ALL`, writing the hit coordinate to `g_MatrixMap->m_TraceStopPos` and the hit object to `g_MatrixMap->m_TraceStopObj`.
        - Frustum-culls map groups and Z-sorts visible `CMatrixMapStatic` objects into the static array `CMatrixMapStatic::objects[]` (which will be used by the *next* tick's `SortEndGraphicTakt`), and uploads/prepares dynamic D3D9 resources (`SortEndBeforeDraw`).
    - **`g_D3DD->BeginScene()` -> `g_MatrixMap->Draw()` -> `EndScene()` -> `Present()`**.

---

### 2. Entity State: Units, Positions, and Command Hierarchy

#### Inheritance & Memory Layout
Every physical entity in the world inherits from `CMatrixMapStatic` (`MatrixGame/src/MatrixMapStatic.hpp`):
- `CMatrixMapStatic` (holds `m_NID`, `m_ObjectState` bitflags, spatial grid pointers `m_InGroups[]`, and a heap-allocated `SObjectCore* m_Core` containing `m_Matrix`, `m_IMatrix`, `m_GeoCenter`, `m_Radius`, and `m_Type`).
    - `CMatrixMapObject` (static props: trees, rocks, gates, debris — *not* in the logic list unless burning/interactive)
    - `CMatrixBuilding` (bases and resource factories, holds `CBuildingStack m_BS` for production queues)
    - `CMatrixCannon` (turrets)
    - `CMatrixFlyer` (delivery/cargo helicopters)
    - `CMatrixRobot` -> `CMatrixRobotAI` (playable/AI robots)

#### Spatial Indexing & Transform Sync
- The map is divided into a fine movement grid (`m_Move`, scaled by `GLOBAL_SCALE_MOVE`) and coarse visibility/spatial chunks (`CMatrixMapGroup`, scaled by `GLOBAL_SCALE * MAP_GROUP_SIZE`).
- A robot's authoritative 2D position is stored in `CMatrixRobotAI::m_PosX` and `m_PosY` (world floats) and `m_MapX`, `m_MapY` (move-grid integers), while its 3D orientation is stored via `m_Forward` / `m_HullForward` vectors and the 4x4 transform matrix `m_Core->m_Matrix`.
- Whenever a unit moves or rotates, it marks dirty flags via `RChange(MR_Matrix | MR_Shadow...)`, recomputes its world matrix on `RNeed(MR_Matrix)`, and updates its spatial chunk membership via `JoinToGroup()`.

#### Two-Tier Command & AI Hierarchy
Commands do not go straight from mouse click to robot movement; they pass through a two-tier system:
1. **Side / Tactical Group Level (`CMatrixSideUnit` & `SMatrixPlayerGroup` in `MatrixSide.cpp`)**:
    - Each side (`m_Side[0..3]`, IDs 1–4) owns `m_PlayerGroup[MAX_LOGIC_GROUP]`.
    - When a player issues an order (`PGOrderMoveTo`, `PGOrderCapture`, `PGOrderAttack`, `PGOrderStop`, etc.), the selected robots are assigned a logic group index (`robot->SetGroupLogic(no)`), the group's order state (`mpo_MoveTo`, `mpo_Capture`, etc.) is set, and `PGAssignPlacePlayer` reserves non-overlapping tactical grid slots (`m_Place` / `m_PlaceAdd`) for each robot in the group.
    - `CMatrixSideUnit::TaktPL()` (throttled to run once every 100ms of game time on the server) evaluates each `m_PlayerGroup`, computes macro road-network paths (`m_RoadPath`), and pushes concrete low-level orders into each robot's personal order queue.
2. **Individual Robot Order Pool (`CMatrixRobotAI::m_OrdersList[MAX_ORDERS]` in `MatrixRobot.cpp`)**:
    - Each `CMatrixRobotAI` has a stack/queue of `SOrder` structs (`ROT_MOVE_TO`, `ROT_MOVE_TO_BACK`, `ROT_MOVE_RETURN`, `ROT_STOP_MOVE`, `ROT_FIRE`, `ROT_STOP_FIRE`, `ROT_CAPTURE_FACTORY`, `ROT_STOP_CAPTURE`).
    - Inside `CMatrixRobotAI::LogicTakt(int ms)`:
        - Target acquisition rotates the hull/chassis (`RotateHull`, `RotateRobot`) toward `m_Environment.m_Target`.
        - The `while (cnt < m_OrdersInPool)` loop processes `m_OrdersList[0]`.
        - For `ROT_MOVE_TO`, it runs zone pathfinding (`ZoneCurFind`, `ZonePathCalc`, `ZoneMoveCalc`) and advances the robot along its waypoint array via `MoveByMovePath(ms)` -> `LowLevelMove(...)`, which updates `m_Velocity`, `m_Speed`, `m_PosX`, `m_PosY`, and handles unit-to-unit collision avoidance.
        - After orders, it ticks all `m_Weapons[nC]` (`CMatrixEffectWeapon::Takt`), handling weapon heat/cooldowns and firing (`CMatrixEffectWeapon::Fire()`).

---

### 3. Input Handling: Where Player Inputs Are Captured Before Affecting Game State

Input flows from Win32 into `CFormMatrixGame` (`MatrixGame/src/MatrixFormGame.cpp`), which branches into three main surfaces:

1. **Win32 Entry (`MatrixLib/3G/3g.cpp`)**:
    - `L3G_WndProc` -> `processInput` translates `WM_MOUSEMOVE`, `WM_LBUTTONDOWN/UP/DBLCLK`, `WM_RBUTTONDOWN/UP/DBLCLK`, `WM_MOUSEWHEEL`, and `WM_KEYDOWN/UP` into calls on `g_FormCur` (`CFormMatrixGame`).
    - Note: `CFormMatrixGame` relies heavily on `g_MatrixMap->m_TraceStopPos` and `g_MatrixMap->m_TraceStopObj`—which were computed during the **previous frame's** `BeforeDraw()` raycast!
2. **Mouse Clicks & Selection (`CFormMatrixGame::MouseKey` -> `CMatrixSideUnit`)**:
    - **Left-Button Down (`VK_LBUTTON`, `B_DOWN`)**:
        - If `g_IFaceList->m_InFocus == INTERFACE`, routes to UI (`g_IFaceList->OnMouseLBDown()`), which triggers button callbacks like `CConstructor::RemoteBuild` (building robots) or command panel buttons.
        - If `g_IFaceList->m_InFocus == UNKNOWN` (world view), starts a drag-selection box (`CMultiSelection::Begin`) and calls `OnLButtonDown` on the side(s) (which handles turret placement confirmation and pre-ordered commands like `PREORDER_MOVE`, `PREORDER_FIRE`, `PREORDER_CAPTURE`, `PREORDER_PATROL`, `PREORDER_BOMB`, `PREORDER_REPAIR`).
    - **Left-Button Up (`VK_LBUTTON`, `B_UP`)**:
        - Finalizes the selection box via `do_selection_logic(...)`, populating the side's `m_CurSelGroup` / `m_CurGroup`.
    - **Right-Button Down (`VK_RBUTTON`, `B_DOWN`)**:
        - If in world view, calls `CMatrixSideUnit::OnRButtonDown(CPoint(x, y))` (`MatrixGame/src/MatrixSide.cpp:832`).
        - This is the primary context-sensitive RTS order dispatch:
            - Right-click enemy/neutral building -> **Capture** (`PGOrderCapture` on server, `NetOrderCapture` on client).
            - Right-click enemy unit -> **Attack** (`PGOrderAttack`).
            - Right-click terrain/water -> **Move** (`PGOrderMoveTo` on server, `NetOrderMoveTo` on client).
3. **Keyboard Hotkeys (`CFormMatrixGame::Keyboard` in `MatrixFormGame.cpp:776`)**:
    - Handles group hotkeys (`0`–`9`), direct robot control (`W`/`A`/`S`/`D`, `Space`/`Enter` to enter/exit arcade mode, `E` to self-destruct), and order hotkeys (`M`ove, `S`top, `A`ttack, `K`apture, `P`atrol, `R`epair, `E`xplode, `U`/`C`/`D` auto-orders, `B`uild robot, `T`urret menu, `H`elp/Maintenance).

---

### 4. Red Flags, Bugs, and "Stupid Things" in the Codebase

As you noted in the rules, there are several wild architectural quirks, leftover experiments, and critical bugs in both the legacy engine and the current networking layer. Here are the most important ones to be aware of:

3. **Network ID (`m_NID`) Collisions & Pollution (`MatrixMapStatic.hpp:298`)**:
   `u32 m_NID{g_Network.next_nid++};` is a member initializer on the base class `CMatrixMapStatic`.
    - Every tree, rock, ruin, smoke mesh, and temporary UI preview robot (such as `m_Robot` and `m_Build` inside `CConstructor`, or the ghost turret `m_CannonForBuild` in `CMatrixSideUnit`) increments `g_Network.next_nid`!
    - Because `MatrixMapPrepare.cpp` skips loading robots, cannons, and effects on the client (`if (g_Network.is_authority() && step == RS_ROBOTS)`), if any static object is created after those steps—or when a client opens the constructor UI and instantiates preview objects—`next_nid` will diverge if not strictly managed by the server.

5. **Unreliable / Unbounded Packet Handling & Union Memory Leak Hazard**:
    - **`Network::send_message` (`Network.cpp:62-89`)**: Passes `0` for `ENetPacketFlag` (unreliable, unsequenced) for **every** packet, including `COMMAND_MOVE`, `COMMAND_BUILD`, `COMMAND_CAPTURE`, and `GAME_SETTINGS`! If a single UDP packet drops, a player's build or move command vanishes (or in `connect_to_server`, the client hangs forever waiting for `GAME_SETTINGS`). Also, the overload `send_message(Message &msg, ENetPeer *peer)` ignores `peer` and calls `enet_host_broadcast` anyway!
    - **`Message` Union (`Message.hpp:257-322`)**: `Message` puts non-trivial types (`std::map`, `std::vector`, `std::string`) inside an unrestricted `union`, but **defines no copy constructor, move constructor, or assignment operator**. Yet `Message` is passed by value in `MessageWorldSnapshotParams(WorldSnapshot _ws)` and returned by value in `deserialize_from_bitstream`. If a `Message` is ever copied or moved outside of guaranteed RVO, it will either fail to compile or cause double-free/memory corruption.
    - **`BitReader` (`BitStream.hpp:48`)**: Takes a raw `u8* buf` with no size/capacity parameter and performs zero bounds checking (including `memcpy` on `read_string()`).
