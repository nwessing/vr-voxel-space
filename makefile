ifeq ($(OS),Windows_NT)
	COMPILE := cl /O2 /I C:\Libraries\Includes C:\Libraries\Libs\SDL2.lib C:\Libraries\Libs\SDL2main.lib C:\Libraries\Libs\LibOVR.lib src/main.c src/glad.c /link /SUBSYSTEM:CONSOLE
else
	COMPILE := clang -I ~/headers src/main.c -lSDL2 -o vr-voxel-space -Wall -Wextra -pedantic -g -O2
endif


vr-voxel-space: src/main.c
	$(COMPILE)

