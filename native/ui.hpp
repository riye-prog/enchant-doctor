#pragma once
#include <RmlUi/Core.h>
#include <deque>
#include <string>

class UiController final : public Rml::EventListener {
    Rml::Context* context{};
    Rml::ElementDocument* document{};
    std::deque<std::string> actions;
    bool visible{};
    int completed{-1};
    std::string tab{"recovery"};
    void ProcessEvent(Rml::Event& event) override;
public:
    UiController(const std::string& assets, int width, int height, float scale);
    ~UiController() override;
    void resize(int width, int height, float scale);
    void show(bool show);
    void warmup();
    void render();
    void input(int type, double x, double y, int code, int modifiers);
    void selectTab(const std::string& selected);
    void setProgress(int progress);
    void text(const std::string& id, const std::string& value);
    std::string value(const std::string& id);
    std::string pollAction();
    Rml::Element* element(const std::string& id);
};
