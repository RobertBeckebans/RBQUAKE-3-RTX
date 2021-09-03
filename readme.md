## ABOUT THE PORT

`RBQUAKE-3-RTX is a personal toy engine based on Justin Marshall's D3D12 RTX porting effort.`

## CHANGES compared to vanilla Quake 3

* Port to D3D12 using tinyrenderers and Nvidia's DX12 helpers
* The renderer changes are minimum invasive
* There is no forward or deferred rendering. It's pure raytracing
* Engine compiles only native to 64 bit

## CHANGES compared to original fork

* Flexible build system using Premake 5
* Code layout reflects RBQUAKE-3 or better known as XreaL


## COMPILING ON WINDOWS

1. Download and install the Visual Studio 2017 Community Edition.
2. Generate the VS2017 projects using Premake 5 by doubleclicking a matching configuration .bat file in the premake/ folder.
3. Use the VC2017 solution to compile what you need: RBQUAKE-3-RTX/premake/RBQUAKE-3-RTX.sln

## RUN THIS THING
This build is to test performance and stability and should be considered a early release

How to play:
1) Copy the Quake 3 Arena pk3 files into baseq3
2) Double click run.bat.

For testing only use the following maps:
Maps:
q3dm0
q3dm7
q3dm11
q3tourney4

User Maps:
Varying degrees of lighting bugs may or may not occur. 

## KNOWN ISSUES

Restarting into a new level causes first person weapons to not render.

## FAQ ##

**Q**: Why bother with Quake 3 in 2021?
**A**: It is fun, period. Quake 3 was one of my favorite games of all time. It is a classic like Pacman or Chess and never gets old.

**Q** I'm seeing `VM_Create on UI failed`. Help!
**A**: This means the UI DLL didn't copy to the right deploy directory. It should be in the same directory as your exe, or in _baseq3_.

**Q**: How do I run the game in widescreen?
**A**: `r_mode -1; r_customwidth 1920; r_customheight 1080`

**Q**: How do I know what code you've changed?
**A**: Apart from the Git diffs, you can look for `// jmarshall` or  `// RB` in the source code.