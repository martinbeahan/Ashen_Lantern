plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    // Enabled for online Host/Join (requires app/google-services.json locally)
    alias(libs.plugins.google.services)
}

android {
    namespace = "com.fintrack.dndbeginnerremote"
    compileSdk = 35

    // NDK r28+ aligns 64-bit .so files for 16 KB page-size devices by default
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.fintrack.dndbeginnerremote"
        minSdk = 30
        targetSdk = 35
        versionCode = 57
        versionName = "2.18"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                // 16 KB page-size support (required on newer Android / Play)
                arguments += listOf(
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )
                // Belt-and-suspenders linker flags for older cached toolchains
                arguments += listOf(
                    "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"
                )
            }
        }

        // Do NOT set ndk.abiFilters here — that would strip ABIs from the APK and
        // cause INSTALL_FAILED_NO_MATCHING_ABIS on some phones/emulators.
        // Default AGP packaging ships armeabi-v7a, arm64-v8a, x86, x86_64 (universal).
    }

    buildTypes {
        release {
            // Minify stays OFF until keep rules are validated on a real device.
            // ProGuard/R8 rules in proguard-rules.pro are ready; see BUILD_HELP.md.
            isMinifyEnabled = false
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
    kotlinOptions {
        jvmTarget = "17"
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

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.games.activity)

    // Firebase is optional until google-services.json is present
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.database)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
