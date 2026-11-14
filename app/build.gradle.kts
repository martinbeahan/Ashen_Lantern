import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import java.util.zip.ZipFile

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    // Enabled for online Host/Join (requires app/google-services.json locally)
    alias(libs.plugins.google.services)
}

android {
    namespace = "com.fintrack.dndbeginnerremote"
    compileSdk = 36

    // NDK r28+ aligns 64-bit .so files for 16 KB page-size devices by default
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.fintrack.dndbeginnerremote"
        minSdk = 30
        targetSdk = 36
        versionCode = 76
        versionName = "2.37"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                // 16 KB page-size support (required on newer Android / Play). The only place
                // this is configured (L7); NDK r28 already aligns 64-bit libs to 16 KB.
                arguments += listOf(
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )
            }
        }

        // Do NOT set ndk.abiFilters here — that would strip ABIs from the APK and
        // cause INSTALL_FAILED_NO_MATCHING_ABIS on some phones/emulators.
        // Default AGP packaging ships armeabi-v7a, arm64-v8a, x86, x86_64 (universal).
    }

    buildTypes {
        release {
            // R8 minify ON since v2.24 (Code Auditor L9). Keep rules for the JNI bridge
            // (MainActivity external funs ↔ Java_com_fintrack_… in main.cpp), custom views and
            // Firebase live in proguard-rules.pro. Resource shrinking stays off for now (separate
            // change; needs its own device pass).
            isMinifyEnabled = true
            isShrinkResources = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        // Prefer sharing release/signed APKs with testers — do not weaken debug
        // (android:testOnly). Studio Run/Debug APKs are test-only by design.
        debug {
            isMinifyEnabled = false
        }
    }

    // No ABI / density splits — testers get one sideloadable universal APK.
    // (Play App Bundles still optimize delivery when you upload an AAB.)
    splits {
        abi {
            isEnable = false
        }
        density {
            isEnable = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    packaging {
        jniLibs {
            // Uncompressed + page-aligned native libs (needed for 16 KB devices)
            useLegacyPackaging = false
            // Avoid duplicate native libs from Game Activity prefab
            pickFirsts += listOf("**/libc++_shared.so")
        }
    }
}

// M6: kotlinOptions {} is deprecated (an error from Kotlin 2.2); use the compilerOptions DSL.
kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.games.activity)

    // Firebase is optional until google-services.json is present
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.database)
    // v2.37 #90: anonymous sign-in before Host / Join (the database rules require auth).
    implementation(libs.firebase.auth)

    testImplementation(libs.junit)
    // v2.37: real org.json for the database-rules structure test (Android's is a stub on the JVM).
    testImplementation(libs.org.json)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}

// v2.33: the level-50 test shortcut (src/debug DebugShortcuts.kt + main.cpp #ifndef NDEBUG) must never
// ship. After the release APK is packaged, scan its dex and native libs for the shortcut's markers;
// any hit fails assembleRelease. (tools/check_release_jni.sh checks the JNI symbol too.)
val debugShortcutMarkers = listOf("JumpToRekindleReady", "ASHEN_DEBUG_ONLY_SHORTCUT", "[debug] Party set to level 50")
val verifyReleaseHasNoDebugShortcuts by tasks.registering {
    group = "verification"
    description = "Fails if the debug-only Rekindling test shortcut is present in the release APK."
    val apkDir = layout.buildDirectory.dir("outputs/apk/release")
    val markers = debugShortcutMarkers
    dependsOn("packageRelease")
    inputs.dir(apkDir)
    doLast {
        val apks = apkDir.get().asFile.listFiles { f -> f.name.endsWith(".apk") }.orEmpty()
        if (apks.isEmpty()) throw GradleException("verifyReleaseHasNoDebugShortcuts: no release APK found")
        val needles = markers.map { it.toByteArray(Charsets.UTF_8) }
        fun ByteArray.has(n: ByteArray): Boolean {
            if (n.isEmpty() || size < n.size) return false
            outer@ for (i in 0..size - n.size) {
                for (j in n.indices) if (this[i + j] != n[j]) continue@outer
                return true
            }
            return false
        }
        var scanned = 0
        for (apk in apks) {
            ZipFile(apk).use { zip ->
                for (e in zip.entries().asSequence()) {
                    val n = e.name
                    if (!(n.endsWith(".dex") || (n.startsWith("lib/") && n.endsWith(".so")))) continue
                    val bytes = zip.getInputStream(e).use { it.readBytes() }
                    scanned++
                    needles.forEachIndexed { k, needle ->
                        if (bytes.has(needle)) throw GradleException(
                            "Release APK ${apk.name} contains the debug-only shortcut marker '${markers[k]}' in $n. " +
                                "It must stay in src/debug / #ifndef NDEBUG.")
                    }
                }
            }
        }
        if (scanned == 0) throw GradleException("verifyReleaseHasNoDebugShortcuts: no dex / .so scanned")
        println("verifyReleaseHasNoDebugShortcuts OK: $scanned dex/.so files in ${apks.size} APK(s), no debug shortcut")
    }
}
tasks.matching { it.name == "assembleRelease" }.configureEach { dependsOn(verifyReleaseHasNoDebugShortcuts) }
