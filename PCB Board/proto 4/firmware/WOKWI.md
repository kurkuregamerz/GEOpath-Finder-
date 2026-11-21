# Wokwi simulator

This is a **simulator-only** build. It uses Wokwi's portrait ILI9341 display as a stand-in for the physical 320×480 ILI9488. The simulator firmware supplies fixed mock GPS coordinates and compass heading; the real firmware continues to use the NEO-M8N and ICM-20948.

## Run it

1. Open this `firmware` folder as the VS Code workspace (File → Open Folder). The PlatformIO IDE and Wokwi extensions should both be enabled.
2. Build first: click the PlatformIO **Build** checkmark in the status bar, or use the PlatformIO sidebar's Project Tasks → `wokwi` → General → Build. `wokwi` is the default PlatformIO environment in this project, so this creates `.pio/build/wokwi/firmware.bin` and `.elf`, which the TOML points to.
3. Then run **Wokwi: Start Simulator** from the command palette.

Wokwi only runs the compiled binary; it does not compile it. If you prefer a command line, open the **PlatformIO Core CLI** terminal from the PlatformIO sidebar/toolbar before running `pio run -e wokwi`. A regular Windows terminal may not know the `pio` command.

The `sdcard` folder contains fictional demo tiles for the simulator's fixed location. They only make the preview look like a map; replace them with converted real XYZ tiles to inspect your own offline map coverage. Wokwi does not simulate the board's exact ILI9488, GPS, or ICM-20948.
