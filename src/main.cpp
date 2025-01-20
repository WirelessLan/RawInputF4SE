#include <Windows.h>
#include <xbyak/xbyak.h>

RE::Setting* g_ironSightsFOVRotateMultSetting = nullptr;
RE::Setting* g_mouseHeadingXScaleSetting = nullptr;
RE::Setting* g_mouseHeadingYScaleSetting = nullptr;
RE::Setting* g_mouseHeadingNormalizeMaxSetting = nullptr;
RE::Setting* g_pitchSpeedRatioSetting = nullptr;
RE::Setting* g_ironSightsPitchSpeedRatioSetting = nullptr;

RE::BGSKeyword* g_hasScope = nullptr;

bool g_useScopeFOVRotateMult = false;
float g_scopeFOVRotateMult = 0.0f;

bool IsFirstPerson() {
	RE::PlayerCamera* pCam = RE::PlayerCamera::GetSingleton();
	if (!pCam) {
		return false;
	}

	return pCam->currentState == pCam->cameraStates[RE::CameraState::kFirstPerson];
}

float FilterControllerOutput_Hook() {
	if (!g_useScopeFOVRotateMult) {
		return g_ironSightsFOVRotateMultSetting->GetFloat();
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

			if (g_hasScope && (instanceData->flags & 0x00200000 || instanceData->keywords->HasKeyword(g_hasScope))) {
				return g_scopeFOVRotateMult != 0.0f ? g_scopeFOVRotateMult : g_ironSightsFOVRotateMultSetting->GetFloat();
			}
		}
	}

	return g_ironSightsFOVRotateMultSetting->GetFloat();
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
	for (auto& it : RE::INISettingCollection::GetSingleton()->settings) {
		if (!g_ironSightsFOVRotateMultSetting && it->GetKey() == "fIronSightsFOVRotateMult") {
			g_ironSightsFOVRotateMultSetting = it;
		}
		if (!g_mouseHeadingXScaleSetting && it->GetKey() == "fMouseHeadingXScale:Controls") {
			g_mouseHeadingXScaleSetting = it;
		}
		if (!g_mouseHeadingYScaleSetting && it->GetKey() == "fMouseHeadingYScale:Controls") {
			g_mouseHeadingYScaleSetting = it;
		}
		if (!g_mouseHeadingNormalizeMaxSetting && it->GetKey() == "fMouseHeadingNormalizeMax:Controls") {
			g_mouseHeadingNormalizeMaxSetting = it;
		}
		if (!g_pitchSpeedRatioSetting && it->GetKey() == "fPitchSpeedRatio:Controls") {
			g_pitchSpeedRatioSetting = it;
		}
		if (!g_ironSightsPitchSpeedRatioSetting && it->GetKey() == "fIronSightsPitchSpeedRatio:Controls") {
			g_ironSightsPitchSpeedRatioSetting = it;
		}
	}

	auto hasScopeForm = RE::TESForm::GetFormByID(0x0009F425);
	if (hasScopeForm) {
		g_hasScope = hasScopeForm->As<RE::BGSKeyword>();
	}

	if (!g_ironSightsFOVRotateMultSetting) {
		logger::error("Unable to find settings: fIronSightsFOVRotateMult");
	}
	if (!g_mouseHeadingXScaleSetting) {
		logger::error("Unable to find settings: fMouseHeadingXScale:Controls");
	}
	if (!g_mouseHeadingYScaleSetting) {
		logger::error("Unable to find settings: fMouseHeadingYScale:Controls");
	}
	if (!g_mouseHeadingNormalizeMaxSetting) {
		logger::error("Unable to find settings: fMouseHeadingNormalizeMax:Controls");
	}
	if (!g_pitchSpeedRatioSetting) {
		logger::error("Unable to find settings: fPitchSpeedRatio:Controls");
	}
	if (!g_ironSightsPitchSpeedRatioSetting) {
		logger::error("Unable to find settings: fIronSightsPitchSpeedRatio:Controls");
	}
	if (!g_hasScope) {
		logger::error("Unable to find HasScope Keyword(0009F425)");
	}
}

std::string GetINIValue(const char* section, const char* key) {
	static const std::string& configPath = "Data\\MCM\\Settings\\" + std::string(Version::PROJECT) + ".ini";
	char result[256]{};
	GetPrivateProfileStringA(section, key, NULL, result, sizeof(result), configPath.c_str());
	return result;
}

void ReadINI() {
	std::string value;

	if (g_ironSightsFOVRotateMultSetting) {
		value = GetINIValue("Settings", "fIronSightsFOVRotateMult");
		if (!value.empty()) {
			try {
				g_ironSightsFOVRotateMultSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fIronSightsFOVRotateMult: {}", g_ironSightsFOVRotateMultSetting->GetFloat());
	}

	value = GetINIValue("Settings", "bUseScopeFOVRotateMult");
	if (!value.empty()) {
		try {
			g_useScopeFOVRotateMult = static_cast<bool>(std::stoul(value));
		} catch (...) {}
	}
	logger::info("bUseScopeFOVRotateMult: {}", g_scopeFOVRotateMult);

	value = GetINIValue("Settings", "fScopeFOVRotateMult");
	if (!value.empty()) {
		try {
			g_scopeFOVRotateMult = std::stof(value);
		} catch (...) {}
	}
	logger::info("fScopeFOVRotateMult: {}", g_scopeFOVRotateMult);

	if (g_mouseHeadingXScaleSetting) {
		value = GetINIValue("Settings", "fMouseHeadingXScale");
		if (!value.empty()) {
			try {
				g_mouseHeadingXScaleSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fMouseHeadingXScale: {}", g_mouseHeadingXScaleSetting->GetFloat());
	}

	if (g_mouseHeadingYScaleSetting) {
		value = GetINIValue("Settings", "fMouseHeadingYScale");
		if (!value.empty()) {
			try {
				g_mouseHeadingYScaleSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fMouseHeadingYScale: {}", g_mouseHeadingYScaleSetting->GetFloat());
	}

	if (g_mouseHeadingNormalizeMaxSetting) {
		value = GetINIValue("Settings", "fMouseHeadingNormalizeMax");
		if (!value.empty()) {
			try {
				g_mouseHeadingNormalizeMaxSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fMouseHeadingNormalizeMax: {}", g_mouseHeadingNormalizeMaxSetting->GetFloat());
	}

	if (g_pitchSpeedRatioSetting) {
		value = GetINIValue("Settings", "fPitchSpeedRatio");
		if (!value.empty()) {
			try {
				g_pitchSpeedRatioSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fPitchSpeedRatio: {}", g_pitchSpeedRatioSetting->GetFloat());
	}

	if (g_ironSightsPitchSpeedRatioSetting) {
		value = GetINIValue("Settings", "fIronSightsPitchSpeedRatio");
		if (!value.empty()) {
			try {
				g_ironSightsPitchSpeedRatioSetting->SetFloat(std::stof(value));
			} catch (...) {}
		}
		logger::info("fIronSightsPitchSpeedRatio: {}", g_ironSightsPitchSpeedRatioSetting->GetFloat());
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

		if (strcmp(a_params.args[0].GetString(), "fIronSightsFOVRotateMult") == 0) {
			if (g_ironSightsFOVRotateMultSetting) {
				g_ironSightsFOVRotateMultSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), "bUseScopeFOVRotateMult") == 0) {
			g_useScopeFOVRotateMult = a_params.args[1].GetBoolean();
		} else if (strcmp(a_params.args[0].GetString(), "fScopeFOVRotateMult") == 0) {
			if (g_hasScope) {
				g_scopeFOVRotateMult = static_cast<float>(a_params.args[1].GetNumber());
			}
		} else if (strcmp(a_params.args[0].GetString(), "fMouseHeadingXScale") == 0) {
			if (g_mouseHeadingXScaleSetting) {
				g_mouseHeadingXScaleSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), "fMouseHeadingYScale") == 0) {
			if (g_mouseHeadingYScaleSetting) {
				g_mouseHeadingYScaleSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), "fMouseHeadingNormalizeMax") == 0) {
			if (g_mouseHeadingNormalizeMaxSetting) {
				g_mouseHeadingNormalizeMaxSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), "fPitchSpeedRatio") == 0) {
			if (g_pitchSpeedRatioSetting) {
				g_pitchSpeedRatioSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
			}
		} else if (strcmp(a_params.args[0].GetString(), "fIronSightsPitchSpeedRatio") == 0) {
			if (g_ironSightsPitchSpeedRatioSetting) {
				g_ironSightsPitchSpeedRatioSetting->SetFloat(static_cast<float>(a_params.args[1].GetNumber()));
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
