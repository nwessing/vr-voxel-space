CC=clang
CFLAGS=-DINCLUDE_GLAD -I ~/headers -Wall -Wextra -pedantic -g -O2 -std=c11 -Werror=implicit-function-declaration
LIBS=-lSDL2 -ldl -framework OpenGL
DEPS = src/game.h src/shader.h src/file.h src/vr.h src/image.h src/raycasting.h src/types.h src/util.h
OBJ = src/game.o src/main.o glad/glad.o src/shader.o src/file.o src/image.c src/raycasting.c src/util.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

# ifeq ($(OS),Windows_NT)
#   COMPILE := cl /O2 /I C:\Libraries\Includes C:\Libraries\Libs\SDL2.lib C:\Libraries\Libs\SDL2main.lib C:\Libraries\Libs\LibOVR.lib src/shader.c src/main.c glad/glad.c src/vr.c /Zi /FC /link /out:vr-voxel-space.exe /SUBSYSTEM:CONSOLE /DEBUG
# else
#   COMPILE := clang -I ~/headers src/main.c glad/glad.c src/shader.c src/file.c -lSDL2 -ldl -framework OpenGL -o vr-voxel-space -Wall -Wextra -pedantic -g -O2
# endif


vr-voxel-space: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

