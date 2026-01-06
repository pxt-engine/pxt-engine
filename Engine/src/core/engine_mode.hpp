#pragma once

namespace pxt::core {
    enum class EngineMode {
        EDIT = 0,
        PLAY,
        RUNTIME
    };

    inline std::string engineModeToString(EngineMode engineMode) {
        switch (engineMode) { 
        case (EngineMode::EDIT):
            return "EDIT";
        case (EngineMode::PLAY):
            return "PLAY";
        case (EngineMode::RUNTIME):
            return "RUNTIME";
        default:
            return "UNDEFINED_ENGINE_MODE";
        }
    }
}