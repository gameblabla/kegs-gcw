______________________________________
KEGS Apple IIGS emulator for GCW Zero
______________________________________

Ported by gameblabla
Alpha version.

KEGS is an emulator that is pretty fast and can emulate the Apple IIGS and its games pretty well.

==============
Installation
==============

To play Apple IIGS games, you need the Apple IIGS ROM. (either the 128k version or the 256k one)
It can be found online on Apple's FTP.
As they don't allow redistribution of copyrighted roms, i can't provide it with this emulator.

If you use a FTP client to transfer your files to the GCW0,
MAKE SURE YOU SET IT TO BINARY MODE OR ELSE THE APPLE IIGS WILL BE CORRUPTED 
AND YOU WONT BE ABLE TO USE KEGS !

Put the OPK file in /media/data/apps.
In /media/data/local/home, put the provided ".kegs" folder there.

In /media/data/local/home/.kegs, put your apple IIGS rom and rename it to ROM.

If you start KEGS through the user interface right now, it will boot up the ROM,
but with no games.

Games are generally coming in 2MG formats but not always.
It doesn't really matter anyways wihch format they are, as long as they are Apple IIGS games.

Put your games in /media/data/local/home/.kegs and edit the kegs_conf file with a text editor.
For example, i have 2 disks of Rastan i want to play, so kegs_conf should look like this.

s7d1 = rastan1.2mg
s7d2 = rastan2.2mg

You may need to swap the order of the disks, depending on the game.
If you have an operating system installed, nothing prevents from put several disks 
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
With the current mapping, Rastan and Airball are perfectly playable.