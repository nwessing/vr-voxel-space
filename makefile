ifeq ($(OS),Windows_NT)
	COMPILE := cl /O2 /I C:\Libraries\Includes C:\Libraries\Libs\SDL2.lib C:\Libraries\Libs\SDL2main.lib C:\Libraries\Libs\LibOVR.lib src/shader.c src/main.c glad/glad.c src/vr.c /Zi /FC /link /out:vr-voxel-space.exe /SUBSYSTEM:CONSOLE /DEBUG
else
	COMPILE := clang -I ~/headers src/main.c glad/glad.c src/shader.c src/file.c -lSDL2 -ldl -framework OpenGL -o vr-voxel-space -Wall -Wextra -pedantic -g -O2
endif

vr-voxel-space: src/main.c src/shader.c glad/glad.c src/vr.c src/file.c
	$(COMPILE)

