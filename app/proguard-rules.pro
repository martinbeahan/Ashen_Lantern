# Ashen Lantern — ProGuard / R8 rules
# release isMinifyEnabled is currently false; these rules are ready when minify is turned on.
# See BUILD_HELP.md ("Enabling minify safely") before flipping isMinifyEnabled.

# ---- General / Kotlin ----
-keepattributes Signature, InnerClasses, EnclosingMethod
-keepattributes RuntimeVisibleAnnotations, RuntimeVisibleParameterAnnotations
-keepattributes AnnotationDefault
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile

# Kotlin metadata & coroutines-friendly (harmless if unused)
-keep class kotlin.Metadata { *; }
-dontwarn kotlin.**
-dontwarn kotlinx.**

# ---- App package: activities, JNI bridge ----
-keep class com.fintrack.dndbeginnerremote.MainActivity { *; }
-keep class com.fintrack.dndbeginnerremote.** { *; }

# Preserve JNI entry points (external fun ↔ Java_com_fintrack_… in main.cpp)
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}
-keepclassmembers class com.fintrack.dndbeginnerremote.MainActivity {
    native <methods>;
    public <methods>;
}

# System.loadLibrary("dndbeginnerremote") — keep companion / static init reachable
-keepclassmembers class com.fintrack.dndbeginnerremote.MainActivity$Companion {
    *;
}

# ---- Game Activity / games-activity prefab (native glue + JNI init) ----
-keep class com.google.androidgamesdk.** { *; }
-keep class androidx.games.** { *; }
-keep class com.google.androidgamesdk.GameActivity { *; }
-dontwarn com.google.androidgamesdk.**
-dontwarn androidx.games.**

# Native lib name must stay loadable; do not strip JNI glue symbols via shrinker side-effects
-keepclasseswithmembernames class * {
    native <methods>;
}

# ---- Firebase Realtime Database / Google Services ----
-keep class com.google.firebase.** { *; }
-keep class com.google.android.gms.** { *; }
-dontwarn com.google.firebase.**
-dontwarn com.google.android.gms.**

# Firebase uses reflection for serializers / ValueEventListener callbacks
-keepclassmembers class * {
    @com.google.firebase.database.PropertyName *;
}
-keepclassmembers class com.fintrack.dndbeginnerremote.MultiplayerManager { *; }

# ---- AndroidX / AppCompat / Material (UI shell) ----
-keep class androidx.appcompat.** { *; }
-keep class com.google.android.material.** { *; }
-dontwarn androidx.**
-dontwarn com.google.android.material.**

# Keep Parcelable / Serializable helpers if added later
-keepclassmembers class * implements android.os.Parcelable {
    public static final ** CREATOR;
}
-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}
