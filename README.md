# VR Voxel Space (Oculus Quest)

## About
I got the idea for this project from the following repo: https://github.com/s-macke/VoxelSpace
Instead of implementing the software rendering algorithm, I instead chose to convert the heightmap to 
a mesh, which would give me more flexibility in VR where the user can turn their head in any direction.

I was interested writing the Quest application in C with touching as little Java code as possible. I
found the following repo: https://github.com/makepad/hello_quest helpful in understanding how to setup 
the build process. I converted the build process to use CMake in order to more easily generate `compile_commands.json` 
for language server integration.

See the releases for an APK you can install onto your Oculus Quest if you want to see the project without building.

## Controls
## Left Controller
| Input         | Action                | 
| ------------- |-------------          |
| Analog Stick  | Move and strafe       |
| X             | Toggle camera height auto adjusts to terrain|
| Left Trigger  | Decrease world size |

## Right Controller
| Input         | Action                | 
| ------------- |-------------          |
| Analog Stick  | Rotate left/right     |
| A             | Move down             |
| B             | Move up               |
| Right Trigger | Increase world size|


## Building
### Dependencies
#### Oculus Mobile SDK 12.0
It may compile with newer versions of the SDK, but this is the version I use
https://developer.oculus.com/downloads/package/oculus-mobile-sdk/1.29.0/
Download the SDK and set the environment variable `OVR_HOME` to it's location

####Android SDK


