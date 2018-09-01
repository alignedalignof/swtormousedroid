SWToR Mouse Droid(SMD)
======================

Description
-----------
A 16-keybind mouselook application with UI element recognition for Star Wars: The Old Republic.

Usage
-----
Run swtormousedroid.exe. The application will detect when SWToR is running.
Click the scroll wheel to toggle mouselook(ML). A white dot(crosshair) appears in the middle of the screen when ML is active.
Press the scroll wheel in ML mode to simulate clicking. When ML is active use your mouse to turn your character and LMB/RMB/scrolling to simulate keypresses.
See cheatsheet.png for further details.

Command-Line Options
--------------------
ML mode can be customized by passing extra parameters from the command-line.
Shift + right-click on an empty space inside the SWToR Mouse Droid folder and select 'Open command window here'.
Write the executable name and the desired parameters. E.g
- swtormousedroid.exe --disable-ui-scan --delay 250 --y-offset 40

The following parameters are available:
- --disable-ui-scan, disables detection of user interface elements
- --delay, time window for pressing and double-clicking in milliseconds, default 200
- --x-offset, crosshair offset from left in percent, default 50
- --y-offset, offset from top, default 45




