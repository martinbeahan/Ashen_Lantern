#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <cmath>
#include <string>
#include <cctype>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"
#include "GameLogic.h"

#define DUNGEON_BG 18 / 255.f, 24 / 255.f, 36 / 255.f, 1

static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;
out vec2 fragUV;
uniform mat4 uProjection;
uniform vec3 uOffset;
uniform vec3 uScale;
void main() {
    fragUV = inUV;
    gl_Position = uProjection * vec4((inPosition * uScale) + uOffset, 1.0);
}
)vertex";

static const char *fragment = R"fragment(#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
uniform vec3 uTint;
uniform float uFlash;
uniform bool uUseTexture;
out vec4 outColor;
void main() {
    vec4 texColor = uUseTexture ? texture(uTexture, fragUV) : vec4(1.0);
    vec3 flashColor = vec3(1.0, 1.0, 1.0);
    outColor = vec4(mix(texColor.rgb * uTint, flashColor, uFlash), texColor.a);
}
)fragment";

static constexpr float kProjectionHalfHeight = 2.f;
static constexpr float kProjectionNearPlane = -1.f;
static constexpr float kProjectionFarPlane = 1.f;

Renderer::Renderer(android_app *pApp, dnd::Game& game) :
        app_(pApp),
        game_(game),
        display_(EGL_NO_DISPLAY),
        surface_(EGL_NO_SURFACE),
        context_(EGL_NO_CONTEXT),
        width_(0),
        height_(0),
        totalTime_(0.0f),
        shaderNeedsNewProjectionMatrix_(true) {
    ready_ = initRenderer();
    if (!ready_) aout << "Renderer: init failed — GL layer disabled for this window" << std::endl;
}

Renderer::~Renderer() {
    // M3: GL objects (textures in models_, the shader program) must be deleted while this
    // context is still current — after eglDestroyContext their glDelete* calls are no-ops.
    if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT && surface_ != EGL_NO_SURFACE) {
        eglMakeCurrent(display_, surface_, surface_, context_);
    }
    models_.clear();
    shader_.reset();
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}


static int modelIndexForPlayer(dnd::CharacterClass c) {
    switch (c) {
        case dnd::CharacterClass::FIGHTER: return 0;
        case dnd::CharacterClass::WIZARD:  return 1;
        case dnd::CharacterClass::ROGUE:   return 2;
        case dnd::CharacterClass::CLERIC:  return 3;
        case dnd::CharacterClass::BARD:    return 1; // reuse wizard model until Bard CC0 art
    }
    return 0;
}

static int modelIndexForEnemy(const std::string& enemyId) {
    // Stable enemy ID (Bestiary.h), not the display name. GL layer only has two foe textures.
    if (enemyId.rfind("enemy_ashbound", 0) == 0 || enemyId == "boss_sexton" || enemyId == "boss_prior") return 5;
    return 4;
}

void Renderer::updateAnimations() {
    while (game_.hasVisualEvent()) {
        dnd::VisualEvent ev = game_.popVisualEvent();
        bool isPlayerSide = (ev.type == dnd::VisualEventType::PLAYER_ATTACK || ev.type == dnd::VisualEventType::PLAYER_DAMAGE);
        triggerAnim(isPlayerSide, ev.targetIndex, ev.type);
    }

    auto updateMap = [](std::map<int, AnimationState>& anims) {
        for (auto it = anims.begin(); it != anims.end(); ) {
            AnimationState& s = it->second;
            if (s.framesRemaining > 0) {
                s.framesRemaining--;
                float t = (s.framesTotal > 0)
                    ? (1.0f - (float)s.framesRemaining / (float)s.framesTotal)
                    : 1.0f;
                // Ease attack lunge back to rest
                s.xOffset *= 0.82f;
                s.yOffset *= 0.82f;
                s.scaleBoost *= 0.88f;
                if (s.flashIntensity > 0.01f) {
                    s.flashIntensity *= 0.88f;
                    // Hit shake
                    s.xOffset += static_cast<float>(dnd::rng::below(11) - 5) * 0.012f;
                }
                if (s.framesRemaining == 0) {
                    s.xOffset = 0;
                    s.yOffset = 0;
                    s.scaleBoost = 0;
                    s.flashIntensity = 0;
                }
                (void)t;
                ++it;
            } else {
                it = anims.erase(it);
            }
        }
    };
    updateMap(playerAnims_);
    updateMap(enemyAnims_);
}

void Renderer::triggerAnim(bool isPlayer, int index, dnd::VisualEventType type) {
    AnimationState& state = isPlayer ? playerAnims_[index] : enemyAnims_[index];
    if (type == dnd::VisualEventType::PLAYER_ATTACK || type == dnd::VisualEventType::ENEMY_ATTACK) {
        state.framesTotal = 18;
        state.framesRemaining = 18;
        state.xOffset = isPlayer ? 0.65f : -0.65f;
        state.yOffset = 0.12f;
        state.scaleBoost = 0.35f;
        state.flashIntensity = 0.15f;
    } else {
        state.framesTotal = 20;
        state.framesRemaining = 20;
        state.flashIntensity = 1.0f;
        state.scaleBoost = 0.2f;
        state.xOffset = isPlayer ? -0.08f : 0.08f;
    }
}

void Renderer::render() {
    if (!ready_ || !shader_) return; // H3: never draw with a failed context / null shader
    updateRenderArea();
    updateAnimations();
    totalTime_ += 0.016f;

    // L3: a 0-height surface (mid-resize) would make the aspect ratio inf/NaN — wait for a real size.
    if (shaderNeedsNewProjectionMatrix_ && width_ > 0 && height_ > 0) {
        float projectionMatrix[16] = {0};
        Utility::buildOrthographicMatrix(
                projectionMatrix,
                kProjectionHalfHeight,
                static_cast<float>(width_) / static_cast<float>(height_),
                kProjectionNearPlane,
                kProjectionFarPlane);
        shader_->setProjectionMatrix(projectionMatrix);
        shaderNeedsNewProjectionMatrix_ = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (!models_.empty()) {
        const GLint uOffset = uniforms_.offset;
        const GLint uTint = uniforms_.tint;
        const GLint uScale = uniforms_.scale;
        const GLint uFlash = uniforms_.flash;
        const GLint uUseTexture = uniforms_.useTexture;

        auto drawHealthBar = [&](float x, float y, float healthPerc) {
            glUniform1i(uUseTexture, 0);
            glUniform3f(uScale, 0.4f, 0.05f, 1.0f);

            glUniform3f(uOffset, x, y + 0.72f, 0.0f);
            glUniform3f(uTint, 0.25f, 0.05f, 0.05f);
            shader_->drawModel(models_[0]);

            glUniform3f(uScale, 0.4f * healthPerc, 0.05f, 1.0f);
            // Keep left-aligned fill as HP drops
            glUniform3f(uOffset, x - (0.4f * (1.0f - healthPerc) * 0.5f), y + 0.72f, 0.0f);
            glUniform3f(uTint, 0.2f, 1.0f, 0.2f);
            shader_->drawModel(models_[0]);

            glUniform1i(uUseTexture, 1);
            glUniform3f(uScale, 1.0f, 1.0f, 1.0f);
        };

        dnd::Character* activeActor = game_.getCurrentActor();

        const auto& players = game_.getPlayers();
        float pyStart = (players.empty() ? 0.0f : static_cast<float>(players.size() - 1) * 0.55f);
        for (size_t i = 0; i < players.size(); ++i) {
            const AnimationState& anim = playerAnims_[(int)i];
            float idleBob = 0.03f * sinf(totalTime_ * 3.0f + (float)i);
            float xPos = -1.35f + anim.xOffset;
            float yPos = pyStart - (static_cast<float>(i) * 1.05f) + anim.yOffset + idleBob;

            float pulse = 1.0f + anim.scaleBoost;
            if (activeActor == players[i].get()) {
                pulse += 0.08f + 0.08f * sinf(totalTime_ * 8.0f);
            }

            glUniform3f(uOffset, xPos, yPos, 0.0f);
            glUniform3f(uScale, pulse, pulse, 1.0f);
            glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
            glUniform1f(uFlash, anim.flashIntensity);

            int mi = modelIndexForPlayer(players[i]->characterClass);
            if (mi < 0 || mi >= (int)models_.size()) mi = (int)models_.size() - 1;
            glUniform1i(uUseTexture, models_[mi].hasTexture() ? 1 : 0);
            shader_->drawModel(models_[mi]);

            drawHealthBar(xPos, yPos, (float)players[i]->currentHp / (float)std::max(1, players[i]->maxHp));
        }

        const auto& enemies = game_.getEnemies();
        float eyStart = (enemies.empty() ? 0.0f : static_cast<float>(enemies.size() - 1) * 0.55f);
        for (size_t i = 0; i < enemies.size(); ++i) {
            const AnimationState& anim = enemyAnims_[(int)i];
            float idleBob = 0.03f * sinf(totalTime_ * 3.2f + (float)i + 1.0f);
            float xPos = 1.35f + anim.xOffset;
            float yPos = eyStart - (static_cast<float>(i) * 1.05f) + anim.yOffset + idleBob;

            float pulse = 1.0f + anim.scaleBoost;
            if (activeActor == enemies[i].get()) {
                pulse += 0.08f + 0.08f * sinf(totalTime_ * 8.0f);
            }

            glUniform3f(uOffset, xPos, yPos, 0.0f);
            glUniform3f(uScale, pulse * 0.95f, pulse * 0.95f, 1.0f);
            glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
            glUniform1f(uFlash, anim.flashIntensity);

            int mi = modelIndexForEnemy(enemies[i]->enemyId);
            if (mi < 0 || mi >= (int)models_.size()) mi = (int)models_.size() - 1;
            glUniform1i(uUseTexture, models_[mi].hasTexture() ? 1 : 0);
            shader_->drawModel(models_[mi]);

            drawHealthBar(xPos, yPos, (float)enemies[i]->currentHp / (float)std::max(1, enemies[i]->maxHp));
        }
        glUniform3f(uScale, 1.0f, 1.0f, 1.0f);
    }
}

void Renderer::present() {
    if (ready_ && display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(display_, surface_);
    }
}

bool Renderer::initRenderer() {
    // H3: every EGL call is checked; release builds compile assert() out (NDEBUG), so a missing
    // config / context / shader used to dereference end() or null and SIGSEGV at window init.
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_NONE
    };
    if (!app_ || !app_->window) {
        aout << "Renderer: no native window" << std::endl;
        return false;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
        aout << "Renderer: eglInitialize failed (0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }
    display_ = display; // from here on, failures are cleaned up by the destructor

    EGLint numConfigs = 0;
    if (eglChooseConfig(display, attribs, nullptr, 0, &numConfigs) != EGL_TRUE || numConfigs <= 0) {
        aout << "Renderer: no EGL config for ES3 window (0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }
    std::vector<EGLConfig> supportedConfigs(static_cast<size_t>(numConfigs));
    if (eglChooseConfig(display, attribs, supportedConfigs.data(), numConfigs, &numConfigs) != EGL_TRUE
        || numConfigs <= 0) {
        aout << "Renderer: eglChooseConfig failed" << std::endl;
        return false;
    }
    supportedConfigs.resize(static_cast<size_t>(numConfigs));

    // Prefer an exact RGB888 / D24 config; otherwise fall back to the first one returned
    // (configs are already filtered to >= 8 bits per channel, and the 2D renderer needs no depth).
    EGLConfig config = supportedConfigs.front();
    for (const EGLConfig& c : supportedConfigs) {
        EGLint red = 0, green = 0, blue = 0, depth = 0;
        if (eglGetConfigAttrib(display, c, EGL_RED_SIZE, &red)
            && eglGetConfigAttrib(display, c, EGL_GREEN_SIZE, &green)
            && eglGetConfigAttrib(display, c, EGL_BLUE_SIZE, &blue)
            && eglGetConfigAttrib(display, c, EGL_DEPTH_SIZE, &depth)
            && red == 8 && green == 8 && blue == 8 && depth == 24) {
            config = c;
            break;
        }
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        aout << "Renderer: eglCreateWindowSurface failed (0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }
    surface_ = surface;
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        aout << "Renderer: eglCreateContext failed (0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }
    context_ = context;

    if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
        aout << "Renderer: eglMakeCurrent failed (0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }
    width_ = -1;
    height_ = -1;

    shader_ = std::unique_ptr<Shader>(
            Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection"));
    if (!shader_) {
        aout << "Renderer: shader failed to load" << std::endl;
        return false;
    }
    shader_->activate();
    {
        const GLuint program = shader_->getProgram();
        uniforms_.offset = glGetUniformLocation(program, "uOffset");
        uniforms_.tint = glGetUniformLocation(program, "uTint");
        uniforms_.scale = glGetUniformLocation(program, "uScale");
        uniforms_.flash = glGetUniformLocation(program, "uFlash");
        uniforms_.useTexture = glGetUniformLocation(program, "uUseTexture");
    }

    glClearColor(DUNGEON_BG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    createModels();
    return true;
}

void Renderer::updateRenderArea() {
    EGLint width, height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::createModels() {
    // Slightly taller quad for character sprites
    std::vector<Vertex> vertices = {
            // Inner braces initialise the union's {x, y, z} / {u, v} struct member (L4).
            Vertex(Vector3{{0.45f, 0.55f, 0}}, Vector2{{0, 0}}),
            Vertex(Vector3{{-0.45f, 0.55f, 0}}, Vector2{{1, 0}}),
            Vertex(Vector3{{-0.45f, -0.55f, 0}}, Vector2{{1, 1}}),
            Vertex(Vector3{{0.45f, -0.55f, 0}}, Vector2{{0, 1}})
    };
    std::vector<Index> indices = {0, 1, 2, 0, 2, 3};

    // Order must match modelIndexFor*() helpers: 0 fighter, 1 wizard, 2 rogue, 3 cleric,
    // 4 wickthief, 5 ashbound. v2.24 removed the legacy CC0 textures (assets/*.png): this GL
    // layer is not used by the shipped AppCompat UI, so it draws untextured placeholder quads.
    for (int i = 0; i < 6; ++i) {
        models_.emplace_back(vertices, indices, nullptr);
    }
}


void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}
