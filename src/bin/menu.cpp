#include "include/menu.h"
#include "include/MenuTable.h"
#include "include/settings.h"
#include "include/blockHandler.h"
#include "include/lib/SKSEMenuFramework.h"

namespace VCMenu
{
	namespace
	{
		using namespace ImGuiMCP;

		constexpr const char* kPath = "Data\\SKSE\\Plugins\\ValhallaCombat\\Settings.ini";
		constexpr const char* kOldPath = "Data\\MCM\\Settings\\ValhallaCombat.ini";

		std::unordered_map<std::string, float> g_values;  // "Section|Key" -> value; only touched from the render thread and Commit
		std::mutex                             g_lock;
		const Entry*                           g_capture = nullptr;
		bool                                   g_registered = false;

		std::string Id(const Entry& e) { return std::string(e.section) + "|" + e.key; }

		float Get(const Entry& e)
		{
			std::lock_guard lock(g_lock);
			const auto it = g_values.find(Id(e));
			return it != g_values.end() ? it->second : e.def;
		}

		void SetLocal(const Entry& e, float v)
		{
			std::lock_guard lock(g_lock);
			g_values[Id(e)] = v;
		}

		void LoadAll()
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.LoadFile(kPath);
			std::lock_guard lock(g_lock);
			for (const auto& page : kPages) {
				for (int i = 0; i < page.count; ++i) {
					const auto& e = page.entries[i];
					if (e.key[0] && (e.kind == Kind::Toggle || e.kind == Kind::Slider || e.kind == Kind::Key || e.kind == Kind::Enum)) {
						if (const char* v = ini.GetValue(e.section, e.key)) {
							g_values[Id(e)] = static_cast<float>(std::atof(v));
						}
					}
				}
			}
		}

		/* Writes one value to the settings file and has the mod read its settings again. */
		void Commit(const Entry& e, float v)
		{
			SetLocal(e, v);
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.LoadFile(kPath);
			char text[64];
			if (e.kind == Kind::Slider) {
				std::snprintf(text, sizeof(text), "%.*f", e.decimals < 1 ? 1 : (e.decimals > 4 ? 4 : e.decimals), v);
			} else {
				std::snprintf(text, sizeof(text), "%d", static_cast<int>(v));
			}
			ini.SetValue(e.section, e.key, text);
			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(kPath).parent_path(), ec);
			ini.SaveFile(kPath);
			settings::readSettings();
		}

		const char* KeyName(int code)
		{
			static const char* const kb[] = {
				nullptr, "Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter",
				"Left Ctrl", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", "Left Shift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "Right Shift",
				"Num *", "Left Alt", "Space", "Caps Lock", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Num Lock", "Scroll Lock", "Num 7", "Num 8", "Num 9",
				"Num -", "Num 4", "Num 5", "Num 6", "Num +", "Num 1", "Num 2", "Num 3", "Num 0", "Num .", nullptr, nullptr, nullptr, "F11", "F12"
			};
			static char buf[32];
			if (code < 0) {
				return "Not set";
			}
			if (code < static_cast<int>(sizeof(kb) / sizeof(kb[0])) && kb[code]) {
				return kb[code];
			}
			switch (code) {
			case 156: return "Num Enter";
			case 157: return "Right Ctrl";
			case 181: return "Num /";
			case 183: return "Print Screen";
			case 184: return "Right Alt";
			case 199: return "Home";
			case 200: return "Up";
			case 201: return "Page Up";
			case 203: return "Left";
			case 205: return "Right";
			case 207: return "End";
			case 208: return "Down";
			case 209: return "Page Down";
			case 210: return "Insert";
			case 211: return "Delete";
			case 256: return "Left Mouse";
			case 257: return "Right Mouse";
			case 258: return "Middle Mouse";
			default: break;
			}
			if (code >= 256 && code < 266) {
				std::snprintf(buf, sizeof(buf), "Mouse %d", code - 255);
			} else {
				std::snprintf(buf, sizeof(buf), "Button %d", code);
			}
			return buf;
		}

		bool g_captureArmed = false;  // the click that started the capture must be released first

		/* While a key bind waits, the menu overlay owns the input and the game's input handler gets nothing, so the press is read from the overlay and
		translated to the game's codes (keyboard scan code, mouse button + 256). Returns -1 for nothing yet, -2 for cancel (Escape), else the code. */
		int PollCapture()
		{
			if (!g_captureArmed) {
				g_captureArmed = !IsMouseDown(0);
				return -1;
			}
			struct Map { ImGuiKey key; int code; };
			static const Map keys[] = {
				{ ImGuiKey_1, 2 }, { ImGuiKey_2, 3 }, { ImGuiKey_3, 4 }, { ImGuiKey_4, 5 }, { ImGuiKey_5, 6 }, { ImGuiKey_6, 7 }, { ImGuiKey_7, 8 }, { ImGuiKey_8, 9 }, { ImGuiKey_9, 10 },
				{ ImGuiKey_0, 11 }, { ImGuiKey_Minus, 12 }, { ImGuiKey_Equal, 13 }, { ImGuiKey_Backspace, 14 }, { ImGuiKey_Tab, 15 },
				{ ImGuiKey_Q, 16 }, { ImGuiKey_W, 17 }, { ImGuiKey_E, 18 }, { ImGuiKey_R, 19 }, { ImGuiKey_T, 20 }, { ImGuiKey_Y, 21 }, { ImGuiKey_U, 22 }, { ImGuiKey_I, 23 },
				{ ImGuiKey_O, 24 }, { ImGuiKey_P, 25 }, { ImGuiKey_LeftBracket, 26 }, { ImGuiKey_RightBracket, 27 }, { ImGuiKey_Enter, 28 }, { ImGuiKey_LeftCtrl, 29 },
				{ ImGuiKey_A, 30 }, { ImGuiKey_S, 31 }, { ImGuiKey_D, 32 }, { ImGuiKey_F, 33 }, { ImGuiKey_G, 34 }, { ImGuiKey_H, 35 }, { ImGuiKey_J, 36 }, { ImGuiKey_K, 37 },
				{ ImGuiKey_L, 38 }, { ImGuiKey_Semicolon, 39 }, { ImGuiKey_Apostrophe, 40 }, { ImGuiKey_GraveAccent, 41 }, { ImGuiKey_LeftShift, 42 }, { ImGuiKey_Backslash, 43 },
				{ ImGuiKey_Z, 44 }, { ImGuiKey_X, 45 }, { ImGuiKey_C, 46 }, { ImGuiKey_V, 47 }, { ImGuiKey_B, 48 }, { ImGuiKey_N, 49 }, { ImGuiKey_M, 50 }, { ImGuiKey_Comma, 51 },
				{ ImGuiKey_Period, 52 }, { ImGuiKey_Slash, 53 }, { ImGuiKey_RightShift, 54 }, { ImGuiKey_KeypadMultiply, 55 }, { ImGuiKey_LeftAlt, 56 }, { ImGuiKey_Space, 57 },
				{ ImGuiKey_CapsLock, 58 }, { ImGuiKey_F1, 59 }, { ImGuiKey_F2, 60 }, { ImGuiKey_F3, 61 }, { ImGuiKey_F4, 62 }, { ImGuiKey_F5, 63 }, { ImGuiKey_F6, 64 },
				{ ImGuiKey_F7, 65 }, { ImGuiKey_F8, 66 }, { ImGuiKey_F9, 67 }, { ImGuiKey_F10, 68 }, { ImGuiKey_NumLock, 69 }, { ImGuiKey_ScrollLock, 70 },
				{ ImGuiKey_Keypad7, 71 }, { ImGuiKey_Keypad8, 72 }, { ImGuiKey_Keypad9, 73 }, { ImGuiKey_KeypadSubtract, 74 }, { ImGuiKey_Keypad4, 75 }, { ImGuiKey_Keypad5, 76 },
				{ ImGuiKey_Keypad6, 77 }, { ImGuiKey_KeypadAdd, 78 }, { ImGuiKey_Keypad1, 79 }, { ImGuiKey_Keypad2, 80 }, { ImGuiKey_Keypad3, 81 }, { ImGuiKey_Keypad0, 82 },
				{ ImGuiKey_KeypadDecimal, 83 }, { ImGuiKey_F11, 87 }, { ImGuiKey_F12, 88 }, { ImGuiKey_KeypadEnter, 156 }, { ImGuiKey_RightCtrl, 157 }, { ImGuiKey_KeypadDivide, 181 },
				{ ImGuiKey_PrintScreen, 183 }, { ImGuiKey_RightAlt, 184 }, { ImGuiKey_Home, 199 }, { ImGuiKey_UpArrow, 200 }, { ImGuiKey_PageUp, 201 }, { ImGuiKey_LeftArrow, 203 },
				{ ImGuiKey_RightArrow, 205 }, { ImGuiKey_End, 207 }, { ImGuiKey_DownArrow, 208 }, { ImGuiKey_PageDown, 209 }, { ImGuiKey_Insert, 210 }, { ImGuiKey_Delete, 211 },
			};
			if (IsKeyPressed(ImGuiKey_Escape, false)) {
				return -2;
			}
			for (const auto& m : keys) {
				if (IsKeyPressed(m.key, false)) {
					return m.code;
				}
			}
			for (int b = 0; b < 5; ++b) {
				if (IsMouseClicked(b, false)) {
					return 256 + b;
				}
			}
			return -1;
		}

		bool HiddenState(Hidden h)
		{
			switch (h) {
			case Hidden::TrueHudApi: return settings::facts::TrueHudAPI_Obtained;
			case Hidden::TrueHudSpecialMeter: return settings::facts::TrueHudAPI_HasSpecialBarControl;
			case Hidden::EldenCounterDamage:
			case Hidden::EldenCounterNpc: return blockHandler::EldenCounterCompatibility::on;
			default: return false;
			}
		}

		bool Enabled(const Entry& e, const bool (&state)[16])
		{
			bool ok = true;
			if (e.condNot) {
				ok = ok && !state[e.condNot];
			}
			bool hasOr = false, anyOr = false;
			for (const int id : e.condOr) {
				if (id > 0) {
					hasOr = true;
					anyOr = anyOr || state[id];
				}
			}
			if (hasOr) {
				ok = ok && anyOr;
			}
			for (const int id : e.condAnd) {
				if (id > 0) {
					ok = ok && state[id];
				}
			}
			return ok;
		}

		void Tip(const Entry& e)
		{
			if (e.help[0] && IsItemHovered()) {
				SetTooltip("%s", e.help);
			}
		}

		void __stdcall RenderOverview()
		{
			TextUnformatted("Valhalla Combat - settings in SKSE Menu Framework (SkyUI and MCM Helper are not needed).");
			Spacing();
			const auto line = [](const char* name, bool ok) {
				TextUnformatted(name);
				SameLine();
				TextColored(ok ? ImVec4(0.65f, 0.9f, 0.5f, 1.0f) : ImVec4(0.95f, 0.6f, 0.4f, 1.0f), ok ? "found" : "not found");
			};
			line("TrueHUD API", settings::facts::TrueHudAPI_Obtained);
			line("TrueHUD special meter control", settings::facts::TrueHudAPI_HasSpecialBarControl);
			line("Precision API (melee hits)", settings::facts::PrecisionAPI_Obtained);
			Spacing();
			TextWrapped("Settings are saved to Data/SKSE/Plugins/ValhallaCombat/Settings.ini as you change them and apply at once.");
		}

		void RenderPage(int a_page)
		{
			const auto& page = kPages[a_page];
			if (g_capture) {
				const int code = PollCapture();
				if (code == -2) {
					g_capture = nullptr;
				} else if (code >= 0) {
					const Entry* entry = g_capture;
					g_capture = nullptr;
					Commit(*entry, static_cast<float>(code));
				}
			}
			bool state[16]{};
			for (int i = 0; i < page.count; ++i) {
				const auto& e = page.entries[i];
				if (e.control > 0 && e.control < 16) {
					state[e.control] = e.kind == Kind::Hidden ? HiddenState(e.hidden) : Get(e) != 0.0f;
				}
			}
			for (int i = 0; i < page.count; ++i) {
				const auto& e = page.entries[i];
				if (e.kind == Kind::Hidden) {
					continue;
				}
				if (e.kind == Kind::Header) {
					Spacing();
					SeparatorText(e.label);
					continue;
				}
				const bool enabled = Enabled(e, state);
				if (!enabled && e.skip) {
					continue;
				}
				if (e.kind == Kind::Text) {
					TextUnformatted(e.label);
					SameLine();
					TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", e.help);
					continue;
				}
				if (!enabled) {
					BeginDisabled();
				}
				const std::string label = std::string(e.label) + "##" + e.key;
				switch (e.kind) {
				case Kind::Toggle: {
					bool v = Get(e) != 0.0f;
					if (Checkbox(label.c_str(), &v)) {
						Commit(e, v ? 1.0f : 0.0f);
					}
					Tip(e);
					break;
				}
				case Kind::Slider: {
					float             v = Get(e);
					const std::string fmt = std::format("%.{}f", e.decimals);
					if (SliderFloat(label.c_str(), &v, e.min, e.max, fmt.c_str())) {
						if (e.step > 0.0f) {
							v = std::round(v / e.step) * e.step;
						}
						SetLocal(e, v);
					}
					Tip(e);
					if (IsItemDeactivatedAfterEdit()) {
						Commit(e, Get(e));
					}
					break;
				}
				case Kind::Enum: {
					int v = static_cast<int>(Get(e));
					if (Combo(label.c_str(), &v, e.options)) {
						Commit(e, static_cast<float>(v));
					}
					Tip(e);
					break;
				}
				case Kind::Key: {
					TextUnformatted(e.label);
					SameLine();
					const bool        waiting = g_capture == &e;
					const std::string button = std::string(waiting ? "Press a key..." : KeyName(static_cast<int>(Get(e)))) + "##bind" + e.key;
					if (Button(button.c_str())) {
						g_capture = waiting ? nullptr : &e;
						g_captureArmed = false;
					}
					SameLine();
					if (Button((std::string("Clear##") + e.key).c_str())) {
						g_capture = nullptr;
						Commit(e, -1.0f);
					}
					Tip(e);
					break;
				}
				default:
					break;
				}
				if (!enabled) {
					EndDisabled();
				}
			}
		}

		void __stdcall RenderPage0() { RenderPage(0); }
		void __stdcall RenderPage1() { RenderPage(1); }
		void __stdcall RenderPage2() { RenderPage(2); }
		void __stdcall RenderPage3() { RenderPage(3); }
	}

	void EnsureSettingsFile()
	{
		std::error_code ec;
		if (std::filesystem::exists(kPath, ec) || !std::filesystem::exists(kOldPath, ec)) {
			return;
		}
		std::filesystem::create_directories(std::filesystem::path(kPath).parent_path(), ec);
		std::filesystem::copy_file(kOldPath, kPath, ec);
		logger::info("Copied the MCM Helper settings {} to {}", kOldPath, kPath);
	}

	bool CaptureActive() { return g_capture != nullptr; }

	void OnKeyCaptured(int a_key)
	{
		const Entry* entry = g_capture;
		g_capture = nullptr;
		if (entry) {
			Commit(*entry, static_cast<float>(a_key));
		}
	}

	void Register()
	{
		if (g_registered) {
			return;
		}
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("SKSE Menu Framework is not installed: no settings menu (the settings file can be edited by hand)");
			return;
		}
		EnsureSettingsFile();
		LoadAll();
		SKSEMenuFramework::SetSection("Valhalla Combat");
		SKSEMenuFramework::AddSectionItem("Overview", RenderOverview);
		static void(__stdcall* const renderers[])() = { RenderPage0, RenderPage1, RenderPage2, RenderPage3 };
		for (int i = 0; i < static_cast<int>(sizeof(renderers) / sizeof(renderers[0])); ++i) {
			SKSEMenuFramework::AddSectionItem(kPages[i].name, renderers[i]);
		}
		g_registered = true;
		logger::info("Registered with SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
