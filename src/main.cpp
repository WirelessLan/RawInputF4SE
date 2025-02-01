#include <Windows.h>
#include <xbyak/xbyak.h>

namespace Settings {
	namespace Names {
		constexpr const char* IronSightsFOVRotateMult = "fIronSightsFOVRotateMult";

		namespace Game {
			constexpr const char* MouseHeadingXScale = "fMouseHeadingXScale:Controls";
			constexpr const char* MouseHeadingYScale = "fMouseHeadingYScale:Controls";
			constexpr const char* MouseHeadingNormalizeMax = "fMouseHeadingNormalizeMax:Controls";
			constexpr const char* PitchSpeedRatio = "fPitchSpeedRatio:Controls";
			constexpr const char* IronSightsPitchSpeedRatio = "fIronSightsPitchSpeedRatio:Controls";
		}

		namespace Plugin {
			constexpr const char* Section = "Settings";
			constexpr const char* UseScopeFOVRotateMult = "bUseScopeFOVRotateMult";
			constexpr const char* ScopeFOVRotateMult = "fScopeFOVRotateMult";
			constexpr const char* MouseHeadingXScale = "fMouseHeadingXScale";
			constexpr const char* MouseHeadingYScale = "fMouseHeadingYScale";
			constexpr const char* MouseHeadingNormalizeMax = "fMouseHeadingNormalizeMax";
			constexpr const char* PitchSpeedRatio = "fPitchSpeedRatio";
			constexpr const char* IronSightsPitchSpeedRatio = "fIronSightsPitchSpeedRatio";
		}
	}

	namespace DefaultValues {
		constexpr float IronSightsFOVRotateMult = 1.0f;
	}

	RE::Setting* IronSightsFOVRotateMultSetting = nullptr;
	RE::Setting* MouseHeadingXScaleSetting = nullptr;
	RE::Setting* MouseHeadingYScaleSetting = nullptr;
	RE::Setting* MouseHeadingNormalizeMaxSetting = nullptr;
	RE::Setting* PitchSpeedRatioSetting = nullptr;
	RE::Setting* IronSightsPitchSpeedRatioSetting = nullptr;

	RE::BGSKeyword* HasScopeKeyword = nullptr;

	bool UseScopeFOVRotateMult = false;
	float ScopeFOVRotateMult = 0.0f;
}

bool IsFirstPerson() {
	RE::PlayerCamera* pCam = RE::PlayerCamera::GetSingleton();
	if (!pCam) {
		return false;
	}

	return pCam->currentState == pCam->cameraStates[RE::CameraState::kFirstPerson];
}

float FilterControllerOutput_Hook() {
	float ironSightsFOVRotateMult = Settings::IronSightsFOVRotateMultSetting ? Settings::IronSightsFOVRotateMultSetting->GetFloat() : Settings::DefaultValues::IronSightsFOVRotateMult;

	if (!Settings::UseScopeFOVRotateMult) {
		return ironSightsFOVRotateMult;
	}

	RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
	if (player && player->currentProcess && player->currentProcess->middleHigh && IsFirstPerson()) {
		for (auto& it : player->currentProcess->middleHigh->equippedItems) {
			if (!it.item.instanceData || it.equipIndex.index != 0) {
				continue;
			}

			auto instanceData = RE::fallout_cast<RE::TESObjectWEAP::InstanceData*, RE::TBO_InstanceData>(it.item.instanceData.get());
			if (!instanceData) {
				continue;
			}

			if (Settings::HasScopeKeyword && ((instanceData->flags & 0x00200000) || instanceData->keywords->HasKeyword(Settings::HasScopeKeyword))) {
				return Settings::ScopeFOVRotateMult != 0.0f ? Settings::ScopeFOVRotateMult : ironSightsFOVRotateMult;
			}
		}
	}

	return ironSightsFOVRotateMult;
}

void InstallHook() {
	struct asm_code : Xbyak::CodeGenerator {
		asm_code(std::uintptr_t a_srcAddr) {
			Xbyak::Label funcLabel, retLabel;

			sub(rsp, 0x10);
			movaps(ptr[rsp], xmm0);
			push(rax);
			sub(rsp, 0x18);

			call(ptr[rip + funcLabel]);
			movss(xmm1, xmm0);

			add(rsp, 0x18);
			pop(rax);
			movaps(xmm0, ptr[rsp]);
			add(rsp, 0x10);

			jmp(ptr[rip + retLabel]);

			L(funcLabel);
			dq((std::uintptr_t)FilterControllerOutput_Hook);

			L(retLabel);
			dq(a_srcAddr + 0x08);
		}
	};

	REL::Relocation<std::uintptr_t> target(REL::Offset(0xEC1792));
	asm_code p{ target.address() };
	auto& trampoline = F4SE::GetTrampoline();
	void* codeBuf = trampoline.allocate(p);
	trampoline.write_branch<6>(target.address(), codeBuf);
}

void InitSettings() {
	for (const auto& it : RE::INISettingCollection::GetSingleton()->settings) {
		if (!Settings::IronSightsFOVRotateMultSetting && it->GetKey() == Settings::Names::IronSightsFOVRotateMult) {
			Settings::IronSightsFOVRotateMultSetting = it;
		}
		if (!Settings::MouseHeadingXScaleSetting && it->GetKey() == Settings::Names::Game::MouseHeadingXScale) {
			Settings::MouseHeadingXScaleSetting = it;
		}
		if (!Settings::MouseHeadingYScaleSetting && it->GetKey() == Settings::Names::Game::MouseHeadingYScale) {
			Settings::MouseHeadingYScaleSetting = it;
		}
		if (!Settings::MouseHeadingNormalizeMaxSetting && it->GetKey() == Settings::Names::Game::MouseHeadingNormalizeMax) {
			Settings::MouseHeadingNormalizeMaxSetting = it;
		}
		if (!Settings::PitchSpeedRatioSetting && it->GetKey() == Settings::Names::Game::PitchSpeedRatio) {
			Settings::PitchSpeedRatioSetting = it;
		}
		if (!Settings::IronSightsPitchSpeedRatioSetting && it->GetKey() == Settings::Names::Game::IronSightsPitchSpeedRatio) {
			Settings::IronSightsPitchSpeedRatioSetting = it;
		}
	}

	auto hasScopeForm = RE::TESForm::GetFormByID(0x0009F425);
	if (hasScopeForm) {
		Settings::HasScopeKeyword = hasScopeForm->As<RE::BGSKeyword>();
	}

	if (!Settings::IronSightsFOVRotateMultSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::IronSightsFOVRotateMult);
	}
	if (!Settings::MouseHeadingXScaleSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::Game::MouseHeadingXScale);
	}
	if (!Settings::MouseHeadingYScaleSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::Game::MouseHeadingYScale);
	}
	if (!Settings::MouseHeadingNormalizeMaxSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::Game::MouseHeadingNormalizeMax);
	}
	if (!Settings::PitchSpeedRatioSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::Game::PitchSpeedRatio);
	}
	if (!Settings::IronSightsPitchSpeedRatioSetting) {
		logger::error("Unable to find settings: {}", Settings::Names::Game::IronSightsPitchSpeedRatio);
	}

	if (!Settings::HasScopeKeyword) {
		logger::error("Unable to find HasScope Keyword (0009F425)");
	}
}

std::string GetINIValue(const char* section, const char* key) {
	static const std::string configPath = std::string("Data\\MCM\\Settings\\").append(Version::PROJECT).append(".ini");
	char result[256]{};
	GetPrivateProfileStringA(section, key, "", result, sizeof(result), configPath.c_str());
	return result;
}

void ReadINI() {
	std::string value;

	if (Settings::IronSightsFOVRotateMultSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::IronSightsFOVRotateMult);
		if (!value.empty()) {
			try {
				Settings::IronSightsFOVRotateMultSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::IronSightsFOVRotateMult, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::IronSightsFOVRotateMult, Settings::IronSightsFOVRotateMultSetting->GetFloat());
	}

	value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::UseScopeFOVRotateMult);
	if (!value.empty()) {
		try {
			Settings::UseScopeFOVRotateMult = static_cast<bool>(std::stoul(value));
		} catch (const std::exception& e) {
			logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::UseScopeFOVRotateMult, value, e.what());
		}
	}
	logger::info("{}: {}", Settings::Names::Plugin::UseScopeFOVRotateMult, Settings::UseScopeFOVRotateMult);

	value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::ScopeFOVRotateMult);
	if (!value.empty()) {
		try {
			Settings::ScopeFOVRotateMult = std::stof(value);
		} catch (const std::exception& e) {
			logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::ScopeFOVRotateMult, value, e.what());
		}
	}
	logger::info("{}: {}", Settings::Names::Plugin::ScopeFOVRotateMult, Settings::ScopeFOVRotateMult);

	if (Settings::MouseHeadingXScaleSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::MouseHeadingXScale);
		if (!value.empty()) {
			try {
				Settings::MouseHeadingXScaleSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::MouseHeadingXScale, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::Plugin::MouseHeadingXScale, Settings::MouseHeadingXScaleSetting->GetFloat());
	}

	if (Settings::MouseHeadingYScaleSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::MouseHeadingYScale);
		if (!value.empty()) {
			try {
				Settings::MouseHeadingYScaleSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::MouseHeadingYScale, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::Plugin::MouseHeadingYScale, Settings::MouseHeadingYScaleSetting->GetFloat());
	}
	
	if (Settings::MouseHeadingNormalizeMaxSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::MouseHeadingNormalizeMax);
		if (!value.empty()) {
			try {
				Settings::MouseHeadingNormalizeMaxSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::MouseHeadingNormalizeMax, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::Plugin::MouseHeadingNormalizeMax, Settings::MouseHeadingNormalizeMaxSetting->GetFloat());
	}

	if (Settings::PitchSpeedRatioSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::PitchSpeedRatio);
		if (!value.empty()) {
			try {
				Settings::PitchSpeedRatioSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::PitchSpeedRatio, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::Plugin::PitchSpeedRatio, Settings::PitchSpeedRatioSetting->GetFloat());
	}

	if (Settings::IronSightsPitchSpeedRatioSetting) {
		value = GetINIValue(Settings::Names::Plugin::Section, Settings::Names::Plugin::IronSightsPitchSpeedRatio);
		if (!value.empty()) {
			try {
				Settings::IronSightsPitchSpeedRatioSetting->SetFloat(std::stof(value));
			} catch (const std::exception& e) {
				logger::error("Failed to parse {}: {}. Exception: {}", Settings::Names::Plugin::IronSightsPitchSpeedRatio, value, e.what());
			}
		}
		logger::info("{}: {}", Settings::Names::Plugin::IronSightsPitchSpeedRatio, Settings::IronSightsPitchSpeedRatioSetting->GetFloat());
	}
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg) {
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameDataReady:
		InitSettings();
		ReadINI();
		break;
	}
}

void RegisterFunction(RE::Scaleform::GFx::Movie* a_view, RE::Scaleform::GFx::Value* a_f4se_root, RE::Scaleform::GFx::FunctionHandler* a_handler, F4SE::stl::zstring a_name) {
	RE::Scaleform::GFx::Value fn;
	a_view->CreateFunction(&fn, a_handler);
	a_f4se_root->SetMember(a_name, fn);
}

class SettingHandler : public RE::Scaleform::GFx::FunctionHandler {
public:
	virtual void Call(const Params& a_params) override {
		if (a_params.argCount != 2 || a_params.args[0].GetType() != RE::Scaleform::GFx::Value::ValueType::kString) {
			return;
		}

		if (strcmp(a_params.args[0].GetString(), Settings::Names::IronSightsFOVRotateMult) == 0) {
			if (Settings::IronSightsFOVRotateMultSetting) {
				Settings::IronSightsFOVRotateMultSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::UseScopeFOVRotateMult) == 0) {
			Settings::UseScopeFOVRotateMult = a_params.args[1].GetBoolean();
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::ScopeFOVRotateMult) == 0) {
			Settings::ScopeFOVRotateMult = static_cast<float>(a_params.args[1].GetNumber());
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::MouseHeadingXScale) == 0) {
			if (Settings::MouseHeadingXScaleSetting) {
				Settings::MouseHeadingXScaleSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::MouseHeadingYScale) == 0) {
			if (Settings::MouseHeadingYScaleSetting) {
				Settings::MouseHeadingYScaleSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::MouseHeadingNormalizeMax) == 0) {
			if (Settings::MouseHeadingNormalizeMaxSetting) {
				Settings::MouseHeadingNormalizeMaxSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::PitchSpeedRatio) == 0) {
			if (Settings::PitchSpeedRatioSetting) {
				Settings::PitchSpeedRatioSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), Settings::Names::Plugin::IronSightsPitchSpeedRatio) == 0) {
			if (Settings::IronSightsPitchSpeedRatioSetting) {
				Settings::IronSightsPitchSpeedRatioSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		}
	}
};

bool RegisterScaleforms(RE::Scaleform::GFx::Movie* a_view, RE::Scaleform::GFx::Value* a_value) {
	RegisterFunction(a_view, a_value, new SettingHandler(), "Set"sv);
	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface * a_f4se, F4SE::PluginInfo * a_info) {
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface * a_f4se) {
	F4SE::AllocTrampoline(static_cast<size_t>(1) << 8u);
	F4SE::Init(a_f4se);

	InstallHook();

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message) {
		message->RegisterListener(OnF4SEMessage);
	}

	const F4SE::ScaleformInterface* scaleform = F4SE::GetScaleformInterface();
	if (scaleform) {
		scaleform->Register(Version::PROJECT, RegisterScaleforms);
	}

	return true;
}
