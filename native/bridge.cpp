#include "renderer.hpp"
#include "ui.hpp"
#include <jni.h>
#include <chrono>
#include <memory>
#include <cstdio>

namespace {
class System final : public Rml::SystemInterface {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
public:
    Rml::String clipboard;
    bool clipboardChanged{};
    double GetElapsedTime() override { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); }
    bool LogMessage(Rml::Log::Type, const Rml::String& message) override {
        std::fprintf(stderr, "[Doctrine/RmlUi] %s\n", message.c_str());
        return true;
    }
    void SetClipboardText(const Rml::String& text) override { clipboard = text; clipboardChanged = true; }
    void GetClipboardText(Rml::String& text) override { text = clipboard; }
};
System systemInterface;
std::unique_ptr<Renderer> renderer;
std::unique_ptr<UiController> ui;
std::string string(JNIEnv* env, jstring value) {
    if (!value) return {};
    auto* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}
void error(JNIEnv* env, const std::exception& exception) { env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), exception.what()); }
}

extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_initialize(JNIEnv* env, jclass, jlong instance, jlong physical, jlong device, jlong loader, jint format, jstring directory, jint width, jint height, jfloat scale) {
    try {
        if (renderer) return;
        volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(loader));
        volkLoadInstance(reinterpret_cast<VkInstance>(instance));
        volkLoadDevice(reinterpret_cast<VkDevice>(device));
        auto assets = string(env, directory);
        renderer = std::make_unique<Renderer>(reinterpret_cast<VkDevice>(device), reinterpret_cast<VkPhysicalDevice>(physical), static_cast<VkFormat>(format), assets);
        Rml::SetSystemInterface(&systemInterface);
        Rml::SetRenderInterface(renderer.get());
        if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");
        if (!Rml::LoadFontFace(assets + "/PlexSans.ttf")) throw std::runtime_error("Unable to load Plex Sans");
        renderer->begin(width,height);
        ui = std::make_unique<UiController>(assets,width,height,scale);
        ui->warmup();
        std::fprintf(stderr, "[Doctrine/RmlUi] Workbench prewarmed during startup.\n");
    } catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_renderFrame(JNIEnv* env, jclass, jlong command, jlong image, jint width, jint height, jboolean show, jfloat scale) {
    try {
        if (!renderer || width <= 0 || height <= 0) return;
        renderer->begin(width,height);
        ui->resize(width,height,scale);
        ui->show(show);
        ui->render();
        renderer->record(reinterpret_cast<VkCommandBuffer>(command), reinterpret_cast<VkImage>(image));
    } catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_resize(JNIEnv* env, jclass) {
    try { if (renderer) renderer->clearTargets(); }
    catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_shutdown(JNIEnv* env, jclass) {
    try {
        if (!renderer) return;
        ui.reset();
        Rml::Shutdown();
        renderer.reset();
    } catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_text(JNIEnv* env, jclass, jstring id, jstring value) {
    try { if (ui) ui->text(string(env,id),string(env,value)); }
    catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_doctrine_client_NativeUi_value(JNIEnv* env, jclass, jstring id) {
    return env->NewStringUTF(ui ? ui->value(string(env,id)).c_str() : "");
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_doctrine_client_NativeUi_pollAction(JNIEnv* env, jclass) {
    auto action = ui ? ui->pollAction() : "";
    return action.empty() ? nullptr : env->NewStringUTF(action.c_str());
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_dispatchInput(JNIEnv* env, jclass, jint type, jdouble x, jdouble y, jint code, jint mods) {
    try { if (ui) ui->input(type,x,y,code,mods); }
    catch (const std::exception& exception) { error(env, exception); }
}
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_selectTab(JNIEnv* env, jclass, jstring tab) { if (ui) ui->selectTab(string(env,tab)); }
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_progress(JNIEnv*, jclass, jint completed) { if (ui) ui->setProgress(completed); }
extern "C" JNIEXPORT void JNICALL Java_dev_doctrine_client_NativeUi_setClipboard(JNIEnv* env, jclass, jstring value) {
    systemInterface.clipboard = string(env,value);
    systemInterface.clipboardChanged = false;
}
extern "C" JNIEXPORT jstring JNICALL Java_dev_doctrine_client_NativeUi_takeClipboard(JNIEnv* env, jclass) {
    if (!systemInterface.clipboardChanged) return nullptr;
    systemInterface.clipboardChanged = false;
    return env->NewStringUTF(systemInterface.clipboard.c_str());
}
