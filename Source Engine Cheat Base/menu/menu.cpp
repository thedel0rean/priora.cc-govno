// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <ShlObj_core.h>
#include <unordered_map>
#include "menu.h"
#include "ImGui/code_editor.h"
#include "constchars.h"
#include "../features/misc/logs.h"
#include "../features/misc/postprocessing/GameData.h"
#include "../features/misc/postprocessing/PostProccessing.h"
#include <cctype>                           // для функции tolower
#include "../Data/icon.h"


#define ALPHA (ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar| ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float)
#define NOALPHA (ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float)

std::vector <std::string> files;
std::vector <std::string> scripts;
std::string editing_script;

auto selected_script = 0;
auto loaded_editing_script = false;

static auto menu_setupped = false;
static auto should_update = true;
IDirect3DTexture9* all_skins[36];
static float model_rotation = -180;
static int model_type = 0;

namespace skins {
	std::string get_wep(int id, int custom_index = -1, bool knife = true)
	{
		if (custom_index > -1)
		{
			if (knife)
			{
				switch (custom_index)
				{
				case 0: return crypt_str("weapon_knife");
				case 1: return crypt_str("weapon_bayonet");
				case 2: return crypt_str("weapon_knife_css");
				case 3: return crypt_str("weapon_knife_skeleton");
				case 4: return crypt_str("weapon_knife_outdoor");
				case 5: return crypt_str("weapon_knife_cord");
				case 6: return crypt_str("weapon_knife_canis");
				case 7: return crypt_str("weapon_knife_flip");
				case 8: return crypt_str("weapon_knife_gut");
				case 9: return crypt_str("weapon_knife_karambit");
				case 10: return crypt_str("weapon_knife_m9_bayonet");
				case 11: return crypt_str("weapon_knife_tactical");
				case 12: return crypt_str("weapon_knife_falchion");
				case 13: return crypt_str("weapon_knife_survival_bowie");
				case 14: return crypt_str("weapon_knife_butterfly");
				case 15: return crypt_str("weapon_knife_push");
				case 16: return crypt_str("weapon_knife_ursus");
				case 17: return crypt_str("weapon_knife_gypsy_jackknife");
				case 18: return crypt_str("weapon_knife_stiletto");
				case 19: return crypt_str("weapon_knife_widowmaker");
				}
			}
			else
			{
				switch (custom_index)
				{
				case 0: return crypt_str("ct_gloves"); //-V1037
				case 1: return crypt_str("studded_bloodhound_gloves");
				case 2: return crypt_str("t_gloves");
				case 3: return crypt_str("ct_gloves");
				case 4: return crypt_str("sporty_gloves");
				case 5: return crypt_str("slick_gloves");
				case 6: return crypt_str("leather_handwraps");
				case 7: return crypt_str("motorcycle_gloves");
				case 8: return crypt_str("specialist_gloves");
				case 9: return crypt_str("studded_hydra_gloves");
				}
			}
		}
		else
		{
			switch (id)
			{
			case 0: return crypt_str("knife");
			case 1: return crypt_str("gloves");
			case 2: return crypt_str("weapon_ak47");
			case 3: return crypt_str("weapon_aug");
			case 4: return crypt_str("weapon_awp");
			case 5: return crypt_str("weapon_cz75a");
			case 6: return crypt_str("weapon_deagle");
			case 7: return crypt_str("weapon_elite");
			case 8: return crypt_str("weapon_famas");
			case 9: return crypt_str("weapon_fiveseven");
			case 10: return crypt_str("weapon_g3sg1");
			case 11: return crypt_str("weapon_galilar");
			case 12: return crypt_str("weapon_glock");
			case 13: return crypt_str("weapon_m249");
			case 14: return crypt_str("weapon_m4a1_silencer");
			case 15: return crypt_str("weapon_m4a1");
			case 16: return crypt_str("weapon_mac10");
			case 17: return crypt_str("weapon_mag7");
			case 18: return crypt_str("weapon_mp5sd");
			case 19: return crypt_str("weapon_mp7");
			case 20: return crypt_str("weapon_mp9");
			case 21: return crypt_str("weapon_negev");
			case 22: return crypt_str("weapon_nova");
			case 23: return crypt_str("weapon_hkp2000");
			case 24: return crypt_str("weapon_p250");
			case 25: return crypt_str("weapon_p90");
			case 26: return crypt_str("weapon_bizon");
			case 27: return crypt_str("weapon_revolver");
			case 28: return crypt_str("weapon_sawedoff");
			case 29: return crypt_str("weapon_scar20");
			case 30: return crypt_str("weapon_ssg08");
			case 31: return crypt_str("weapon_sg556");
			case 32: return crypt_str("weapon_tec9");
			case 33: return crypt_str("weapon_ump45");
			case 34: return crypt_str("weapon_usp_silencer");
			case 35: return crypt_str("weapon_xm1014");
			default: return crypt_str("unknown");
			}
		}
	}

	IDirect3DTexture9* get_skin_preview(const char* weapon_name, const std::string& skin_name, IDirect3DDevice9* device)
	{
		IDirect3DTexture9* skin_image = nullptr;
		std::string vpk_path;

		if (strcmp(weapon_name, crypt_str("unknown")) && strcmp(weapon_name, crypt_str("knife")) && strcmp(weapon_name, crypt_str("gloves"))) //-V526
		{
			if (skin_name.empty() || skin_name == crypt_str("default"))
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/") + std::string(weapon_name) + crypt_str(".png");
			else
				vpk_path = crypt_str("resource/flash/econ/default_generated/") + std::string(weapon_name) + crypt_str("_") + std::string(skin_name) + crypt_str("_light_large.png");
		}
		else
		{
			if (!strcmp(weapon_name, crypt_str("knife")))
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/weapon_knife.png");
			else if (!strcmp(weapon_name, crypt_str("gloves")))
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/ct_gloves.png");
			else if (!strcmp(weapon_name, crypt_str("unknown")))
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/weapon_snowball.png");

		}
		const auto handle = m_basefilesys()->Open(vpk_path.c_str(), crypt_str("r"), crypt_str("GAME"));
		if (handle)
		{
			int file_len = m_basefilesys()->Size(handle);
			char* image = new char[file_len]; //-V121

			m_basefilesys()->Read(image, file_len, handle);
			m_basefilesys()->Close(handle);

			D3DXCreateTextureFromFileInMemory(device, image, file_len, &skin_image);
			delete[] image;
		}

		if (!skin_image)
		{
			std::string vpk_path;

			if (strstr(weapon_name, crypt_str("bloodhound")) != NULL || strstr(weapon_name, crypt_str("hydra")) != NULL)
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/ct_gloves.png");
			else
				vpk_path = crypt_str("resource/flash/econ/weapons/base_weapons/") + std::string(weapon_name) + crypt_str(".png");

			const auto handle = m_basefilesys()->Open(vpk_path.c_str(), crypt_str("r"), crypt_str("GAME"));

			if (handle)
			{
				int file_len = m_basefilesys()->Size(handle);
				char* image = new char[file_len]; //-V121

				m_basefilesys()->Read(image, file_len, handle);
				m_basefilesys()->Close(handle);

				D3DXCreateTextureFromFileInMemory(device, image, file_len, &skin_image);
				delete[] image;
			}
		}

		return skin_image;
	}
}

namespace configs {
	std::string get_config_dir()
	{
		std::string folder;
		static TCHAR path[MAX_PATH];

		if (SUCCEEDED(SHGetFolderPath(NULL, 0x001a, NULL, NULL, path)))
			folder = std::string(path) + crypt_str("\\Legendware\\Configs\\");

		CreateDirectory(folder.c_str(), NULL);
		return folder;
	}

	void load_config()
	{
		if (cfg_manager->files.empty())
			return;

		cfg_manager->load(cfg_manager->files.at(g_cfg.selected_config), false);
		c_lua::get().unload_all_scripts();

		for (auto& script : g_cfg.scripts.scripts)
			c_lua::get().load_script(c_lua::get().get_script_id(script));

		scripts = c_lua::get().scripts;

		if (selected_script >= scripts.size())
			selected_script = scripts.size() - 1; //-V103

		for (auto& current : scripts)
		{
			if (current.size() >= 5 && current.at(current.size() - 1) == 'c')
				current.erase(current.size() - 5, 5);
			else if (current.size() >= 4)
				current.erase(current.size() - 4, 4);
		}

		for (auto i = 0; i < g_cfg.skins.skinChanger.size(); ++i)
			all_skins[i] = nullptr;

		g_cfg.scripts.scripts.clear();

		cfg_manager->load(cfg_manager->files.at(g_cfg.selected_config), true);
		cfg_manager->config_files();

		eventlogs::get().add(crypt_str("Loaded ") + files.at(g_cfg.selected_config) + crypt_str(" config"), false);
	}

	void save_config()
	{
		if (cfg_manager->files.empty())
			return;

		g_cfg.scripts.scripts.clear();

		for (auto i = 0; i < c_lua::get().scripts.size(); ++i)
		{
			auto script = c_lua::get().scripts.at(i);

			if (c_lua::get().loaded.at(i))
				g_cfg.scripts.scripts.emplace_back(script);
		}

		cfg_manager->save(cfg_manager->files.at(g_cfg.selected_config));
		cfg_manager->config_files();

		eventlogs::get().add(crypt_str("Saved ") + files.at(g_cfg.selected_config) + crypt_str(" config"), false);
	}

	void remove_config()
	{
		if (cfg_manager->files.empty())
			return;

		eventlogs::get().add(crypt_str("Removed ") + files.at(g_cfg.selected_config) + crypt_str(" config"), false);

		cfg_manager->remove(cfg_manager->files.at(g_cfg.selected_config));
		cfg_manager->config_files();

		files = cfg_manager->files;

		if (g_cfg.selected_config >= files.size())
			g_cfg.selected_config = files.size() - 1; //-V103

		for (auto& current : files)
			if (current.size() > 2)
				current.erase(current.size() - 3, 3);
	}

	void add_config()
	{
		auto empty = true;

		for (auto current : g_cfg.new_config_name)
		{
			if (current != ' ')
			{
				empty = false;
				break;
			}
		}

		if (empty)
			g_cfg.new_config_name = crypt_str("config");

		eventlogs::get().add(crypt_str("Added ") + g_cfg.new_config_name + crypt_str(" config"), false);

		if (g_cfg.new_config_name.find(crypt_str(".lw")) == std::string::npos)
			g_cfg.new_config_name += crypt_str(".lw");

		cfg_manager->save(g_cfg.new_config_name);
		cfg_manager->config_files();

		g_cfg.selected_config = cfg_manager->files.size() - 1; //-V103
		files = cfg_manager->files;

		for (auto& current : files)
			if (current.size() > 2)
				current.erase(current.size() - 3, 3);
	}

	void lua_edit(std::string window_name)
	{
		std::string file_path;

		auto get_dir = [&]() -> void
		{
			static TCHAR path[MAX_PATH];

			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, NULL, path)))
				file_path = std::string(path) + crypt_str("\\Legendware\\Scripts\\");

			CreateDirectory(file_path.c_str(), NULL);
			file_path += window_name + crypt_str(".lua");
		};

		get_dir();
		const char* child_name = (window_name + window_name).c_str();

		ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Once);
		ImGui::Begin(window_name.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);

		static TextEditor editor;

		if (!loaded_editing_script)
		{
			static auto lang = TextEditor::LanguageDefinition::Lua();

			editor.SetLanguageDefinition(lang);
			editor.SetReadOnly(false);

			std::ifstream t(file_path);

			if (t.good()) // does while exist?
			{
				std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
				editor.SetText(str); // setup script content
			}

			loaded_editing_script = true;
		}

		// dpi scale for font
		// we dont need to resize it for full scale
		ImGui::SetWindowFontScale(1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f));

		// new size depending on dpi scale
		ImGui::SetWindowSize(ImVec2(ImFloor(800 * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f))), ImFloor(700 * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f)))));
		editor.Render(child_name, ImGui::GetWindowSize() - ImVec2(0, 66 * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f))));

		// seperate code with buttons
		ImGui::Separator();

		// set cursor pos to right edge of window
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetWindowSize().x - (16.f * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f))) - (250.f * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f))));
		ImGui::BeginGroup();

		if (ImGui::CustomButton(crypt_str("Save"), (crypt_str("Save") + window_name).c_str(), ImVec2(125 * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f)), 0), true, c_menu::get().ico_bottom, crypt_str("S")))
		{
			std::ofstream out;

			out.open(file_path);
			out << editor.GetText() << std::endl;
			out.close();
		}

		ImGui::SameLine();

		// TOOD: close button will close window (return in start of function)
		if (ImGui::CustomButton(crypt_str("Close"), (crypt_str("Close") + window_name).c_str(), ImVec2(125 * (1.f + ((c_menu::get().dpi_scale - 1.0) * 0.5f)), 0)))
		{
			g_ctx.globals.focused_on_input = false;
			loaded_editing_script = false;
			editing_script.clear();
		}

		ImGui::EndGroup();

		ImGui::PopStyleVar();
		ImGui::End();
	}


}

namespace menu_widgets {
	__forceinline void padding(float x, float y)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x * c_menu::get().dpi_scale);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y * c_menu::get().dpi_scale);
	}


	// title of content child
	void child_title(const char* label)
	{
		ImGui::PushFont(c_menu::get().futura_large);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (12 * c_menu::get().dpi_scale));
		ImGui::Text(label);

		ImGui::PopStyleColor();
		ImGui::PopFont();
	}

	void draw_combo(const char* name, int& variable, const char* labels[], int count)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6 * c_menu::get().dpi_scale);
		ImGui::Text(name);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5 * c_menu::get().dpi_scale);
		ImGui::Combo(std::string(crypt_str("##COMBO__") + std::string(name)).c_str(), &variable, labels, count);
	}

	void draw_combo(const char* name, int& variable, bool (*items_getter)(void*, int, const char**), void* data, int count)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6 * c_menu::get().dpi_scale);
		ImGui::Text(name);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5 * c_menu::get().dpi_scale);
		ImGui::Combo(std::string(crypt_str("##COMBO__") + std::string(name)).c_str(), &variable, items_getter, data, count);
	}

	void draw_multicombo(std::string name, std::vector<int>& variable, const char* labels[], int count, std::string& preview)
	{
		padding(-3, -6);
		ImGui::Text((crypt_str(" ") + name).c_str());
		padding(0, -5);

		auto hashname = crypt_str("##") + name; // we dont want to render name of combo

		for (auto i = 0, j = 0; i < count; i++)
		{
			if (variable[i])
			{
				if (j)
					preview += crypt_str(", ") + (std::string)labels[i];
				else
					preview = labels[i];

				j++;
			}
		}

		if (ImGui::BeginCombo(hashname.c_str(), preview.c_str())) // draw start
		{
			ImGui::Spacing();
			ImGui::BeginGroup();
			{

				for (auto i = 0; i < count; i++)
					ImGui::Selectable(labels[i], (bool*)&variable[i], ImGuiSelectableFlags_DontClosePopups);

			}
			ImGui::EndGroup();
			ImGui::Spacing();

			ImGui::EndCombo();
		}

		preview = crypt_str("None"); // reset preview to use later
	}

	bool LabelClick(const char* label, bool* v, const char* unique_id)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		// The concatoff/on thingies were for my weapon config system so if we're going to make that, we still need this aids.
		char Buf[64];
		_snprintf(Buf, 62, crypt_str("%s"), label);

		char getid[128];
		sprintf_s(getid, 128, crypt_str("%s%s"), label, unique_id);


		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(getid);
		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

		const ImRect check_bb(window->DC.CursorPos, ImVec2(label_size.y + style.FramePadding.y * 2 + window->DC.CursorPos.x, window->DC.CursorPos.y + label_size.y + style.FramePadding.y * 2));
		ImGui::ItemSize(check_bb, style.FramePadding.y);

		ImRect total_bb = check_bb;

		if (label_size.x > 0)
		{
			ImGui::SameLine(0, style.ItemInnerSpacing.x);
			const ImRect text_bb(ImVec2(window->DC.CursorPos.x, window->DC.CursorPos.y + style.FramePadding.y), ImVec2(window->DC.CursorPos.x + label_size.x, window->DC.CursorPos.y + style.FramePadding.y + label_size.y));

			ImGui::ItemSize(ImVec2(text_bb.GetWidth(), check_bb.GetHeight()), style.FramePadding.y);
			total_bb = ImRect(ImMin(check_bb.Min, text_bb.Min), ImMax(check_bb.Max, text_bb.Max));
		}

		if (!ImGui::ItemAdd(total_bb, id))
			return false;

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
		if (pressed)
			*v = !(*v);

		if (*v)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(126 / 255.f, 131 / 255.f, 219 / 255.f, 1.f));
		if (label_size.x > 0.0f)
			ImGui::RenderText(ImVec2(check_bb.GetTL().x + 12, check_bb.GetTL().y), Buf);
		if (*v)
			ImGui::PopStyleColor();

		return pressed;

	}


	void draw_keybind(const char* label, key_bind* key_bind, const char* unique_id)
	{
		// reset bind if we re pressing esc
		if (key_bind->key == KEY_ESCAPE)
			key_bind->key = KEY_NONE;

		auto clicked = false;
		auto text = (std::string)m_inputsys()->ButtonCodeToString(key_bind->key);

		if (key_bind->key <= KEY_NONE || key_bind->key >= KEY_MAX)
			text = crypt_str("None");

		// if we clicked on keybind
		if (hooks::input_shouldListen && hooks::input_receivedKeyval == &key_bind->key)
		{
			clicked = true;
			text = crypt_str("...");
		}

		auto textsize = ImGui::CalcTextSize(text.c_str()).x + 8 * c_menu::get().dpi_scale;
		auto labelsize = ImGui::CalcTextSize(label);

		ImGui::Text(label);
		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowSize().x - (ImGui::GetWindowSize().x - ImGui::CalcItemWidth()) - max(50 * c_menu::get().dpi_scale, textsize));
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3 * c_menu::get().dpi_scale);

		if (ImGui::KeybindButton(text.c_str(), unique_id, ImVec2(max(50 * c_menu::get().dpi_scale, textsize), 23 * c_menu::get().dpi_scale), clicked))
			clicked = true;


		if (clicked)
		{
			hooks::input_shouldListen = true;
			hooks::input_receivedKeyval = &key_bind->key;
		}

		static auto hold = false, toggle = false;

		switch (key_bind->mode)
		{
		case HOLD:
			hold = true;
			toggle = false;
			break;
		case TOGGLE:
			toggle = true;
			hold = false;
			break;
		}

		if (ImGui::BeginPopup(unique_id))
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6 * c_menu::get().dpi_scale);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((ImGui::GetCurrentWindow()->Size.x / 2) - (ImGui::CalcTextSize(crypt_str("Hold")).x / 2)));
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 11);

			if (LabelClick(crypt_str("Hold"), &hold, unique_id))
			{
				if (hold)
				{
					toggle = false;
					key_bind->mode = HOLD;
				}
				else if (toggle)
				{
					hold = false;
					key_bind->mode = TOGGLE;
				}
				else
				{
					toggle = false;
					key_bind->mode = HOLD;
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((ImGui::GetCurrentWindow()->Size.x / 2) - (ImGui::CalcTextSize(crypt_str("Toggle")).x / 2)));
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 11);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 9 * c_menu::get().dpi_scale);

			if (LabelClick(crypt_str("Toggle"), &toggle, unique_id))
			{
				if (toggle)
				{
					hold = false;
					key_bind->mode = TOGGLE;
				}
				else if (hold)
				{
					toggle = false;
					key_bind->mode = HOLD;
				}
				else
				{
					hold = false;
					key_bind->mode = TOGGLE;
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void draw_semitabs(const char* labels[], int count, int& tab, ImGuiStyle& style)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (2 * c_menu::get().dpi_scale));

		// center of main child
		float offset = 343 * c_menu::get().dpi_scale;

		// text size padding + frame padding
		for (int i = 0; i < count; i++)
			offset -= (ImGui::CalcTextSize(labels[i]).x) / 2 + style.FramePadding.x * 2;

		// set new padding
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
		ImGui::BeginGroup();

		for (int i = 0; i < count; i++)
		{
			// switch current tab
			if (ImGui::ContentTab(labels[i], tab == i))
				tab = i;

			// continue drawing on same line 
			if (i + 1 != count)
			{
				ImGui::SameLine();
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + style.ItemSpacing.x);
			}
		}

		ImGui::EndGroup();
	}

	__forceinline void tab_start()
	{
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (20 * c_menu::get().dpi_scale), ImGui::GetCursorPosY() + (5 * c_menu::get().dpi_scale)));
	}

	__forceinline void tab_end()
	{
		ImGui::PopStyleVar();
		ImGui::SetWindowFontScale(c_menu::get().dpi_scale);
	}
}

void c_menu::menu_setup(ImGuiStyle& style) //-V688
{
	ImGui::StyleColorsClassic(); // colors setup
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Once); // window pos setup
	ImGui::SetNextWindowBgAlpha(min(style.Alpha, 0.94f)); // window bg alpha setup

	styles.WindowPadding = style.WindowPadding;
	styles.WindowRounding = style.WindowRounding;
	styles.WindowMinSize = style.WindowMinSize;
	styles.ChildRounding = style.ChildRounding;
	styles.PopupRounding = style.PopupRounding;
	styles.FramePadding = style.FramePadding;
	styles.FrameRounding = style.FrameRounding;
	styles.ItemSpacing = style.ItemSpacing;
	styles.ItemInnerSpacing = style.ItemInnerSpacing;
	styles.TouchExtraPadding = style.TouchExtraPadding;
	styles.IndentSpacing = style.IndentSpacing;
	styles.ColumnsMinSpacing = style.ColumnsMinSpacing;
	styles.ScrollbarSize = style.ScrollbarSize;
	styles.ScrollbarRounding = style.ScrollbarRounding;
	styles.GrabMinSize = style.GrabMinSize;
	styles.GrabRounding = style.GrabRounding;
	styles.TabRounding = style.TabRounding;
	styles.TabMinWidthForUnselectedCloseButton = style.TabMinWidthForUnselectedCloseButton;
	styles.DisplayWindowPadding = style.DisplayWindowPadding;
	styles.DisplaySafeAreaPadding = style.DisplaySafeAreaPadding;
	styles.MouseCursorScale = style.MouseCursorScale;

	// setup skins preview
	for (auto i = 0; i < g_cfg.skins.skinChanger.size(); i++)
		if (!all_skins[i])
			all_skins[i] = skins::get_skin_preview(skins::get_wep(i, (i == 0 || i == 1) ? g_cfg.skins.skinChanger.at(i).definition_override_vector_index : -1, i == 0).c_str(), g_cfg.skins.skinChanger.at(i).skin_name, device); //-V810

	menu_setupped = true; // we dont want to setup menu again
}

void c_menu::dpi_resize(float scale_factor, ImGuiStyle& style) //-V688
{
	style.WindowPadding = (styles.WindowPadding * scale_factor);
	style.WindowRounding = (styles.WindowRounding * scale_factor);
	style.WindowMinSize = (styles.WindowMinSize * scale_factor);
	style.ChildRounding = (styles.ChildRounding * scale_factor);
	style.PopupRounding = (styles.PopupRounding * scale_factor);
	style.FramePadding = (styles.FramePadding * scale_factor);
	style.FrameRounding = (styles.FrameRounding * scale_factor);
	style.ItemSpacing = (styles.ItemSpacing * scale_factor);
	style.ItemInnerSpacing = (styles.ItemInnerSpacing * scale_factor);
	style.TouchExtraPadding = (styles.TouchExtraPadding * scale_factor);
	style.IndentSpacing = (styles.IndentSpacing * scale_factor);
	style.ColumnsMinSpacing = (styles.ColumnsMinSpacing * scale_factor);
	style.ScrollbarSize = (styles.ScrollbarSize * scale_factor);
	style.ScrollbarRounding = (styles.ScrollbarRounding * scale_factor);
	style.GrabMinSize = (styles.GrabMinSize * scale_factor);
	style.GrabRounding = (styles.GrabRounding * scale_factor);
	style.TabRounding = (styles.TabRounding * scale_factor);
	if (styles.TabMinWidthForUnselectedCloseButton != FLT_MAX) //-V550
		style.TabMinWidthForUnselectedCloseButton = (styles.TabMinWidthForUnselectedCloseButton * scale_factor);
	style.DisplayWindowPadding = (styles.DisplayWindowPadding * scale_factor);
	style.DisplaySafeAreaPadding = (styles.DisplaySafeAreaPadding * scale_factor);
	style.MouseCursorScale = (styles.MouseCursorScale * scale_factor);
}


void c_menu::draw(bool is_open)
{
	static auto w = 0, h = 0, current_h = 0;
	m_engine()->GetScreenSize(w, current_h);

	if (h != current_h)
	{
		if (h)
			update_scripts = true;

		h = current_h;
		update_dpi = true;
	}

	// animation related code
	static float m_alpha = 0.0002f;
	m_alpha = math::clamp(m_alpha + (2.1f * ImGui::GetIO().DeltaTime * (is_open ? 4.2f : -4.2f)), 0.0001f, 1.f);

	// set alpha in class to use later in widgets
	public_alpha = m_alpha;

	if (m_alpha <= 0.0001f)
		return;

	// set new alpha
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_alpha);

	// setup colors and some styles
	if (!menu_setupped)
		c_menu::menu_setup(ImGui::GetStyle());

	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrab].x, ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrab].y, ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrab].z, m_alpha));



	// last active tab to switch effect & reverse alpha & preview alpha
	// IMPORTANT: DO TAB SWITCHING BY LAST_TAB!!!!!
	static int last_tab = active_tab;
	static bool preview_reverse = false;

	ImGui::SetWindowSize(ImVec2(694.22, 494.25));
	ImGui::Begin(crypt_str("limehook"), nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground);
	{
		{
			ImGui::SetWindowSize(ImVec2(694.22, 494.25));
			auto p = ImGui::GetWindowPos();
			auto s = ImGui::GetWindowSize();
			auto draw = ImGui::GetWindowDrawList();



			draw->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), ImColor(1, 1, 1));
			draw->AddRectFilled(ImVec2(p.x, p.y + 25), ImVec2(p.x + s.x, p.y + s.y - 25), ImColor(1, 1, 1));


			draw->AddText(c_menu::get().default_font, 16, ImVec2(p.x + 308, p.y + 10.72), ImColor(255, 255, 255), "priority.cc");


			draw->AddRectFilledMultiColor(ImVec2(p.x, p.y + 46.49), ImVec2(p.x + s.x, p.y + 50.26), ImColor(62, 64, 109, 255), ImColor(142, 148, 255, 255), ImColor(142, 148, 255, 255), ImColor(62, 64, 109, 255)); // upper gradient

		}

		static int tab = 0;
		static int tab2 = 0;


		ImGui::BeginGroup();
		{
			ImGui::SetCursorPos(ImVec2(15.09, 60.37));
			if (ImGui::Tab(ICON_FA_KNIFE_KITCHEN, "Ragebot", 1, tab == 0, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 0;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38));
			if (ImGui::Tab(ICON_FA_GHOST, "Anti-Aim", 2, tab == 1, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 1;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38 + 38));
			if (ImGui::Tab(ICON_FA_EYE, "Visuals", 3, tab == 2, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 2;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38 + 38 + 38));
			if (ImGui::Tab(ICON_FA_FEATHER, "Skins", 3, tab == 3, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 3;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38 + 38 + 38 + 38));

			if (ImGui::Tab(ICON_FA_ANGEL, "Misc", 3, tab == 4, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 4;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38 + 38 + 38 + 38 + 38));
			if (ImGui::Tab(ICON_FA_FILE_ARCHIVE, "Configs", 3, tab == 5, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 5;
			ImGui::SetCursorPos(ImVec2(15.09, 60.37 + 38 + 38 + 38 + 38 + 38 + 38));
			if (ImGui::Tab(ICON_FA_CODE, "Scripts", 3, tab == 6, ImVec2(128.28 * dpi_scale, 37.73 * dpi_scale)))
				tab = 6;
		}
		ImGui::EndGroup();

		if (tab == 5)
		{
			ImGui::PushFont(font);
			{
				ImGui::BeginGroup();
				ImGui::SetCursorPos(ImVec2(158.46, 60.37));
				ImGui::MenuChild("General", ImVec2(252.79, 418.79), false);
				{
					static auto should_update = true;

					if (should_update)
					{
						should_update = false;

						cfg_manager->config_files();
						files = cfg_manager->files;

						for (auto& current : files)
							if (current.size() > 2)
								current.erase(current.size() - 3, 3);
					}

					if (ImGui::CustomButton(crypt_str("Open configs folder"), crypt_str("##CONFIG__FOLDER"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
					{
						std::string folder;

						auto get_dir = [&folder]() -> void
						{
							static TCHAR path[MAX_PATH];

							if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, NULL, path)))
								folder = std::string(path) + crypt_str("\\Legendware\\Configs\\");

							CreateDirectory(folder.c_str(), NULL);
						};

						get_dir();
						ShellExecute(NULL, crypt_str("open"), folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
					}

					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
					ImGui::ListBoxConfigArray(crypt_str("##CONFIGS"), &g_cfg.selected_config, files, 7);
					ImGui::PopStyleVar();

					if (ImGui::CustomButton(crypt_str("Refresh configs"), crypt_str("##CONFIG__REFRESH"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
					{
						cfg_manager->config_files();
						files = cfg_manager->files;

						for (auto& current : files)
							if (current.size() > 2)
								current.erase(current.size() - 3, 3);
					}

					static char config_name[64] = "\0";

					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
					ImGui::InputText(crypt_str("##configname"), config_name, sizeof(config_name));
					ImGui::PopStyleVar();

					if (ImGui::CustomButton(crypt_str("Create config"), crypt_str("##CONFIG__CREATE"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
					{
						g_cfg.new_config_name = config_name;
						configs::add_config();
					}

					static auto next_save = false;
					static auto prenext_save = false;
					static auto clicked_sure = false;
					static auto save_time = m_globals()->m_realtime;
					static auto save_alpha = 1.0f;

					save_alpha = math::clamp(save_alpha + (4.f * ImGui::GetIO().DeltaTime * (!prenext_save ? 1.f : -1.f)), 0.01f, 1.f);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, save_alpha * public_alpha * (1.f - preview_alpha));

					if (!next_save)
					{
						clicked_sure = false;

						if (prenext_save && save_alpha <= 0.01f)
							next_save = true;

						if (ImGui::CustomButton(crypt_str("Save config"), crypt_str("##CONFIG__SAVE"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
						{
							save_time = m_globals()->m_realtime;
							prenext_save = true;
						}
					}
					else
					{
						if (prenext_save && save_alpha <= 0.01f)
						{
							prenext_save = false;
							next_save = !clicked_sure;
						}

						if (ImGui::CustomButton(crypt_str("Are you sure?"), crypt_str("##AREYOUSURE__SAVE"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
						{
							configs::save_config();
							prenext_save = true;
							clicked_sure = true;
						}

						if (!clicked_sure && m_globals()->m_realtime > save_time + 1.5f)
						{
							prenext_save = true;
							clicked_sure = true;
						}
					}

					ImGui::PopStyleVar();

					if (ImGui::CustomButton(crypt_str("Load config"), crypt_str("##CONFIG__LOAD"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
						configs::load_config();

					static auto next_delete = false;
					static auto prenext_delete = false;
					static auto clicked_sure_del = false;
					static auto delete_time = m_globals()->m_realtime;
					static auto delete_alpha = 1.0f;

					delete_alpha = math::clamp(delete_alpha + (4.f * ImGui::GetIO().DeltaTime * (!prenext_delete ? 1.f : -1.f)), 0.01f, 1.f);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, delete_alpha * public_alpha * (1.f - preview_alpha));

					if (!next_delete)
					{
						clicked_sure_del = false;

						if (prenext_delete && delete_alpha <= 0.01f)
							next_delete = true;

						if (ImGui::CustomButton(crypt_str("Remove config"), crypt_str("##CONFIG__delete"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
						{
							delete_time = m_globals()->m_realtime;
							prenext_delete = true;
						}
					}
					else
					{
						if (prenext_delete && delete_alpha <= 0.01f)
						{
							prenext_delete = false;
							next_delete = !clicked_sure_del;
						}

						if (ImGui::CustomButton(crypt_str("Are you sure?"), crypt_str("##AREYOUSURE__delete"), ImVec2(629 * dpi_scale, 26 * dpi_scale)))
						{
							configs::remove_config();
							prenext_delete = true;
							clicked_sure_del = true;
						}

						if (!clicked_sure_del && m_globals()->m_realtime > delete_time + 1.5f)
						{
							prenext_delete = true;
							clicked_sure_del = true;
						}
					}

					ImGui::PopStyleVar();

				}

				ImGui::EndChild();
				ImGui::EndGroup();
			}
			{
				ImGui::BeginGroup();
				ImGui::SetCursorPos(ImVec2(426, 60));
				ImGui::MenuChild("Accuracy", ImVec2(253, 297), false);
				{

				}
				ImGui::EndChild();
				ImGui::EndGroup();
			}
			{
				ImGui::BeginGroup();
				ImGui::SetCursorPos(ImVec2(426, 372));
				ImGui::MenuChild("Exploit", ImVec2(253, 107), false);
				{

				}
				ImGui::EndChild();
				ImGui::EndGroup();
				ImGui::PopFont();
			}
		}


	}
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}