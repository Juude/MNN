#include "model_sources.hpp"
#include "file_utils.hpp"
#include "dl_config.hpp"
//
//  tts_command_handler.cpp
//
//  Handler for 'tts' command
//

#include "handlers/tts_command_handler.hpp"
#include "cli_command_parser.hpp"
#include "cli_command_spec.hpp"
#include "user_interface.hpp"
#include "mnn_tts_sdk.hpp"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace mnncli {
using namespace mnn::downloader;

namespace {

// Accepts either a path to an existing model directory, or a model name already
// present in the mnncli cache.
bool ResolveModelDir(const std::string& target, std::string& model_dir) {
    std::error_code ec;
    std::string expanded = FileUtils::ExpandTilde(target);
    if (fs::is_directory(expanded, ec)) {
        model_dir = expanded;
    } else {
        std::string config_path = FileUtils::GetConfigPath(target);
        if (config_path.empty()) {
            return false;
        }
        model_dir = fs::path(config_path).parent_path().string();
    }
    return fs::exists(fs::path(model_dir) / "config.json", ec);
}

// The bundled mnn_tts SDK compiles the piper backend out - it assigns a null impl
// and there is no espeak-ng in the tree - so Process() would dereference null.
bool IsSupportedModelType(const std::string& model_dir, std::string& model_type) {
    try {
        std::ifstream is((fs::path(model_dir) / "config.json").string());
        model_type = nlohmann::json::parse(is).value("model_type", "");
    } catch (const std::exception&) {
        return false;
    }
    return model_type == "bertvits" || model_type == "supertonic";
}

} // namespace

std::string TtsCommandHandler::CommandName() const {
    return "tts";
}

const CommandSpec& TtsCommandHandler::GetSpec() const {
    return mnncli::GetSpec("tts");
}

int TtsCommandHandler::Handle(const ParsedCommand& cmd) {
    if (cmd.arguments.empty()) {
        return 1;
    }

    std::string model_dir;
    if (!ResolveModelDir(cmd.arguments[0], model_dir)) {
        UserInterface::ShowError("No TTS model found for '" + cmd.arguments[0] + "'",
                                 "Pass a model directory, or download one with: mnncli download supertonic-tts-mnn");
        return 1;
    }

    std::string model_type;
    if (!IsSupportedModelType(model_dir, model_type)) {
        UserInterface::ShowError("Unsupported TTS model in " + model_dir,
                                 "Supported model_type values: bertvits, supertonic");
        return 1;
    }

    const std::string text = CommandParser::GetOption(cmd, "text", "");
    if (text.empty()) {
        UserInterface::ShowError("--text is required");
        return 1;
    }
    const std::string output = CommandParser::GetOption(cmd, "output", "tts_output.wav");

    nlohmann::json params = nlohmann::json::object();
    if (CommandParser::HasOption(cmd, "speaker")) {
        params["speaker_id"] = CommandParser::GetOption(cmd, "speaker");
    }
    if (CommandParser::HasOption(cmd, "speed")) {
        params["speed"] = CommandParser::GetOption(cmd, "speed");
    }
    if (CommandParser::HasOption(cmd, "steps")) {
        params["iter_steps"] = CommandParser::GetOption(cmd, "steps");
    }
    if (CommandParser::HasOption(cmd, "precision")) {
        params["precision"] = CommandParser::GetOption(cmd, "precision");
    }

    std::cout << "TTS model: " << model_dir << " (" << model_type << ")" << std::endl;
    std::cout << "Text: " << text << std::endl;

    try {
        MNNTTSSDK sdk(model_dir, params.dump());
        auto result = sdk.Process(text);
        const int sample_rate = std::get<0>(result);
        const Audio& pcm = std::get<1>(result);
        if (pcm.empty()) {
            UserInterface::ShowError("TTS produced no audio");
            return 1;
        }
        sdk.WriteAudioToFile(pcm, output);

        std::error_code ec;
        if (!fs::exists(output, ec) || fs::file_size(output, ec) == 0) {
            UserInterface::ShowError("Failed to write " + output,
                                     "Check that the output directory exists and is writable");
            return 1;
        }
        const double seconds = sample_rate > 0 ? static_cast<double>(pcm.size()) / sample_rate : 0.0;
        UserInterface::ShowSuccess("Wrote " + output);
        std::cout << "  sample_rate: " << sample_rate << " Hz" << std::endl;
        std::cout << "  samples:     " << pcm.size() << std::endl;
        std::cout << "  duration:    " << std::fixed << std::setprecision(2) << seconds << " s" << std::endl;
    } catch (const std::exception& e) {
        UserInterface::ShowError("TTS failed: " + std::string(e.what()));
        return 1;
    }
    return 0;
}

} // namespace mnncli
