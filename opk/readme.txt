=====================
KEGS for GCW Zero
=====================

Ported by gameblabla
Beta version.

KEGS is a pretty fast Apple IIGS emulator by Goguel.
Most games work pretty well.

==============
Installation
==============

To play Apple IIGS games, you need the Apple IIGS ROM. 
(either the 128k version or the 256k one)

It can be found online on Apple's FTP.
As they don't allow redistribution of copyrighted roms, 
i can't provide it with this emulator.

THIS IS IMPORTANT !
IF YOU USE A FTP CLIENT,
MAKE SURE YOU SET IT TO BINARY MODE OR ELSE 
YOUR FILES WILL BE CORRUPTED AND YOU WONT BE ABLE TO USE KEGS !

Put the OPK file in /media/data/apps.

In /media/data/local/home, create a ".kegs" folder.
In /media/data/local/home/.kegs, put :
- All your games
- The Apple IIGS Rom renamed to ROM

The emulator "eats" text files.
These text files tells the emulator how to load the games.
This has the advantage of easily allowing multiple disk games like Rastan to be played.

Create a txt file and tell the emulator what 
files it should load.
For example for Rastan, your text file should look like this:
s7d1 = rastan1.2mg
s7d2 = rastan2.2mg

You may need to swap the order of the disks, depending on the game.
If you have an operating system installed,
nothing prevents you to put several disks 
in one configuration file.

Enjoy !

===============
CONTROLS
===============

Dpad : Arrows
Stick : Stick
A : Joystick button 1
B : Joystick button 2

Start : Return key
Select : Escape (Pause in games)

Select and Start : Exit

X : Shift
Y : Space
L shoulder : TAB


Hold Select and Press...
Up : K
Right : J
Down : I
Left : S
B : R

Hold Start and Press...
Up : 1
Right : 2
Down : 3
Left : 4
B : 5
A : 6
X : 7
Y : 8
L shoulder : 9
R shoulder : 0

There might be a better mapping soon.
With the current mapping, 
Rastan and Airball are perfectly playable.