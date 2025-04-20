#pragma once

#include "pch.h"

#include "widgets.h"

// Custom spdlog log sink that calls ImGui::BakeToast

class ToastSink : public spdlog::sinks::sink {
public:
	void log(const spdlog::details::log_msg& msg) override {
		if (!ImGui::GetCurrentContext()) {
			return;
		}

		std::string message = fmt::to_string(msg.payload);
		ImGui::ToastLevel level = ImGui::ToastLevel::Info;

		if (msg.level == spdlog::level::err) {
			level = ImGui::ToastLevel::Error;
		}
		else if (msg.level == spdlog::level::warn) {
			level = ImGui::ToastLevel::Warning;
		}
		else if (msg.level == spdlog::level::info) {
			level = ImGui::ToastLevel::Info;
		}
		else if (msg.level == spdlog::level::debug) {
			level = ImGui::ToastLevel::Debug;
		}

		ImGui::ToastSystem::Show(message, level);
	}

	void flush() override { }
	void set_pattern(const std::string&) override { }
	void set_formatter(std::unique_ptr<spdlog::formatter>) override { }
};

void SetupLogger() {
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto toast_sink = std::make_shared<ToastSink>();

	std::vector<spdlog::sink_ptr> sinks { console_sink, toast_sink };

	auto logger = std::make_shared<spdlog::logger>("multi", sinks.begin(), sinks.end());
#ifdef _DEBUG
	logger->set_level(spdlog::level::debug);
#else
	logger->set_level(spdlog::level::info);
#endif
	spdlog::set_default_logger(logger);
}
