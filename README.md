# PHAC_Frimware
 AIO Firmware ,still in progress...

## Quick Start

- Import picosdk using the method you prefer.
- Build with cmake and ninja.
- Using Pi Pico extension in VSCode might be easier.

## Customization
- Edit `main.c` to match your controller's pinout
- Current support is direct pin scan, no matrix scan, so you need to implement it yourself.
## Roadmap:
  - [x] Basic RGB support
  - [x] Rewritten encoder logic with PIO
  - [ ] Key remapping (WIP)

