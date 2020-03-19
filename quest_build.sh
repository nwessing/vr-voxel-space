#!/bin/bash
PATH=$PATH:$ANDROID_HOME/build-tools/28.0.3
# NOTE: replace darwin with linux on linux
PATH=$PATH:$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin

rm -rf quest/build
mkdir -p quest/build
pushd quest/build > /dev/null
javac\
	-classpath $ANDROID_HOME/platforms/android-26/android.jar\
	-d .\
	../src/main/java/com/wessing/vr_voxel_space/*.java
dx --dex --output classes.dex .
mkdir -p lib/arm64-v8a
pushd lib/arm64-v8a > /dev/null
# ls ../../../../src/
aarch64-linux-android26-clang\
    -march=armv8-a\
    -shared\
    -D_BSD_SOURCE\
    -include ../../../src/main/cpp/android_fopen.h\
    -I ../../../../src/\
    -I /Users/nickwessing/headers/\
    -I /usr/local/include/\
    -I $NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/\
    -I $OVR_HOME/VrApi/Include\
    -L $NDK_HOME/platforms/android-26/arch-arm64/usr/lib\
    -L $OVR_HOME/VrApi/Libs/Android/arm64-v8a/Debug\
    -landroid\
    -llog\
    -lvrapi\
    -o libmain.so\
    ../../../../src/file.c\
    ../../../../src/game.c\
    ../../../../src/image.c\
    ../../../../src/raycasting.c\
    ../../../../src/shader.c\
    ../../../../src/util.c\
    ../../../src/main/cpp/*.c
cp $OVR_HOME/VrApi/Libs/Android/arm64-v8a/Debug/libvrapi.so .
popd > /dev/null
pushd ../../
mkdir quest/build/assets
mkdir quest/build/assets/src
mkdir quest/build/assets/src/shaders
cp *.png quest/build/assets/
cd src/es_shaders
cp * ../../quest/build/assets/src/shaders/
popd > /dev/null
pwd
aapt\
	package\
	-F vr-voxel-space.apk\
	-I $ANDROID_HOME/platforms/android-26/android.jar\
	-M ../src/main/AndroidManifest.xml\
	-f
aapt add vr-voxel-space.apk classes.dex
aapt add vr-voxel-space.apk lib/arm64-v8a/libmain.so
aapt add vr-voxel-space.apk lib/arm64-v8a/libvrapi.so
aapt add vr-voxel-space.apk assets/*
aapt add vr-voxel-space.apk assets/src/shaders/*
apksigner\
	sign\
	-ks ~/.android/debug.keystore\
	--ks-key-alias androiddebugkey\
	--ks-pass pass:android\
	vr-voxel-space.apk
popd > /dev/null
