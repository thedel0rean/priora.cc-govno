#pragma once
#include "../win_includes.hpp"

class c_menu : public singleton<c_menu> {
public:
	void draw( bool is_open );
	void menu_setup(ImGuiStyle& style);

	float dpi_scale = 1.f;

	ImFont* futura;
	ImFont* futura_large;
	ImFont* futura_small;

	ImFont* gotham;

	ImFont* smallest_pixel;
	ImFont* esp_name;
	ImFont* esp_weapon;
	ImFont* name;
	ImFont* isis;
	ImFont* icon_font;
	ImFont* weapon_icons;
	ImFont* weapon_icons2;
	ImFont* fontx;
	ImFont* bold;
	ImFont* default_font;
	ImFont* font;
	ImFont* interfaces;
	ImFont* timersz;
	ImFont* verdana;
	ImFont* cray;

	ImFont* ico_menu;
	ImFont* ico_bottom;

	float public_alpha;
	IDirect3DDevice9* device;
	float color_buffer[4] = { 1.f, 1.f, 1.f, 1.f };
private:
	void dpi_resize(float scale_factor, ImGuiStyle& style);
	struct {
		ImVec2 WindowPadding;
		float  WindowRounding;
		ImVec2 WindowMinSize;
		float  ChildRounding;
		float  PopupRounding;
		ImVec2 FramePadding;
		float  FrameRounding;
		ImVec2 ItemSpacing;
		ImVec2 ItemInnerSpacing;
		ImVec2 TouchExtraPadding;
		float  IndentSpacing;
		float  ColumnsMinSpacing;
		float  ScrollbarSize;
		float  ScrollbarRounding;
		float  GrabMinSize;
		float  GrabRounding;
		float  TabRounding;
		float  TabMinWidthForUnselectedCloseButton;
		ImVec2 DisplayWindowPadding;
		ImVec2 DisplaySafeAreaPadding;
		float  MouseCursorScale;
	} styles;

	bool update_dpi = false;
	bool update_scripts = false;

	int active_tab_index;
	ImGuiStyle style;
	int width = 850, height = 560;
	float child_height;

	float preview_alpha = 1.f;

	int active_tab;

	int rage_section;
	int legit_section;
	int visuals_section;
	int players_section;
	int misc_section;
	int settings_section;



	std::string preview = crypt_str("None");
};
