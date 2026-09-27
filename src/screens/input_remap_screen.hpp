#pragma once

#include <vector>

#include "screens/input_remap.hpp"
#include "screens/screen.hpp"

namespace td {

// C6 remapping screen (PRD section 7.4 "full remapping"). Entered from Select's
// options overlay. Owns the pure InputRemapModel, toggles InputManager capture
// mode to read the next raw key/button, converts the raw code to SDL's canonical
// name, and writes accepted bindings straight into the shared GameConfig that
// main persists on clean exit (the C4/C5 model) while pushing them to the live
// InputManager in the same tick. Escape / pad-Back are reserved and always mean
// "cancel capture" / "Back", so a bad remap can never soft-lock the shell.
class InputRemapScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::InputRemap; }
    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
    void exit(ScreenContext& ctx) override;

    // While capturing, Back cancels the capture instead of navigating away.
    bool handle_back(ScreenContext& ctx) override;
    [[nodiscard]] bool back_consumed() const override { return model_.capturing; }

    // Test accessors (headless).
    [[nodiscard]] const InputRemapModel& model() const { return model_; }
    [[nodiscard]] bool capturing() const { return model_.capturing; }
    [[nodiscard]] bool reset_selected() const { return reset_selected_; }

private:
    void commit(ScreenContext& ctx);
    void start_capture(ScreenContext& ctx);
    void cancel_capture(ScreenContext& ctx);

    InputRemapModel model_;
    bool reset_selected_ = false;
};

} // namespace td
