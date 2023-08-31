// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "warning.h"
#include "../world_esp.h"
#include "../../../../hooks/Render.h"
#include "../../../../menu/menu.h"
#include "../../../misc/postprocessing/PostProccessing.h"

const char* index_to_grenade_name(int index)
{
	switch (index)
	{
	case WEAPON_SMOKEGRENADE: return "smoke"; break;
	case WEAPON_HEGRENADE: return "he grenade"; break;
	case WEAPON_MOLOTOV:return "molotov"; break;
	case WEAPON_INCGRENADE:return "molotov"; break;
	}
}
const char* index_to_grenade_name_icon(int index)
{
	switch (index)
	{
	case WEAPON_SMOKEGRENADE: return "k"; break;
	case WEAPON_HEGRENADE: return "j"; break;
	case WEAPON_MOLOTOV:return "M"; break;
		//case WEAPON_MOLOTOV:return "l"; break;
	case WEAPON_INCGRENADE:return "n"; break;
	}
}

inline float CSGO_Armor(float flDamage, int ArmorValue) {
	float flArmorRatio = 0.5f;
	float flArmorBonus = 0.5f;
	if (ArmorValue > 0) {
		float flNew = flDamage * flArmorRatio;
		float flArmor = (flDamage - flNew) * flArmorBonus;

		if (flArmor > static_cast<float>(ArmorValue)) {
			flArmor = static_cast<float>(ArmorValue) * (1.f / flArmorBonus);
			flNew = flDamage - flArmor;
		}

		flDamage = flNew;
	}
	return flDamage;
}

void rotate_point(Vector2D& point, Vector2D origin, bool clockwise, float angle) {
	Vector2D delta = point - origin;
	Vector2D rotated;

	if (clockwise) {
		rotated = Vector2D(delta.x * cosf(angle) - delta.y * sinf(angle), delta.x * sinf(angle) + delta.y * cosf(angle));
	}
	else {
		rotated = Vector2D(delta.x * sinf(angle) - delta.y * cosf(angle), delta.x * cosf(angle) + delta.y * sinf(angle));
	}

	point = rotated + origin;
}

// @note: actually dont use

/*void DrawBeamPaw(Vector src, Vector end, Color color)
{
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS; //TE_BEAMPOINTS
	beamInfo.m_vecStart = src;
	beamInfo.m_vecEnd = end;
	beamInfo.m_pszModelName = "sprites/purplelaser1.vmt";//sprites/purplelaser1.vmt
	beamInfo.m_pszHaloName = "sprites/purplelaser1.vmt";//sprites/purplelaser1.vmt
	beamInfo.m_flHaloScale = 0;//0
	beamInfo.m_flWidth = 0.35f;//11
	beamInfo.m_flEndWidth = 0.35f;//11
	beamInfo.m_flFadeLength = 1.0f;
	beamInfo.m_flAmplitude = 2.3;
	beamInfo.m_flBrightness = 255.f;
	beamInfo.m_flSpeed = 0.2f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 0.0;
	beamInfo.m_flRed = color.r();
	beamInfo.m_flGreen = color.g();
	beamInfo.m_flBlue = color.b();
	beamInfo.m_nSegments = 2;//40
	beamInfo.m_bRenderable = true;
	beamInfo.m_flLife = 0.03f;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM; //FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM

	Beam_t* myBeam = m_viewrenderbeams()->CreateBeamPoints(beamInfo);

	if (myBeam)
		m_viewrenderbeams()->DrawBeam(myBeam);
}*/

inline float CSGO_Armores(float flDamage, int ArmorValue) {
	float flArmorRatio = 0.5f;
	float flArmorBonus = 0.5f;
	if (ArmorValue > 0) {
		float flNew = flDamage * flArmorRatio;
		float flArmor = (flDamage - flNew) * flArmorBonus;

		if (flArmor > static_cast<float>(ArmorValue)) {
			flArmor = static_cast<float>(ArmorValue) * (1.f / flArmorBonus);
			flNew = flDamage - flArmor;
		}

		flDamage = flNew;
	}
	return flDamage;
}

bool c_grenade_prediction::data_t::draw() const
{
	if (m_path.size() <= 1u
		|| m_globals()->m_curtime >= m_expire_time)
		return false;

	float distance = g_ctx.local()->m_vecOrigin().DistTo(m_origin) / 12;

	static int iScreenWidth, iScreenHeight;
	m_engine()->GetScreenSize(iScreenWidth, iScreenHeight);
	auto* window = ImGui::GetCurrentWindow();
	auto* drawwindow = ImGui::GetWindowDrawList();

	auto prev_screen = ZERO;
	auto prev_on_screen = math::world_to_screen(std::get< Vector >(m_path.front()), prev_screen);

	int total_alpha = 0;
	//int latest_alpha;
	if (distance > 20)
	{
		//total_alpha = 255 - (distance * 3.5);
		//total_alpha = 255 - math::clamp(distance * 3.5, 0, 255);
		total_alpha = 255 - distance * 3.5, 0, 255;
	}
	else
	{
		total_alpha = 255 - (distance * 1);
		//latest_alpha = total_alpha;
	}
	//total_alpha = 255 - math::clamp(distance * 1, 0, 255);
	if (total_alpha <= 0)
		return false;


	for (auto i = 1u; i < m_path.size(); ++i) {
		auto cur_screen = ZERO, last_cur_screen = ZERO;
		const auto cur_on_screen = math::world_to_screen(std::get< Vector >(m_path.at(i)), cur_screen);

		if (prev_on_screen && cur_on_screen)
		{
			auto color = g_cfg.esp.grenade_proximity_tracers_colors;
			//DrawBeamPaw(std::get< Vector >(m_path.at(i - 1)), std::get< Vector >(m_path.at(i)), Color(color.r(), color.g(), color.b(), 100));
			//if (i % 4)

			g_Render->DrawLine((int)prev_screen.x, (int)prev_screen.y, (int)cur_screen.x, (int)cur_screen.y, Color(color.r(), color.g(), color.b(), 120), 1);

			//g_Render->DrawWave(std::get< Vector >(m_path.at(i - 1)), 1.f, Color(color.r(), color.g(), color.b(), 120), 1);
			//g_Render->AddRadialGradient(window->DrawList, ImVec2(prev_screen.x, prev_screen.y), 26, ImColor(color.r(), color.g(), color.b(), total_alpha), ImColor(color.r(), color.g(), color.b(), total_alpha));
		}

		prev_screen = cur_screen;
		prev_on_screen = cur_on_screen;
	}

	float percent = ((m_expire_time - m_globals()->m_curtime) / TICKS_TO_TIME(m_tick));
	int alpha_damage = 0;

	if (m_index == WEAPON_HEGRENADE && distance <= 20)
	{
		alpha_damage = 220 - 220 * (distance / 20);
	}

	if ((m_index == WEAPON_MOLOTOV || m_index == WEAPON_INCGRENADE) && distance <= 15)
	{
		alpha_damage = 220 - 220 * (distance / 15);
	}
	auto col = g_cfg.esp.grenade_proximity_warning_progress_color;
	Ray_t ray;
	CTraceFilterWorldAndPropsOnly filter;
	CGameTrace trace;

	/*auto origin = g_ctx.local()->m_vecOrigin();
	auto collideable = g_ctx.local()->GetCollideable();

	auto min = collideable->OBBMins() + origin;
	auto max = collideable->OBBMaxs() + origin;

	auto center = min + (max - min) * 0.5f;

	// get delta between center of mass and final nade pos.
	Vector delta = center - prev_on_screen;
	float d = (delta.Length() - 25.f) / 140.f;
	float damage = 105.f * std::exp(-d * d);

	auto dmg = max(static_cast<int>(ceilf(g_ctx_Armor(damage, g_ctx.local()->m_ArmorValue()))), 0);*/

	ray.Init(m_origin, m_origin - Vector(0.0f, 0.0f, 105.0f));
	m_trace()->TraceRay(ray, MASK_SOLID, &filter, &trace);
	if ((trace.fraction < 1.0f) && m_index == WEAPON_MOLOTOV || m_index == WEAPON_INCGRENADE) {
		auto vvt = trace.endpos + Vector(0.0f, 0.0f, 2.0f);
		g_Render->DrawRing3D(vvt.x, vvt.y, vvt.z, 90, 140, Color(col.r(), col.g(), col.b(), 255), Color(140, 140, 140, 35), 2, percent);
		static auto g_size = Vector2D(35.0f, 5.0f);
		Vector saddadsadadasdasda;
		if (math::world_to_screen(vvt, saddadsadadasdasda))
			//g_Render->CircleFilled(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, 21, Color(25, 25, 25, col.a()), 60);
			g_Render->CircleFilled(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, 21, Color(25, 25, 25, total_alpha), 60);
		//g_Render->CircleFilled(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, 21, Color(200, 25, 25, alpha_damage), 60);
		g_Render->AddRadialGradient(window->DrawList, ImVec2(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12), 26, ImColor(255, 0, 0, alpha_damage), ImColor(0, 0, 0, 0));
		//g_Render->two_sided_arc(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, 20, percent, Color(col.r(), col.g(), col.b(), 255), 3);
		g_Render->two_sided_arc(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, 20, percent, Color(col.r(), col.g(), col.b(), total_alpha), 3);
		//g_Render->DrawString(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, Color::White, HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().weapon_icons2, index_to_grenade_name_icon(m_index));
		g_Render->DrawString(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12, Color(255, 255, 255, total_alpha), HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().timersz, index_to_grenade_name_icon(m_index));
		//drawwindow->AddImage(c_menu::get().inferno, ImVec2(saddadsadadasdasda.x, saddadsadadasdasda.y - g_size.y * 0.5f - 12), ImVec2(saddadsadadasdasda.x + 10, saddadsadadasdasda.y), ImVec2(0, 0), ImVec2(4, 4), ImColor(255, 255, 255));
	}
	else {

		static auto g_size = Vector2D(35.0f, 5.0f);
		g_Render->CircleFilled(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 12, 21, Color(25, 25, 25, total_alpha), 60);
		//g_Render->CircleFilled(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 12, 21, Color(200, 25, 25, alpha_damage), 60);
		g_Render->AddRadialGradient(window->DrawList, ImVec2(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 12), 26, ImColor(255, 0, 0, alpha_damage), ImColor(0, 0, 0, 0));
		g_Render->two_sided_arc(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 12, 20, percent, Color(col.r(), col.g(), col.b(), total_alpha), 3);
		g_Render->DrawString(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 12, Color(255, 255, 255, total_alpha), HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().weapon_icons2, index_to_grenade_name_icon(m_index));
		//std::string dmg = "-"; dmg += std::to_string(int(local_damage));
		//g_Render->DrawString(prev_screen.x, prev_screen.y - g_size.y * 0.5f - 15, Color(255, 255, 255, 125), HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().verdana, "-%i", int(local_damage));
	}


	auto isOnScreen = [](Vector origin, Vector& screen) -> bool
	{
		if (!math::world_to_screen(origin, screen))
			return false;

		static int iScreenWidth, iScreenHeight;
		m_engine()->GetScreenSize(iScreenWidth, iScreenHeight);

		auto xOk = iScreenWidth > screen.x;
		auto yOk = iScreenHeight > screen.y;

		return xOk && yOk;
	};

	Vector screenPos;
	Vector vEnemyOrigin = m_origin;
	Vector vLocalOrigin = g_ctx.local()->GetAbsOrigin();

	if (!g_ctx.local()->is_alive())
		vLocalOrigin = m_input()->m_vecCameraOffset;

	Vector viewAngles;
	m_engine()->GetViewAngles(viewAngles);

	static int width, height;
	m_engine()->GetScreenSize(width, height);
	if (g_cfg.esp.offscreen_proximity && !isOnScreen(vEnemyOrigin, screenPos))
	{
		auto screenCenter = Vector2D(width * 0.5f, height * 0.5f);
		auto angleYawRad = DEG2RAD(viewAngles.y - math::calculate_angle(g_ctx.globals.eye_pos, vEnemyOrigin).y - 90.0f);

		auto radius = 80 - 70 * float(alpha_damage / 255.f);
		auto size = 6 + 6 * float(alpha_damage / 255.f);

		auto newPointX = screenCenter.x + ((((width - (size * 3)) * 0.5f) * (radius / 100.0f)) * cos(angleYawRad)) + (int)(6.0f * (((float)size - 4.0f) / 16.0f));
		auto newPointY = screenCenter.y + ((((height - (size * 3)) * 0.5f) * (radius / 100.0f)) * sin(angleYawRad));
		auto newPointX2 = screenCenter.x + ((((width - (size * 15)) * 0.5f) * (radius / 100.0f)) * cos(angleYawRad)) + (int)(6.0f * (((float)size - 4.0f) / 16.0f));
		auto newPointY2 = screenCenter.y + ((((height - (size * 15)) * 0.5f) * (radius / 100.0f)) * sin(angleYawRad));
		std::array <Vector2D, 3> points
		{
			Vector2D(newPointX - size, newPointY - size),
				Vector2D(newPointX + size, newPointY),
				Vector2D(newPointX - size, newPointY + size)
		};

		auto warn = Vector2D(newPointX2, newPointY2);
		math::rotate_triangle(points, viewAngles.y - math::calculate_angle(g_ctx.globals.eye_pos, vEnemyOrigin).y - 90.0f);

		g_Render->CircleFilled(warn.x, warn.y - (size * 3) * 0.5f, 21, Color(25, 25, 25, total_alpha), 60);
		//g_Render->CircleFilled(warn.x, warn.y - (size * 3) * 0.5f, 21, Color(200, 25, 25, alpha_damage), 60);
		g_Render->AddRadialGradient(window->DrawList, ImVec2(warn.x, warn.y - (size * 3) * 0.5f), 26, ImColor(255, 0, 0, alpha_damage), ImColor(0, 0, 0, 0));
		g_Render->two_sided_arc(warn.x, warn.y - (size * 3) * 0.5f, 20, percent, Color(col.r(), col.g(), col.b(), total_alpha), 3);
		if (trace.fraction < 1.0f && m_index == 46)
			g_Render->DrawString(warn.x, warn.y - (size * 3) * 0.5f, Color(255, 255, 255, total_alpha), HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().timersz, index_to_grenade_name_icon(m_index));
		else
			g_Render->DrawString(warn.x, warn.y - (size * 3) * 0.5f, Color(255, 255, 255, total_alpha), HFONT_CENTERED_X | HFONT_CENTERED_Y, c_menu::get().weapon_icons2, index_to_grenade_name_icon(m_index));
		g_Render->TriangleFilled(points.at(0).x, points.at(0).y, points.at(1).x, points.at(1).y, points.at(2).x, points.at(2).y, Color(25, 25, 25, 150));
		g_Render->Triangle(points.at(0).x, points.at(0).y, points.at(1).x, points.at(1).y, points.at(2).x, points.at(2).y, Color(col.r(), col.g(), col.b(), 255));
	}
	return true;
}

void c_grenade_prediction::grenade_warning(projectile_t* entity)
{
	auto& predicted_nades = c_grenade_prediction::get().get_list();

	static auto last_server_tick = m_globals()->m_curtime;
	if (last_server_tick != m_globals()->m_curtime) {
		predicted_nades.clear();

		last_server_tick = m_globals()->m_curtime;
	}

	if (entity->IsDormant() || !g_cfg.esp.grenade_proximity_warning)
		return;

	const auto client_class = entity->GetClientClass();
	if (!client_class
		|| client_class->m_ClassID != 114 && client_class->m_ClassID != CBaseCSGrenadeProjectile)
		return;

	if (client_class->m_ClassID == CBaseCSGrenadeProjectile) {
		const auto model = entity->GetModel();
		if (!model)
			return;

		const auto studio_model = m_modelinfo()->GetStudioModel(model);
		if (!studio_model
			|| std::string_view(studio_model->szName).find("fraggrenade") == std::string::npos)
			return;
	}

	const auto handle = entity->GetRefEHandle().ToLong();

	if (entity->m_nExplodeEffectTickBegin()) {
		predicted_nades.erase(handle);

		return;
	}

	if (predicted_nades.find(handle) == predicted_nades.end()) {
		predicted_nades.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(handle),
			std::forward_as_tuple(
				entity->m_hThrower().Get(),
				client_class->m_ClassID == 114 ? WEAPON_MOLOTOV : WEAPON_HEGRENADE,
				entity->m_vecOrigin(), reinterpret_cast<player_t*>(entity)->m_vecVelocity(),
				entity->m_flSpawnTime(), TIME_TO_TICKS(reinterpret_cast<player_t*>(entity)->m_flSimulationTime() - entity->m_flSpawnTime())
			)
		);
	}

	if (predicted_nades.at(handle).draw())
		return;

	predicted_nades.erase(handle);
}