cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE $ENV{NDK_HOME}/build/cmake/android.toolchain.cmake)
set(CMAKE_C_STANDARD 99)
set(ANDROID_TOOLCHAIN_NAME aarch64-linux-android)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-26)
set(ANDROID_NDK $ENV{NDK_HOME})

project(QuestVrVoxelSpace)
set(CMAKE_C_FLAGS "-D_BSD_SOURCE -include ${CMAKE_SOURCE_DIR}/src/main/cpp/android_fopen.h")

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp/ $ENV{OVR_HOME}/VrApi/Include ${CMAKE_SOURCE_DIR}/../vendor/cglm/include ${CMAKE_SOURCE_DIR}/../src)
add_library(main
            SHARED
						${CMAKE_SOURCE_DIR}/src/main/cpp/android_fopen.c
            ${CMAKE_SOURCE_DIR}/src/main/cpp/android_native_app_glue.c
            ${CMAKE_SOURCE_DIR}/src/main/cpp/quest_main.c
            ${CMAKE_SOURCE_DIR}/../src/file.c
            ${CMAKE_SOURCE_DIR}/../src/game.c
            ${CMAKE_SOURCE_DIR}/../src/image.c
            ${CMAKE_SOURCE_DIR}/../src/raycasting.c
            ${CMAKE_SOURCE_DIR}/../src/shader.c
            ${CMAKE_SOURCE_DIR}/../src/util.c
            )
if (MSVC)
	target_compile_options(main PRIVATE /W4 /WX /O2)
else()
	target_compile_options(main PRIVATE -Wall -Wextra -pedantic -Werror -Wno-error=format-pedantic -Wno-error=pedantic -Wno-error=unused-parameter -Wno-error=gnu-empty-initializer -Wno-error=unused-function -O2)
endif()

find_library(EGL_path EGL)
find_library(android_path android)
find_library(log_path log)
find_library(GLESv3_path GLESv3)

target_link_libraries(main EGL)
target_link_libraries(main android)
target_link_libraries(main log)
target_link_libraries(main $ENV{OVR_HOME}/VrApi/Libs/Android/arm64-v8a/Debug/libvrapi.so)
target_link_libraries(main GLESv3)

set(AAPT $ENV{ANDROID_HOME}/build-tools/28.0.3/aapt)

add_custom_command(TARGET main
	POST_BUILD

	# Build bare-minimum java code needed
	COMMAND mkdir -p com/wessing/vr_voxel_space
	COMMAND javac -classpath $ENV{ANDROID_HOME}/platforms/android-26/android.jar -d . ${CMAKE_SOURCE_DIR}/src/main/java/com/wessing/vr_voxel_space/MainActivity.java

	# Convert to Dalvik
	COMMAND $ENV{ANDROID_HOME}/build-tools/28.0.3/dx --dex --output classes.dex com/wessing/vr_voxel_space/MainActivity.class

	# Dynamic libraries libvrapi.so and libmain.so must be in
	# lib/arm64-v8, for apk bundling to work correctly
	COMMAND mkdir -p lib/arm64-v8a
  COMMAND cp $ENV{OVR_HOME}/VrApi/Libs/Android/arm64-v8a/Debug/libvrapi.so lib/arm64-v8a
	COMMAND cp libmain.so lib/arm64-v8a/libmain.so

	# Copy assets for apk bundling
	COMMAND mkdir -p assets/ assets/src assets/src/shaders assets/maps/
	COMMAND cp ${CMAKE_SOURCE_DIR}/../maps/* assets/maps/
	COMMAND cp ${CMAKE_SOURCE_DIR}/../src/shaders/* assets/src/shaders/

	# Create apk
	COMMAND ${AAPT}
		package
		-F vr-voxel-space.apk
		-I $ENV{ANDROID_HOME}/platforms/android-26/android.jar
		-M ../src/main/AndroidManifest.xml
		-f

	# Add files to apk
	COMMAND ${AAPT} add vr-voxel-space.apk classes.dex
	COMMAND ${AAPT} add vr-voxel-space.apk lib/arm64-v8a/libmain.so
	COMMAND ${AAPT} add vr-voxel-space.apk lib/arm64-v8a/libvrapi.so
	COMMAND ${AAPT} add vr-voxel-space.apk assets/*
	COMMAND ${AAPT} add vr-voxel-space.apk assets/maps/*
	COMMAND ${AAPT} add vr-voxel-space.apk assets/src/shaders/*

	# Sign the APK
	COMMAND $ENV{ANDROID_HOME}/build-tools/28.0.3/apksigner
		sign
		-ks ~/.android/debug.keystore
		--ks-key-alias androiddebugkey
		--ks-pass pass:android
		vr-voxel-space.apk
  )
