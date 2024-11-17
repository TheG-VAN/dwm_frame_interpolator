A program which uses the same method for hooking into the desktop window manager as [dwm_lut](https://github.com/ledoge/dwm_lut), but uses it for motion interpolation instead.

# Usage
To use, run DwmLutGUI.exe. This should activate the shader - there should be an fps counter visible in the bottom right of the screen. Additionally, an extra window called DummyApp should have opened. This is there to force DWM into updating every frame rather than only when a change to the screen occurs (allowing us to insert in-between frames).

To view the motion vectors calculated for the shader, hold Ctrl-Shift-Alt.

# FPS
The fps counter in the bottom right shows the fps to be interpolated from, this is calculated as your monitor's refresh rate divided by the fps multiplier you have chosen. You want it to be close to the fps of whatever you want to be interpolated e.g. if you have a game at 60fps, you want it to say 60. Note that it doesn't have to be exact, as long as it is pretty close, it will work fine. The shader runs at your display refresh rate, so you can change the fps by changing your display refresh rate (I use [display changer](https://12noon.com/?page_id=80) to do this quickly like so: `dc.exe -refresh=%fps%` - you can even put that in a .bat file to be able to change your fps from the start menu).

## Explanation of the FPS Multiplier config:
The number is basically the multiplier of the interpolation. So if it is set to 3, it will turn 60fps into 180fps.
In essence, you want the number to be your display refresh rate divided by the game's fps:
- 120hz display and 60fps game, set it to 2
- 120hz display and 30fps game, set it to 4
- 240hz display and 60fps game, set it to 4

Just to reiterate, you don't need the multiplier to be exact, just pretty close. So you can probably get away with keeping your refresh rate at 144hz with a 60fps game at FPS Multiplier set to 2. I would recommend just trying it out and seeing what you prefer.
