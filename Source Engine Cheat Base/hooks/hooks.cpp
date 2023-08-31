// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "hooks.hpp"

#include <tchar.h>
#include <iostream>
#include <d3d9.h>
#include <dinput.h>

#include "..\features\misc\postprocessing\PostProccessing.h"
#include "..\features\misc\logs.h"
#include "..\features\misc\misc.h"
#include "..\features\visuals\esp\other_esp.h"
#include "..\features\visuals\world\radar.h"
#include "Render.h"

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")
#pragma comment(lib, "freetype.lib")

#include <shlobj.h>
#include <shlwapi.h>
#include <thread>
#include "..\menu\menu.h"
#include "../menu/Bytesa.h"
#include <Data/font.h>
#include <Data/icon.h>

auto _visible = true;
static auto d3d_init = false;

namespace INIT
{
	HMODULE Dll;
	HWND Window;
	WNDPROC OldWindow;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//extern IMGUI_IMPL_API LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks
{
	int rage_weapon = 0;
	int legit_weapon = 0;
	bool menu_open = false;
	bool input_shouldListen = false;

	ButtonCode_t* input_receivedKeyval;

	LRESULT __stdcall Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		static auto is_down = true;
		static auto is_clicked = false;

		if (GetAsyncKeyState(VK_INSERT))
		{
			is_clicked = false;
			is_down = true;
		}
		else if (!GetAsyncKeyState(VK_INSERT) && is_down)
		{
			is_clicked = true;
			is_down = false;
		}
		else
		{
			is_clicked = false;
			is_down = false;
		}

		if (is_clicked)
		{
			menu_open = !menu_open;

			if (menu_open && g_ctx.available())
			{
				if (g_ctx.globals.current_weapon != -1)
				{
					if (g_cfg.ragebot.enable)
						rage_weapon = g_ctx.globals.current_weapon;
					else if (g_cfg.legitbot.enabled)
						legit_weapon = g_ctx.globals.current_weapon;
				}
			}
		}

		auto pressed_buttons = false;
		auto pressed_menu_key = uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP || uMsg == WM_MOUSEWHEEL;

		if (g_ctx.local()->is_alive() && !pressed_menu_key && !g_ctx.globals.focused_on_input)
			pressed_buttons = true;

		if (!pressed_buttons && d3d_init && menu_open && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) && !input_shouldListen)
			return true;

		if (menu_open && (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_MOUSEMOVE) && !input_shouldListen)
			return false;

		return CallWindowProc(INIT::OldWindow, hWnd, uMsg, wParam, lParam);
	}

	long __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice)
	{
		static auto original_fn = directx_hook->get_func_address <EndSceneFn> (42);
		return original_fn(pDevice);
	}

	void GUI_Init(IDirect3DDevice9* pDevice)
	{
		ImGui::CreateContext();

		ImGui_ImplWin32_Init(INIT::Window);
		ImGui_ImplDX9_Init(pDevice);

		auto& io = ImGui::GetIO();
		auto& style = ImGui::GetStyle();

		style.WindowMinSize = ImVec2(10, 10);

		ImFontConfig font_config;
		font_config.OversampleH = 1; //or 2 is the same
		font_config.OversampleV = 1;
		font_config.PixelSnapH = 1;

		static const ImWchar ranges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x044F, // Cyrillic
			0x0804, 0x0004,//CH-S
			0x0404, 0x7C04,//CH-T
			0,
		};

		char windows_directory[MAX_PATH];
		GetWindowsDirectory(windows_directory, MAX_PATH);

		auto verdana_directory = (std::string)windows_directory + "\\Fonts\\Verdana.ttf";

		// Menu fonts
		c_menu::get().futura_small = io.Fonts->AddFontFromMemoryTTF((void*)futuramediumc, sizeof(futuramediumc), 15.f, &font_config, ranges);
		c_menu::get().futura = io.Fonts->AddFontFromMemoryTTF((void*)futuramediumc, sizeof(futuramediumc), 17.f, &font_config, ranges);
		c_menu::get().futura_large = io.Fonts->AddFontFromMemoryTTF((void*)futuramediumc, sizeof(futuramediumc), 20.f, &font_config, ranges);
		c_menu::get().gotham = io.Fonts->AddFontFromMemoryTTF((void*)GothamPro, sizeof(GothamPro), 13.f, &font_config, ranges);

		// Icon fonts
		c_menu::get().ico_menu = io.Fonts->AddFontFromMemoryTTF((void*)icomenu, sizeof(icomenu), 20.f, &font_config, io.Fonts->GetGlyphRangesCyrillic());
		c_menu::get().ico_bottom = io.Fonts->AddFontFromMemoryTTF((void*)iconbot, sizeof(iconbot), 20.f, &font_config, io.Fonts->GetGlyphRangesCyrillic());

		c_menu::get().esp_name = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 12.0f, NULL, ranges);
		c_menu::get().esp_weapon = io.Fonts->AddFontFromMemoryTTF((void*)weapons_font, sizeof(weapons_font), 15, &font_config, io.Fonts->GetGlyphRangesDefault());
		c_menu::get().smallest_pixel = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 12.0f, NULL, ranges);
		c_menu::get().name = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 38.0f, NULL, ranges);
		c_menu::get().isis = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 21.0f, NULL, ranges);
		c_menu::get().icon_font = io.Fonts->AddFontFromMemoryTTF((void*)Icons, sizeof(Icons), 40, &font_config, io.Fonts->GetGlyphRangesDefault());
		c_menu::get().weapon_icons = io.Fonts->AddFontFromMemoryTTF((void*)weapons_font, sizeof(weapons_font), 18, &font_config, io.Fonts->GetGlyphRangesDefault());
		c_menu::get().weapon_icons2 = io.Fonts->AddFontFromMemoryTTF((void*)weapons_font, sizeof(weapons_font), 20, &font_config, io.Fonts->GetGlyphRangesDefault());
		c_menu::get().font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 13.0f, NULL, ranges);
		c_menu::get().interfaces = io.Fonts->AddFontFromMemoryTTF((void*)interfaces, sizeof(interfaces), 15.f * 1.25, &font_config, ranges);
		c_menu::get().timersz = io.Fonts->AddFontFromMemoryTTF((void*)timers, sizeof(timers), 20, &font_config, ranges);
		c_menu::get().verdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 20.0f, NULL, ranges);

		//if (c_menu::get().inferno == nullptr)
		//	D3DXCreateTextureFromFileInMemoryEx(pDevice, inferno, sizeof(inferno), 52, 52, D3DX_DEFAULT, D3DUSAGE_DYNAMIC, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &c_menu::get().inferno);

		c_menu::get().fontx = io.Fonts->AddFontFromMemoryTTF(&Font, sizeof Font, 14);
		c_menu::get().bold = io.Fonts->AddFontFromMemoryTTF(&Bold, sizeof Bold, 13);
		c_menu::get().default_font = io.Fonts->AddFontFromMemoryTTF(&Regular, sizeof Regular, 16);

		io.Fonts->AddFontFromMemoryTTF(museo500_binary, sizeof museo500_binary, 14);
		static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		ImFontConfig icons_config;
		c_menu::get().cray = io.Fonts->AddFontFromMemoryTTF(&font_awesome_binary, sizeof font_awesome_binary, 13, &icons_config, icon_ranges);
		io.Fonts->AddFontFromMemoryTTF(museo900_binary, sizeof museo900_binary, 28);

		ImGui_ImplDX9_CreateDeviceObjects();
		d3d_init = true;
	}

	long __stdcall hooked_present(IDirect3DDevice9* device, RECT* src_rect, RECT* dest_rect, HWND dest_wnd_override, RGNDATA* dirty_region)
	{
		static auto original = directx_hook->get_func_address <PresentFn> (17);
		g_ctx.local((player_t*)m_entitylist()->GetClientEntity(m_engine()->GetLocalPlayer()), true);

		if (!d3d_init)
			GUI_Init(device);

		IDirect3DVertexDeclaration9* vertex_dec;
		device->GetVertexDeclaration(&vertex_dec);

		IDirect3DVertexShader9* vertex_shader;
		device->GetVertexShader(&vertex_shader);

		c_menu::get().device = device;
		PostProcessing::setDevice(device);
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		g_Render->BeginScene();

		/*

		⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿
		⣿⣿⣿⣿⣿⣿⣿⣿⣏⠀⠙⢿⣿⣿⣿⣿⣿⠛⠁⢀⣠⣿⣿⣿⣿⣿⣿⣿⣿⣿
		⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠙⢿⣿⡿⠁⢀⣴⡿⡿⠿⠿⣿⣿⣿⣿⣿⣿⣿
		⣿⣿⣿⣿⠟⠋⠉⠉⠉⠉⠛⠳⢄⠀⠉⠁⠀⠀⢀⣀⣀⣀⣀⣸⣿⣿⣿⣿⣿⣿
		⣿⣿⡿⠁⢀⣴⡶⠶⠶⣶⣤⣄⠀⠀⠀⣀⣀⡀⠀⠉⠙⠻⠿⣿⣿⣿⣿⣿⣿⣿
		⣿⣿⡇⠀⢸⣿⡀⠀⠀⠈⠻⣿⣷⣤⠀⠙⣿⣿⠿⠖⠂⠠⠀⠈⠛⢿⣿⣿⣿⣿
		⣿⣿⣿⡄⠀⠻⢷⣄⣀⠀⣀⣿⣿⣿⡇⠀⢸⠁⠀⣠⣤⡤⠄⠀⠀⠀⠻⣿⣿⣿
		⣿⣿⠏⠀⣠⣄⣀⠀⠉⠉⠛⠛⠛⠛⠀⣠⣿⠀⠘⢿⣿⠀⠀⠀⠀⠀⠀⢿⣿⣿
		⣿⡿⠀⢰⡿⠿⣿⣿⣷⣶⣦⣤⣤⣴⣾⣿⣿⣧⣄⠈⠙⠓⠂⠀⠀⣠⠀⠸⣿⣿
		⣿⡇⠀⢸⣧⣀⠀⠙⠛⠿⠿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⣤⡤⠴⠚⠛⡆⠀⢻⣿
		⣿⣿⡄⠀⠻⣿⣷⣶⣤⠀⠀⡀⠀⠉⠉⠉⠉⠉⠉⠉⠁⠀⣀⣀⣤⣴⠃⠀⣿⣿
		⣿⣿⣿⣦⠀⠙⢿⣿⣿⠀⠀⠛⠿⠇⠀⠂⠀⢷⣿⡀⠀⣿⣿⣿⡿⠃⠀⣼⣿⣿
		⣿⣿⣿⣿⣷⣄⠀⠙⠻⣷⣦⣤⣄⣀⣀⣆⠀⠘⠛⠃⠀⢸⠟⠉⢀⣠⣾⣿⣿⣿
		⣿⣿⣿⣿⣿⣿⣷⣤⣀⠀⠉⠛⠿⠿⢿⠿⠷⠤⠀⠀⠀⠀⣠⣴⣿⣿⣿⣿⣿⣿
		⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⣄⣀⣀⣀⣀⣠⣤⣴⣶⣿⣿⣿⣿⣿⣿

		*/

		g_Render->EndScene();

		c_menu::get().draw(menu_open);
		otheresp::get().spread_crosshair(device);

		if (g_ctx.globals.should_update_radar)
			Radar::get().OnMapLoad(m_engine()->GetLevelNameShort(), device);
		else
			Radar::get().Render();

		ImGui::EndFrame();
		ImGui::Render();

		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

		device->SetVertexShader(vertex_shader);
		device->SetVertexDeclaration(vertex_dec);

		return original(device, src_rect, dest_rect, dest_wnd_override, dirty_region);
	}

	long __stdcall Hooked_EndScene_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		static auto ofunc = directx_hook->get_func_address<EndSceneResetFn>(16);

		if (!d3d_init)
			return ofunc(pDevice, pPresentationParameters);

		ImGui_ImplDX9_InvalidateDeviceObjects();

		PostProcessing::onDeviceReset();

		auto hr = ofunc(pDevice, pPresentationParameters);

		if (SUCCEEDED(hr))
			ImGui_ImplDX9_CreateDeviceObjects();

		return hr;
	}

	DWORD original_getforeignfallbackfontname;
	DWORD original_setupbones;
	DWORD original_doextrabonesprocessing;
	DWORD original_standardblendingrules;
	DWORD original_updateclientsideanimation;
	DWORD original_physicssimulate;
	DWORD original_modifyeyeposition;
	DWORD original_calcviewmodelbob;
	DWORD original_processinterpolatedlist;
	DWORD original_clmove;

	vmthook* directx_hook;
	vmthook* client_hook;
	vmthook* clientstate_hook;
	vmthook* engine_hook;
	vmthook* clientmode_hook;
	vmthook* inputinternal_hook;
	vmthook* renderview_hook;
	vmthook* panel_hook;
	vmthook* modelcache_hook;
	vmthook* materialsys_hook;
	vmthook* modelrender_hook;
	vmthook* surface_hook;
	vmthook* bspquery_hook;
	vmthook* prediction_hook;
	vmthook* trace_hook;
	vmthook* filesystem_hook;

	C_HookedEvents hooked_events;
}

void __fastcall hooks::hooked_setkeycodestate(void* thisptr, void* edx, ButtonCode_t code, bool bDown)
{
	static auto original_fn = inputinternal_hook->get_func_address <SetKeyCodeState_t> (91);

	if (input_shouldListen && bDown)
	{
		input_shouldListen = false;

		if (input_receivedKeyval)
			*input_receivedKeyval = code;
	}

	return original_fn(thisptr, code, bDown);
}

void __fastcall hooks::hooked_setmousecodestate(void* thisptr, void* edx, ButtonCode_t code, MouseCodeState_t state)
{
	static auto original_fn = inputinternal_hook->get_func_address <SetMouseCodeState_t> (92);

	if (input_shouldListen && state == BUTTON_PRESSED)
	{
		input_shouldListen = false;

		if (input_receivedKeyval)
			*input_receivedKeyval = code;
	}

	return original_fn(thisptr, code, state);
}
/*
void __fastcall hooks::hooked_renderGlowBoxes(void* ecx, void* edx)
{
	typedef int(__thiscall* fnAddGlowBox)(void* ecx, void* edx);

	//using Fn = void(__thiscall*)(void* ecx, void* edx);
	//static auto fn = reinterpret_cast<Fn>(util::FindSignature(crypt_str("client.dll"), crypt_str("53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 83 EC 60 56 57 8B F9 89")));

	static auto original_fn = clientmode_hook->get_func_address <fnAddGlowBox>(99);

	original_fn(ecx, edx);
}
*/