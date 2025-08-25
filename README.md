### Not supported on Windows 24H2, see https://github.com/lauralex/dwm_lut/issues/60 for more details and potential workarounds.

A program which uses the same method for hooking into the desktop window manager as [dwm_lut](https://github.com/ledoge/dwm_lut), but uses it for motion interpolation instead.

Let's say you have a game that's capped at 60fps but your monitor is 120hz. Use the DWM Frame Interpolator, and your game will now (kinda) look like 120fps (note: only works on windowed or borderless windowed, not exclusive fullscreen).

# Usage
To use, run DwmLutGUI.exe. This should activate the shader - there should be an fps counter visible in the bottom right of the screen. Additionally, an extra window called DummyApp should have opened. This is there to force DWM into updating every frame rather than only when a change to the screen occurs (allowing us to insert in-between frames).

To view the motion vectors calculated for the shader, hold Ctrl-Shift-Alt.

## FPS Counter Explanation
The number in the bottom-right behaves slightly differently depending on whether you have "Auto Calculate FPS Multiplier" enabled or not. If it is enabled, the FPS counter should tell you the base frame rate of whatever is on the desktop e.g. a game or a video. So, if you're playing a game at 60fps, the number in the bottom-right should be 60.

If you have "Auto Calculate FPS Multiplier" disabled, the FPS counter is instead your display refresh rate divided by your FPS Multiplier. This should be used as a guide to adjust your FPS Multiplier to get the number as close to the FPS of the source as possible.

# Config
Note: you have to press apply after making a config change (except for the hotkey).
## Auto Calculate FPS Multiplier
Recommended to use this. This will make it so the frame interpolator will essentially interpolate your fps from anything up to your monitor refresh rate. It works with unstable frame rates as well. Note that the interpolator is disabled if the base FPS is less than 10 (to avoid attempting interpolation on static menus etc.)

## FPS Multiplier
If you have "Auto Calculate FPS Multiplier" disabled, you can choose an FPS Multiplier yourself. This should be as close as you can get to your display refresh rate divided by the FPS of whatever you are interpolating.

## Resolution Multiplier
Make the motion estimation shader run at a higher resolution. Should result in slightly better interpolation quality but with higher GPU usage. I personally leave it at 1, but worth trying higher to see if you prefer it.

## Hotkey
Set a hotkey to toggle the frame interpolator.

## Monitor Selector
Click on a monitor to select it. Whichever monitor is highlighted in blue will have the frae interpolation applied to it. There is currently no way to have the frame interpolator run on multiple monitors at the same time.

# FAQ
## It's not working on Windows 11 24H2
Unfortunately, the 24H2 update made some changes to various display things like DWM, WDDM, DXGI, and seemingly this has made both DWM Frame Interpolator and dwm_lut stop working for some people. A potential fix is to use older GPU drivers: Nvidia 552.x or AMD 24.5.x

## Can I use this on a fullscreen game?
This is because the interpolation is done on the Desktop Window Manager (DWM). This is a Windows application which manages visual effects (like transparency) on the desktop. The frame interpolator basically hijacks this application to run an interpolation shader on the desktop. However, since the purpose of DWM is for the desktop, it is doesn't run on top of fullscreen exclusive games.
