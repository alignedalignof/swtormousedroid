SWToR Mouse Droid(SMD)
======================

Description
-----------
A 12-keybind mouselook mode for Star Wars: The Old Republic.

Usage
-----
Run SWTORMOUSEDROID.EXE when the game has been launched. An audio cue will indicate if activation was successful.
Click the scroll wheel to toggle mouselook(ML). A white dot(crosshair) appears in the middle of the screen when ML is active.
Press the scroll wheel in ML mode to simulate clicking. When ML is active use the following combinations to simulate key presses:
- single forward scroll:	1
- fast forward scroll:		2
- single backward scroll:	3
- fast backward scroll:		4
- left-click:			5
- double left-click:		6
- press left-click:		7
- right-click:			8
- double right-click:		9
- press right-click:		0
- left+right click:		VK_OEM_MINUS(0xbd)*
- press left+right click:	VK_OEM_PLUS(0xbb)*

*May not coincide visually with + and/or -. Located right of 0-key on some keyboard layouts.
You can close SWTORMOUSEDROID.EXE by activating the window and pressing Ctrl+C or clicking the X button.


Command-Line Options
--------------------
ML mode can be customized by passing extra parameters from the command-line.
Shift + right-click on an empty space inside the SWToR Mouse Droid folder and select 'Open command window here'.
Write the executable name and the desired parameters. E.g
- SWTORMOUSEDROID.EXE --debug --no-key-hook --delay 250 --y-offset 40

The following parameters are available:
- --debug, prints information to console window, useful for locating VK_OEM_MINUS and VK_OEM_PLUS keys
- --no-key-hook, turns off NUMPAD key hooking
- --delay, time window for pressing and double-clicking in milliseconds, default 200
- --x-offset, crosshair offset from left in percent, default 50
- --y-offset, offset from top, default 45

Trivia
------
- The simulated key presses can be used with modifier keys, i.e at least 48 binds can be used with a run-of-the-mill mouse
- SMD requires administrator rights to work as swtor.exe runs at high integrity level
- PC security software may emit a warning or block SMD when trying to run it
- Did not work well with ZoneAlarm
- Did not work with Windows Fall Creators Update, but nor did SWToR or my PC
- Tested on 64-bit Windows 10
- Technically SMD is againts SWToR's TOS, practically I hope not


