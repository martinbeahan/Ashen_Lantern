# Android 16 (API 36) readiness — v2.24

Since v2.24 Ashen Lantern compiles against and targets **API 36** (Android 16) with
**AGP 8.13.2 / Gradle 8.13**, Kotlin 2.0.21, NDK 28.2.13676358, games-activity 4.4.2.
`minSdk` stays 30. AGP 9.x was deliberately not adopted (major DSL changes; separate task).

This page records each Android 15/16 behaviour change that applies when an app targets
API 35/36, and what the app does about it.

| Behaviour change (targetSdk ≥ 35/36) | Applies to us? | What we did |
|---|---|---|
| **Edge-to-edge enforced** (API 35) and the `windowOptOutEdgeToEdgeEnforcement` opt-out **removed** (API 36) | Yes — every screen draws behind the system bars | `MainActivity.hideSystemUi()` uses `WindowCompat.setDecorFitsSystemWindows(false)` + `WindowInsetsControllerCompat` (immersive, bars return on swipe). `applyEdgeToEdgeInsets()` pads `android.R.id.content` by the **display cutout** and **IME** insets so the UI never sits under a notch or the chat keyboard. The theme's `ae_void` window background fills those margins. The app never used the opt-out attribute. |
| Deprecated `View.systemUiVisibility` / `SYSTEM_UI_FLAG_*` | Yes (was used) | Replaced (Code Auditor M8). |
| **Predictive back** on by default for apps targeting 36 (`onBackPressed()` / `KEYCODE_BACK` no longer called) | No | The app has no `onBackPressed` override and no `KEYCODE_BACK` handling; system back finishes the activity as before, now with the predictive animation. If in-game back handling is added later, use `OnBackPressedDispatcher`. |
| **Large screens** (sw ≥ 600dp): `screenOrientation`, `resizeableActivity=false`, min/max aspect ratio ignored | No restrictions declared | Nothing to remove; layouts already scale (tablet art from v2.22). Worth a tablet/foldable smoke test. |
| `elegantTextHeight` ignored (always true) | Minor | Only affects tall scripts (Arabic, Thai, etc.); UI text is Latin (Cinzel / Cormorant Garamond). No change. |
| **16 KB page sizes** (Play requirement for new apps/updates targeting 35+) | Yes (native lib) | NDK r28 + `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` (now the single setting — Code Auditor L7). Release `libdndbeginnerremote.so` LOAD segments are verified 16 KB (`0x4000`) aligned for arm64-v8a and x86_64. |
| JobScheduler quota / foreground-service type changes, `ACCESS_LOCAL_NETWORK`, intent-redirection hardening, ordered-broadcast priority | No | The app has no jobs, services, broadcast receivers or LAN access (Firebase is internet). |
| Health/fitness permissions, ART internal API changes | No | Not used; JNI is plain registered-by-name natives. |

## Things to check on a real Android 16 device

1. Phone with a punch-hole camera, portrait: nothing hidden under the cutout; status bar swipe shows and auto-hides bars.
2. Online chat: keyboard opens and the input stays visible.
3. Back gesture on the main screen: predictive animation, app closes as before.
4. Tablet / foldable: layout fills the window; rotation works.
5. Minified (R8) release: solo, Continue, Host/Join, Settings — see `BUILD_HELP.md` → *Minify (R8) checklist*.
