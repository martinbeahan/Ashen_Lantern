# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("dndbeginnerremote")

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(dndbeginnerremote SHARED
        main.cpp
        AndroidOut.cpp
        Renderer.cpp
        Shader.cpp
        TextureAsset.cpp
        Utility.cpp
        Game.cpp
        GameSave.cpp)

# L4: keep the warning set the audit used visible in every build (warnings, not errors).
target_compile_options(dndbeginnerremote PRIVATE -Wall -Wextra)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

# 16 KB page-size ELF alignment (required for Android 15+ / Play) comes from NDK r28's default
# plus -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON in app/build.gradle.kts — set in one place only
# (Code Auditor L7).
# M7: the old "-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode" linker
# workaround is gone. games-activity 4.x registers its natives with RegisterNatives
# (GameActivity_register) instead of relying on that exported name, and the shipped app never
# launches GameActivity anyway — the only activity is the AppCompat MainActivity. If the
# GameActivity path is ever revived, add a JNI_OnLoad that calls GameActivity_register(env).

# Configure libraries CMake uses to link your target library.
target_link_libraries(dndbeginnerremote
        # The game activity
        game-activity::game-activity_static

        # EGL and other dependent libraries required for drawing
        # and interacting with Android system
        EGL
        GLESv3
        jnigraphics
        android
        log)
