A Frankenstein's monster of [dwm_lut](https://github.com/ledoge/dwm_lut) and [an optical flow shader](https://gist.github.com/martymcmodding/69c775f844124ec2c71c37541801c053) to hook a frame interpolator into the desktop window manager.

# Usage
To use, run DwmLutGUI.exe. This should activate the shader - there should be an fps counter visible in the bottom right of the screen. Additionally, an extra window called DummyApp should have opened. This is there to force DWM into updating every frame rather than only when a change to the screen occurs (allowing us to insert in-between frames).

To view the motion vectors calculated for the shader, hold Ctrl-Shift-Alt.

# FPS
You want the fps counter at the bottom right to be double the fps of whatever you want to be interpolated e.g. if you have a game at 60fps, you want the fps of the shader to be 120. The shader runs at your display refresh rate, so you can change the fps by changing your display refresh rate (I use [display changer](https://12noon.com/?page_id=80) to do this quickly like so: `dc.exe -refresh=%fps%` - you can even put that in a .bat file to be able to change your fps from the start menu).
