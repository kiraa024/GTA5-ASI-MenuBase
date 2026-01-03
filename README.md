# GTA5-ASI-MenuBase-DX11 (Legacy)

> **IMPORTANT:** After building, rename the compiled DLL to `.asi`  
> Example: `GTA5-ASI-MenuBase.asi`  
> Place it in your GTA V root directory.

A base ASI menu for GTA V using DX11 and ImGui.  
This project is intended as a starting point for developers who want to build their own GTA V ASI menus.

The project is intentionally minimal and meant to be expanded and customized.

---

## Features

- Simple DX11 ASI menu using ImGui
- Toggle menu with **F4**
- Cleanly unloads with **END**
- Organized and easy to modify

---

## Limitations

- Player input is not fully blocked  
  - Opening the menu does not stop camera movement or key presses  
  - Contributors can implement low-level hooks or ScriptHookV natives to fix this
- DX12 / Enhanced support is not included yet

---

## Build Instructions

1. Create a **DLL project** in **Visual Studio**
2. Add `Main.cpp` as the source file
3. Add these include directories to your project:
   - `ImGui`
   - `MinHook`
   - `ScriptHookV`
4. Link the required libraries:
   - `d3d11.lib`
   - `libMinHook.x64.lib` (x64) or `libMinHook.x86.lib` (x86)
5. Build the project as a DLL
6. Rename the compiled DLL to `.asi`
7. Drop the `.asi` file into your GTA V root folder

---

## Usage

- Press **F4** to toggle the menu
- Press **END** to unload the ASI

> The menu renders and works, but player input is not blocked.  
> This repository exists as a base for others to expand and improve.

---

## Contribution

This project is a starting base. Contributions are welcome.

Examples of things that can be added:
- Input isolation (disable player movement and camera when the menu is open)
- Player, world, vehicle, or weapon features
- Teleportation or spawning features
- DX12 / Enhanced support

Fork it, modify it, and submit pull requests.

---

## License

This project is licensed under the MIT License.

You are free to use, modify, and distribute this project,  
**as long as credit is given to the original author**.
