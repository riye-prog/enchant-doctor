#include "ui.hpp"
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <stdexcept>
#include <algorithm>

namespace {
Rml::Input::KeyIdentifier key(int code) {
    using namespace Rml::Input;
    if (code >= 4 && code <= 29) return static_cast<KeyIdentifier>(KI_A + code - 4);
    if (code >= 30 && code <= 38) return static_cast<KeyIdentifier>(KI_1 + code - 30);
    switch (code) {
        case 39: return KI_0;
        case 42: return KI_BACK;
        case 43: return KI_TAB;
        case 40: return KI_RETURN;
        case 41: return KI_ESCAPE;
        case 44: return KI_SPACE;
        case 76: return KI_DELETE;
        case 80: return KI_LEFT;
        case 79: return KI_RIGHT;
        case 82: return KI_UP;
        case 81: return KI_DOWN;
        case 74: return KI_HOME;
        case 77: return KI_END;
        default: return KI_UNKNOWN;
    }
}
int modifiers(int sdl) {
    int result = 0;
    if (sdl & 3) result |= Rml::Input::KM_SHIFT;
    if (sdl & 192) result |= Rml::Input::KM_CTRL;
    if (sdl & 768) result |= Rml::Input::KM_ALT;
    if (sdl & 3072) result |= Rml::Input::KM_META | Rml::Input::KM_CTRL;
    return result;
}
}

UiController::UiController(const std::string& assets, int width, int height, float scale) {
    context = Rml::CreateContext("doctrine", {width,height});
    if (!context) throw std::runtime_error("Unable to create RmlUi context");
    context->SetDensityIndependentPixelRatio(scale);
    document = context->LoadDocument(assets + "/workbench.rml");
    if (!document) throw std::runtime_error("Unable to load workbench.rml");
    document->AddEventListener("click", this);
    selectTab("recovery");
}
UiController::~UiController() {
    if (document) document->RemoveEventListener("click", this);
    Rml::RemoveContext("doctrine");
}
void UiController::ProcessEvent(Rml::Event& event) {
    auto* target = event.GetTargetElement();
    while (target && !target->HasAttribute("action")) target = target->GetParentNode();
    if (!target) return;
    auto action = target->GetAttribute<Rml::String>("action", "");
    if (action.starts_with("tab:")) selectTab(action.substr(4));
    else if (!action.empty() && actions.size() < 32) actions.push_back(action);
}
void UiController::selectTab(const std::string& selected) {
    if (selected != "recovery" && selected != "offers" && selected != "planner") return;
    tab = selected;
    for (const std::string name : {"recovery", "offers", "planner"}) {
        if (auto* view = element("view-" + name)) view->SetProperty("display", name == tab ? "block" : "none");
        if (auto* button = element("tab-" + name)) button->SetClass("selected", name == tab);
    }
}
void UiController::resize(int width, int height, float scale) {
    if (context->GetDimensions() != Rml::Vector2i(width,height)) context->SetDimensions({width,height});
    if (context->GetDensityIndependentPixelRatio() != scale) context->SetDensityIndependentPixelRatio(scale);
}
void UiController::show(bool show) {
    if (visible == show) return;
    visible = show;
    if (visible) document->Show(); else document->Hide();
}
void UiController::warmup() {
    show(true);
    for (const std::string name : {"recovery", "offers", "planner"}) {
        selectTab(name);
        context->Update();
        context->Render();
    }
    selectTab("recovery");
    context->Update();
    show(false);
}
void UiController::render() {
    if (!visible) return;
    context->Update();
    context->Render();
}
void UiController::input(int type, double x, double y, int code, int mods) {
    if (!visible) return;
    int flags = modifiers(mods);
    int button = code == 1 ? 0 : code == 3 ? 1 : code == 2 ? 2 : code - 1;
    switch (type) {
        case 0: context->ProcessMouseMove(static_cast<int>(x), static_cast<int>(y), flags); break;
        case 1: context->ProcessMouseButtonDown(button, flags); break;
        case 2: context->ProcessMouseButtonUp(button, flags); break;
        case 3: context->ProcessMouseWheel(static_cast<float>(-y), flags); break;
        case 4: context->ProcessKeyDown(key(code), flags); break;
        case 5: context->ProcessKeyUp(key(code), flags); break;
        case 6: context->ProcessTextInput(static_cast<Rml::Character>(code)); break;
    }
}
void UiController::setProgress(int progress) {
    progress = std::clamp(progress,0,12);
    if (progress == completed) return;
    completed = progress;
    for (int i = 0; i < 12; i++) if (auto* probe = element("probe-" + std::to_string(i))) probe->SetClass("complete", i < completed);
}
void UiController::text(const std::string& id, const std::string& value) {
    if (auto* target = element(id)) target->SetInnerRML(Rml::StringUtilities::EncodeRml(value));
}
std::string UiController::value(const std::string& id) {
    auto* target = dynamic_cast<Rml::ElementFormControl*>(element(id));
    return target ? target->GetValue() : "";
}
std::string UiController::pollAction() {
    if (actions.empty()) return {};
    auto action = std::move(actions.front());
    actions.pop_front();
    return action;
}
Rml::Element* UiController::element(const std::string& id) { return document->GetElementById(id); }
