#include <jni.h>
#include <mutex>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>
#include <unistd.h>
#include <exception>
#include <string>

#include "AndroidOut.h"
#include "Renderer.h"
#include "Game.h"
#include "SaveCodec.h"
#include <stdexcept>
#include <sstream>

// Persistent game state that survives window destruction
static dnd::Game g_Game;
static struct android_app* g_pApp = nullptr;
static std::mutex g_RendererMutex;

extern "C" {

void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            {
                aout << "APP_CMD_INIT_WINDOW" << std::endl;
                std::lock_guard<std::mutex> lock(g_RendererMutex);
                if (!pApp->userData) {
                    // Pass the persistent game reference to the renderer. H3: if EGL / shader
                    // setup fails, tear it down and leave userData null (the loop then idles).
                    auto *pRenderer = new Renderer(pApp, g_Game);
                    if (pRenderer->isReady()) {
                        pApp->userData = pRenderer;
                    } else {
                        delete pRenderer;
                        pApp->userData = nullptr;
                    }
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            {
                aout << "APP_CMD_TERM_WINDOW" << std::endl;
                std::lock_guard<std::mutex> lock(g_RendererMutex);
                if (pApp->userData) {
                    auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                    pApp->userData = nullptr;
                    delete pRenderer;
                }
                g_Game.clearVisualEvents(); // L5: nothing renders until the next INIT_WINDOW
            }
            break;
        default:
            break;
    }
}

bool motion_event_filter_func(const GameActivityMotionEvent * /*motionEvent*/) {
    return false; // Allow Android UI to handle touches
}

[[maybe_unused]] void android_main(struct android_app *pApp) {
    g_pApp = pApp;
    pApp->onAppCmd = handle_cmd;
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    do {
        bool done = false;
        while (!done) {
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                case ALOOPER_POLL_WAKE:
                    done = true;
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        Renderer* pRenderer = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_RendererMutex);
            if (pApp->userData) {
                pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->handleInput();
                pRenderer->render();
            }
        }

        if (pRenderer) {
            pRenderer->present();
        } else {
            usleep(16000);
        }

    } while (!pApp->destroyRequested);

    g_pApp = nullptr;
}

} // extern "C" (helpers below are C++ templates)

// --- JNI safety helpers (Code Auditor M4) ---
// Every exported JNI function runs inside jniGuard*: a C++ exception escaping an extern "C"
// JNI frame aborts the process, so they are caught, logged, and a safe default is returned.
// JUtf is an RAII GetStringUTFChars wrapper with the null check the raw calls were missing.
namespace {

struct JniStringError : std::runtime_error {
    JniStringError() : std::runtime_error("GetStringUTFChars returned null") {}
};

class JUtf {
public:
    JUtf(JNIEnv* env, jstring s) : env_(env), s_(s) {
        if (!s_) { p_ = ""; owned_ = false; return; } // Kotlin passes non-null; be safe anyway
        p_ = env_->GetStringUTFChars(s_, nullptr);
        if (!p_) throw JniStringError(); // OOM: a Java exception is pending
        owned_ = true;
    }
    ~JUtf() { if (owned_ && p_) env_->ReleaseStringUTFChars(s_, p_); }
    JUtf(const JUtf&) = delete;
    JUtf& operator=(const JUtf&) = delete;
    const char* c_str() const { return p_; }
    std::string str() const { return std::string(p_); }
private:
    JNIEnv* env_;
    jstring s_;
    const char* p_ = nullptr;
    bool owned_ = false;
};

void logJniFailure(const char* fn, const char* what) {
    aout << "JNI " << fn << " failed: " << what << std::endl;
}

template <typename R, typename F>
R jniGuard(const char* fn, R fallback, F&& f) noexcept {
    try { return f(); }
    catch (const std::exception& ex) { logJniFailure(fn, ex.what()); }
    catch (...) { logJniFailure(fn, "unknown exception"); }
    return fallback;
}

template <typename F>
void jniGuardVoid(const char* fn, F&& f) noexcept {
    try { f(); }
    catch (const std::exception& ex) { logJniFailure(fn, ex.what()); }
    catch (...) { logJniFailure(fn, "unknown exception"); }
}

/** jstring results fall back to "" (Kotlin declares them non-null) unless a Java exception is pending. */
template <typename F>
jstring jniGuardStr(JNIEnv* env, const char* fn, F&& f) noexcept {
    try { return f(); }
    catch (const std::exception& ex) { logJniFailure(fn, ex.what()); }
    catch (...) { logJniFailure(fn, "unknown exception"); }
    if (env->ExceptionCheck()) return nullptr;
    return env->NewStringUTF("");
}

void throwJava(JNIEnv* env, const char* cls, const std::string& msg) {
    if (env->ExceptionCheck()) return;
    jclass c = env->FindClass(cls);
    if (c) env->ThrowNew(c, msg.c_str());
}

/** H2: class ordinal from Kotlin / remote input, validated (0..4) with a Fighter fallback. */
dnd::CharacterClass classFromJint(jint v, const char* fn) {
    if (!dnd::savecodec::isValidClassId(static_cast<int>(v))) {
        aout << fn << ": class " << static_cast<int>(v) << " out of range, using Fighter" << std::endl;
        return dnd::CharacterClass::FIGHTER;
    }
    return static_cast<dnd::CharacterClass>(v);
}

} // namespace

extern "C" {

// --- JNI IMPLEMENTATION ---
// These now use g_Game directly, so they work even if the Renderer is null.

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getPlayerStatus([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getPlayerStatus", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getPartyStatus().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getDetailedSheet([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName) {
    return jniGuardStr(env, "getDetailedSheet", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        std::string nameStr(nativeName);

        for (const auto& p : g_Game.getPlayers()) {
            if (p->name == nameStr) {
                return env->NewStringUTF(p->getDetailedSheet().c_str());
            }
        }
        return env->NewStringUTF("Hero not found.");
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getJournal([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getJournal", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getJournal().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getLastEvent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getLastEvent", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getLastEvent().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getRoomDescription([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getRoomDescription", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getRoomDescription().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSpecialName([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getSpecialName", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getSpecialActionName().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getChatHistory([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getChatHistory", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getChatHistory().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSessionId([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getSessionId", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getSessionId().c_str());
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getShopManifest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getShopManifest", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getShopManifest().c_str());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isMerchantRoom([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isMerchantRoom", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return (jboolean)g_Game.isMerchantRoom();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isInCombat([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isInCombat", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.isInCombat() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isRoomCleared([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isRoomCleared", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.isRoomCleared() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_hasSearchedRoom([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("hasSearchedRoom", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.hasSearchedRoom() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_hasUsedRoomRest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("hasUsedRoomRest", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.hasUsedRoomRest() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getLongRestCooldownRooms([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getLongRestCooldownRooms", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getLongRestCooldownRooms());
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getCurrentActorResources([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getCurrentActorResources", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        dnd::Character* actor = g_Game.getCurrentActor();
        return actor ? static_cast<jint>(actor->resources) : 0;
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getCurrentActorMaxResources([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getCurrentActorMaxResources", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        dnd::Character* actor = g_Game.getCurrentActor();
        return actor ? static_cast<jint>(actor->maxResources) : 0;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doAdvanceRoom([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("doAdvanceRoom", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerAdvanceFromCleared();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doAttack([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint targetIndex) {
    jniGuardVoid("doAttack", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerAttack(targetIndex);
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doHeal([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint targetIndex) {
    jniGuardVoid("doHeal", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerHeal(targetIndex);
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSpecial([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint targetIndex) {
    jniGuardVoid("doSpecial", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerSpecialAction(targetIndex);
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doInteract([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName) {
    jniGuardVoid("doInteract", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        g_Game.playerInteract(nativeName);
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doRest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("doRest", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerRest();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doLongRest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("doLongRest", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.playerLongRest();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUseInventoryPotion([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint invIndex) {
    return jniGuard("doUseInventoryPotion", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.useInventoryPotion(nativeName ? nativeName : "", static_cast<int>(invIndex));
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doIncreaseStat([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint statIndex) {
    jniGuardVoid("doIncreaseStat", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        g_Game.playerIncreaseStat(nativeName, statIndex);
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doBuyItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint itemIndex) {
    return jniGuard("doBuyItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.buyItem(nativeName ? nativeName : "", itemIndex);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getInventoryManifest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName) {
    return jniGuardStr(env, "getInventoryManifest", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        std::string out = g_Game.getInventoryManifest(nativeName ? nativeName : "");
        return env->NewStringUTF(out.c_str());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doEquipItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint invIndex) {
    return jniGuard("doEquipItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.equipInventoryItem(nativeName ? nativeName : "", invIndex);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUnequipItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint slot) {
    return jniGuard("doUnequipItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.unequipSlot(nativeName ? nativeName : "", slot);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUpgradeInventoryItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint invIndex) {
    return jniGuard("doUpgradeInventoryItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.upgradeInventoryItem(nativeName ? nativeName : "", invIndex);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUpgradeEquippedItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint slot) {
    return jniGuard("doUpgradeEquippedItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.upgradeEquippedItem(nativeName ? nativeName : "", slot);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSellInventoryItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint invIndex) {
    return jniGuard("doSellInventoryItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.sellInventoryItem(nativeName ? nativeName : "", invIndex);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSellEquippedItem([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint slot) {
    return jniGuard("doSellEquippedItem", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.sellEquippedItem(nativeName ? nativeName : "", slot);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doTransferInventoryItemToAlly([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint invIndex) {
    return jniGuard("doTransferInventoryItemToAlly", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.transferInventoryItemToAlly(nativeName ? nativeName : "", invIndex);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doTransferEquippedItemToAlly([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName, jint slot) {
    return jniGuard("doTransferEquippedItemToAlly", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        bool ok = g_Game.transferEquippedItemToAlly(nativeName ? nativeName : "", slot);
        return ok ? JNI_TRUE : JNI_FALSE;
    });
}


JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setHost([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jboolean isHost) {
    jniGuardVoid("setHost", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.setAsHost(isHost);
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_processGameTurn([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("processGameTurn", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.processTurn();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_resetGame([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint characterClass, jstring playerName, jint soloPlayMode, jint difficulty, jint companionClass, jboolean companionAutoAi) {
    jniGuardVoid("resetGame", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        // C1: sanitize the hero name at entry; H2: validate both class ordinals.
        g_Game.startNewGame(classFromJint(characterClass, "resetGame"),
                            dnd::savecodec::sanitizeName(nativeName, "Hero"),
                            static_cast<int>(soloPlayMode), static_cast<int>(difficulty),
                            classFromJint(companionClass, "resetGame(companion)"),
                            companionAutoAi == JNI_TRUE);
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getCompanionName([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getCompanionName", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getCompanionName().c_str());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isCompanionAutoAi([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isCompanionAutoAi", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.isCompanionAutoAi() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setCompanionAutoAi([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jboolean autoAi) {
    jniGuardVoid("setCompanionAutoAi", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.setCompanionAutoAi(autoAi == JNI_TRUE);
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setCompanionClass([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint characterClass) {
    return jniGuard("setCompanionClass", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        if (!dnd::savecodec::isValidClassId(static_cast<int>(characterClass))) return static_cast<jboolean>(JNI_FALSE);
        return g_Game.setCompanionClass(static_cast<dnd::CharacterClass>(characterClass)) ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setDifficulty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint difficulty) {
    jniGuardVoid("setDifficulty", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.setDifficulty(static_cast<int>(difficulty));
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getDifficulty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getDifficulty", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getDifficulty());
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSoloPlayMode([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getSoloPlayMode", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getSoloPlayMode());
    });
}

/** Current SoloQuestBeat (QuestScript.h); 0 for crawl / online. Drives the stage background. */
JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getQuestBeat([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getQuestBeat", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getQuestBeat());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isStoryFullyComplete([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isStoryFullyComplete", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.isStoryFullyComplete() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_consumePendingRaidKeyDrop([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("consumePendingRaidKeyDrop", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.consumePendingRaidKeyDrop() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_beginBossRaidFromCurrent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("beginBossRaidFromCurrent", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.beginBossRaidFromCurrent() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_finishBossRaidKeepParty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("finishBossRaidKeepParty", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.finishBossRaidKeepParty();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_beginChallengeDungeonFromCurrent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint legendaryChancePercent) {
    return jniGuard("beginChallengeDungeonFromCurrent", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.beginChallengeDungeonFromCurrent(static_cast<int>(legendaryChancePercent)) ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_finishChallengeDungeonKeepParty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("finishChallengeDungeonKeepParty", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.finishChallengeDungeonKeepParty();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_consumePendingChallengeLegendaryFound([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("consumePendingChallengeLegendaryFound", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.consumePendingChallengeLegendaryFound() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getChallengeLegendaryChance([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getChallengeLegendaryChance", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getChallengeLegendaryChance());
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setChallengeLegendaryChance([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint percent) {
    jniGuardVoid("setChallengeLegendaryChance", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.setChallengeLegendaryChance(static_cast<int>(percent));
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_beginArenaFromCurrent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("beginArenaFromCurrent", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.beginArenaFromCurrent() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_finishArenaKeepParty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("finishArenaKeepParty", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.finishArenaKeepParty();
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getArenaWave([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getArenaWave", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getArenaWave());
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getArenaScore([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getArenaScore", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getArenaScore());
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getArenaWavesCleared([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getArenaWavesCleared", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getArenaWavesCleared());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_beginEndlessDeepFromCurrent([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("beginEndlessDeepFromCurrent", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.beginEndlessDeepFromCurrent() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_finishEndlessDeepKeepParty([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("finishEndlessDeepKeepParty", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.finishEndlessDeepKeepParty();
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getEndlessDepth([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getEndlessDepth", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getEndlessDepth());
    });
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getEndlessBestDepthThisRun([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("getEndlessBestDepthThisRun", static_cast<jint>(0), [&]() -> jint {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return static_cast<jint>(g_Game.getEndlessBestDepthThisRun());
    });
}



JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_recoverFromPartyWipe([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("recoverFromPartyWipe", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.recoverFromPartyWipe() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_addRemoteAlly([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint characterClass, jstring playerName) {
    jniGuardVoid("addRemoteAlly", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        // Remote join (Firebase lobby / join action): sanitize the name, validate the class.
        g_Game.addAlly(dnd::savecodec::sanitizeName(nativeName, "Hero"),
                       classFromJint(characterClass, "addRemoteAlly"));
    });
}


JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_startDmSession([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring dmName) {
    jniGuardVoid("startDmSession", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, dmName);
        const char *nativeName = nativeNameUtf.c_str();
        g_Game.startDmSession(dnd::savecodec::sanitizeName(nativeName, "The Lanternkeeper"));
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmBeginDungeon([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("dmBeginDungeon", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.dmBeginDungeon();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmAdvanceRoom([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("dmAdvanceRoom", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.dmAdvanceRoom();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmNarrate([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring line) {
    jniGuardVoid("dmNarrate", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeLineUtf(env, line);
        const char *nativeLine = nativeLineUtf.c_str();
        g_Game.dmNarrate(nativeLine ? nativeLine : "");
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmGrantShortRest([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("dmGrantShortRest", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.dmGrantShortRest();
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_prepareClientJoin([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    jniGuardVoid("prepareClientJoin", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        g_Game.prepareClientJoin();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isDmTable([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuard("isDmTable", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.isDmOnlyTable() ? JNI_TRUE : JNI_FALSE;
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_saveGameState([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "saveGameState", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.serialize().c_str());
    });
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_loadGameState([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring data) {
    // H1: deserialize() is transactional; on failure the current game is untouched and an
    // IllegalStateException reaches Kotlin so its discard-broken-save / remote-apply handlers run.
    std::string error;
    bool ok = jniGuard("loadGameState", false, [&]() -> bool {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeDataUtf(env, data);
        return g_Game.deserialize(nativeDataUtf.str(), &error);
    });
    if (!ok) {
        if (error.empty()) error = "native error";
        aout << "loadGameState failed: " << error << std::endl;
        throwJava(env, "java/lang/IllegalStateException", "Save could not be loaded: " + error);
    }
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_sendChatMessage([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring sender, jstring message) {
    jniGuardVoid("sendChatMessage", [&]() {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeSenderUtf(env, sender);
        const char *nativeSender = nativeSenderUtf.c_str();
        JUtf nativeMsgUtf(env, message);
        const char *nativeMsg = nativeMsgUtf.c_str();
        g_Game.addChatMessage(nativeSender, nativeMsg);
    });
}


JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getBattleRoster([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getBattleRoster", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getBattleRoster().c_str());
    });
}


JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getXpProgress([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jstring playerName) {
    return jniGuardStr(env, "getXpProgress", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        JUtf nativeNameUtf(env, playerName);
        const char *nativeName = nativeNameUtf.c_str();
        std::string nameStr(nativeName ? nativeName : "");

        for (const auto& p : g_Game.getPlayers()) {
            if (p && p->name == nameStr) {
                // xp,threshold,level,pending
                std::string out = std::to_string(p->xp) + "," +
                    std::to_string(p->xpToNextLevel()) + "," +
                    std::to_string(p->level) + "," +
                    std::to_string(p->pendingStatPoints);
                return env->NewStringUTF(out.c_str());
            }
        }
        return env->NewStringUTF("");
    });
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getPartyXpProgress([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getPartyXpProgress", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        std::stringstream ss;
        bool first = true;
        for (const auto& p : g_Game.getPlayers()) {
            if (!p) continue;
            if (!first) ss << ";";
            first = false;
            // name|xp|threshold|level|pending
            ss << dnd::savecodec::listSafe(p->name) << "|" << p->xp << "|" << p->xpToNextLevel()
               << "|" << p->level << "|" << p->pendingStatPoints;
        }
        return env->NewStringUTF(ss.str().c_str());
    });
}


JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getDailyQuestCounters([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz) {
    return jniGuardStr(env, "getDailyQuestCounters", [&]() -> jstring {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return env->NewStringUTF(g_Game.getDailyQuestCounters().c_str());
    });
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_grantDailyQuestSpoils([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jobject thiz, jint gold, jboolean giveLoot) {
    return jniGuard("grantDailyQuestSpoils", static_cast<jboolean>(JNI_FALSE), [&]() -> jboolean {
        std::lock_guard<std::mutex> lock(g_RendererMutex);
        return g_Game.grantDailyQuestSpoils(static_cast<int>(gold), giveLoot == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
    });
}

} // extern "C"
