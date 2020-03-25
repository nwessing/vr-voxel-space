CC=clang
SHARED_CFLAGS=-Wall -Wextra -pedantic -O2 -std=c11 -Werror=implicit-function-declaration 
CFLAGS=-DINCLUDE_GLAD -Isrc -g
LIBS=-lSDL2 -ldl -framework OpenGL
ODIR=obj/sdl

NDK_CC= $(NDK_HOME)/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android26-clang
NDK_CFLAGS=-march=armv8-a -D_BSD_SOURCE -include quest/src/main/cpp/android_fopen.h -I./src/ -I./quest/src/main/cpp/ -I/usr/local/include/ -I$(NDK_HOME)/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/ -I$(OVR_HOME)/VrApi/Include 
NDK_LIBS=-shared -landroid -llog -lvrapi -L $(NDK_HOME)/platforms/android-26/arch-arm64/usr/lib -L $(OVR_HOME)/VrApi/Libs/Android/arm64-v8a/Debug
NDK_ODIR=obj/android
AAPT=$(ANDROID_HOME)/build-tools/28.0.3/aapt

SHARED_DEPS=src/game.h src/shader.h src/file.h src/image.h src/raycasting.h src/util.h src/types.h
_SHARED_OBJ=src/game.o src/shader.o src/file.o src/image.o src/raycasting.o src/util.o

_SDL_OBJ=$(_SHARED_OBJ) src/main.o src/glad/glad.o
OBJ=$(patsubst %,$(ODIR)/%,$(_SDL_OBJ))
DEPS=$(SHARED_DEPS) src/glad/glad.h 
                             
_NDK_OBJ=$(_SHARED_OBJ) quest/src/main/cpp/quest_main.o quest/src/main/cpp/android_fopen.o quest/src/main/cpp/android_native_app_glue.o
NDK_OBJ=$(patsubst %,$(NDK_ODIR)/%,$(_NDK_OBJ))
NDK_DEPS=$(SHARED_DEPS) quest/src/main/cpp/android_fopen.h quest/src/main/cpp/android_native_app_glue.h

# Build object files for current architecture
$(ODIR)/%.o: %.c $(DEPS) $(SDL_DEPS) 
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(SHARED_CFLAGS) $(CFLAGS)

# Build object files for Oculus Quest
$(NDK_ODIR)/%.o: %.c $(NDK_DEPS) $(NDK_DEPS)
	@mkdir -p $(@D)
	$(NDK_CC) -c -o $@ $< $(SHARED_CFLAGS) $(NDK_CFLAGS)

# Build executable for current architecture
vr-voxel-space: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

# Build bare-minimum java code needed
$(NDK_ODIR)/com/wessing/vr_voxel_space/MainActivity.class: quest/src/main/java/com/wessing/vr_voxel_space/MainActivity.java
	@mkdir -p $(@D)
	javac -classpath $(ANDROID_HOME)/platforms/android-26/android.jar -d $(NDK_ODIR) quest/src/main/java/com/wessing/vr_voxel_space/MainActivity.java 

# Convert our java to dalvik (for android/quest)
$(NDK_ODIR)/classes.dex: $(NDK_ODIR)/com/wessing/vr_voxel_space/MainActivity.class
	@mkdir -p $(@D)
	$(ANDROID_HOME)/build-tools/28.0.3/dx --dex --output $(NDK_ODIR)/classes.dex $(NDK_ODIR)

# Build our application as a dynamically linked library for the Oculus Quest
$(NDK_ODIR)/lib/arm64-v8a/libmain.so: $(NDK_OBJ)
	@mkdir -p $(@D)
	$(NDK_CC) -o $(NDK_ODIR)/lib/arm64-v8a/libmain.so $^ $(NDK_CFLAGS) $(NDK_LIBS) 

# Build the APK for the Oculus Quest.
# Include all native and java code
# Add assets
quest: $(NDK_ODIR)/lib/arm64-v8a/libmain.so $(NDK_ODIR)/classes.dex *.png src/es_shaders/*
	cp $(OVR_HOME)/VrApi/Libs/Android/arm64-v8a/Debug/libvrapi.so $(NDK_ODIR)/lib/arm64-v8a
	@mkdir -p $(NDK_ODIR)/assets/ $(NDK_ODIR)/assets/src $(NDK_ODIR)/assets/src/shaders
	cp *.png $(NDK_ODIR)/assets/
	cp src/es_shaders/* $(NDK_ODIR)/assets/src/shaders/
	$(AAPT)\
		package\
		-F $(NDK_ODIR)/vr-voxel-space.apk\
		-I $(ANDROID_HOME)/platforms/android-26/android.jar\
		-M quest/src/main/AndroidManifest.xml\
		-f
	cd $(NDK_ODIR) && $(AAPT) add vr-voxel-space.apk classes.dex
	cd $(NDK_ODIR) && $(AAPT) add vr-voxel-space.apk lib/arm64-v8a/libmain.so
	cd $(NDK_ODIR) && $(AAPT) add vr-voxel-space.apk lib/arm64-v8a/libvrapi.so
	cd $(NDK_ODIR) && $(AAPT) add vr-voxel-space.apk assets/*
	cd $(NDK_ODIR) && $(AAPT) add vr-voxel-space.apk assets/src/shaders/*
	$(ANDROID_HOME)/build-tools/28.0.3/apksigner\
		sign\
		-ks ~/.android/debug.keystore\
		--ks-key-alias androiddebugkey\
		--ks-pass pass:android\
		$(NDK_ODIR)/vr-voxel-space.apk    


.PHONY: clean
clean:
	rm -rf $(ODIR)
	rm -rf $(NDK_ODIR)
	rm vr-voxel-space

# TODO: Make windows build work if i ever use that again
# ifeq ($(OS),Windows_NT)
#   COMPILE := cl /O2 /I C:\Libraries\Includes C:\Libraries\Libs\SDL2.lib C:\Libraries\Libs\SDL2main.lib C:\Libraries\Libs\LibOVR.lib src/shader.c src/main.c glad/glad.c src/vr.c /Zi /FC /link /out:vr-voxel-space.exe /SUBSYSTEM:CONSOLE /DEBUG
# endif
 
