// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "aim.h"
#include "..\..\misc\misc.h"
#include "..\..\misc\logs.h"
#include "..\autowall\autowall.h"
#include "..\autowall\autowall2.h"
#include "..\..\misc\animation fix\prediction_system.h"
#include "..\anti-aim\slowwalk.h"
#include "..\..\misc\animation fix\local_animations.h"

void aim::run(CUserCmd* cmd)
{
	backup.clear();
	targets.clear();
	scanned_targets.clear();
	final_target.reset();
	should_stop = false;

	if (!g_cfg.ragebot.enable)
		return;

	automatic_revolver(cmd);
	prepare_targets();

	if (g_ctx.globals.weapon->is_non_aim())
		return;

	if (g_ctx.globals.current_weapon == -1)
		return;

	scan_targets();

	if (!should_stop && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_PREDICTIVE]) {
		auto max_speed = 260.0f;
		auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();

		if (weapon_info)
			max_speed = g_ctx.globals.scoped ? weapon_info->flMaxPlayerSpeedAlt : weapon_info->flMaxPlayerSpeed;

		auto ticks_to_stop = math::clamp(engineprediction::get().backup_data.velocity.Length2D() / max_speed * 3.0f, 0.0f, 4.0f);
		auto predicted_eye_pos = g_ctx.globals.eye_pos + engineprediction::get().backup_data.velocity * m_globals()->m_intervalpertick * ticks_to_stop;

		for (auto& target : targets)
		{
			if (!target.last_record->valid())
				continue;

			scan_data last_data;

			target.last_record->adjust_player();
			scan(target.last_record, last_data, predicted_eye_pos);

			if (!last_data.valid())
				continue;

			should_stop = GetTicksToShoot() <= GetTicksToStop() || g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[1] && !g_ctx.globals.weapon->can_fire(true);
			break;
		}
	}

	if (!automatic_stop(cmd))
		return;

	if (scanned_targets.empty())
		return;

	find_best_target();

	if (!final_target.data.valid())
		return;

	fire(cmd);
}

void aim::automatic_revolver(CUserCmd* cmd)
{
	if (!m_engine()->IsActiveApp())
		return;

	if (g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER)
		return;

	if (cmd->m_buttons & IN_ATTACK)
		return;

	cmd->m_buttons &= ~IN_ATTACK2;

	static auto r8cock_time = 0.0f;
	auto server_time = TICKS_TO_TIME(g_ctx.globals.backup_tickbase);

	if (g_ctx.globals.weapon->can_fire(false))
	{
		if (r8cock_time <= server_time) //-V807
		{
			if (g_ctx.globals.weapon->m_flNextSecondaryAttack() <= server_time)
				r8cock_time = server_time + 0.234375f;
			else
				cmd->m_buttons |= IN_ATTACK2;
		}
		else
			cmd->m_buttons |= IN_ATTACK;
	}
	else
	{
		r8cock_time = server_time + 0.234375f;
		cmd->m_buttons &= ~IN_ATTACK;
	}

	g_ctx.globals.revolver_working = true;
}

void aim::prepare_targets()
{
	for (auto i = 1; i < m_globals()->m_maxclients; i++)
	{
		if (g_cfg.player_list.white_list[i])
			continue;

		auto e = (player_t*)m_entitylist()->GetClientEntity(i);

		if (!e->valid(true, false))
			continue;

		// todo. extrapolate

		auto records = &player_records[i]; //-V826

		if (records->empty())
			continue;

		targets.emplace_back(target(e, get_record(records, false), get_record(records, true)));
	}

	if (targets.size() >= 4)
	{
		auto first = rand() % targets.size();
		auto second = rand() % targets.size();
		auto third = rand() % targets.size();

		for (auto i = 0; i < targets.size(); ++i)
		{
			if (i == first || i == second || i == third)
				continue;

			targets.erase(targets.begin() + i);

			if (i > 0)
				--i;
		}
	}


	for (auto& target : targets)
		backup.emplace_back(adjust_data(target.e));
}

static bool compare_records(const optimized_adjust_data& first, const optimized_adjust_data& second)
{
	auto first_pitch = math::normalize_pitch(first.angles.x);
	auto second_pitch = math::normalize_pitch(second.angles.x);

	if (fabs(first_pitch - second_pitch) > 15.0f)
		return fabs(first_pitch) < fabs(second_pitch);
	else if (first.duck_amount != second.duck_amount) //-V550
		return first.duck_amount < second.duck_amount;
	else if (first.origin != second.origin)
		return first.origin.DistTo(g_ctx.local()->GetAbsOrigin()) < second.origin.DistTo(g_ctx.local()->GetAbsOrigin());

	return first.simulation_time > second.simulation_time;
}

adjust_data* aim::get_record(std::deque <adjust_data>* records, bool history)
{
	if (history)
	{
		std::deque <optimized_adjust_data> optimized_records; //-V826

		for (auto i = 0; i < records->size(); ++i)
		{
			auto record = &records->at(i);
			optimized_adjust_data optimized_record;

			optimized_record.i = i;
			optimized_record.player = record->player;
			optimized_record.simulation_time = record->simulation_time;
			optimized_record.duck_amount = record->duck_amount;
			optimized_record.angles = record->angles;
			optimized_record.origin = record->origin;

			optimized_records.emplace_back(optimized_record);
		}

		if (optimized_records.size() < 2)
			return nullptr;

		std::sort(optimized_records.begin(), optimized_records.end(), compare_records);

		for (auto& optimized_record : optimized_records)
		{
			auto record = &records->at(optimized_record.i);

			if (!record->valid())
				continue;

			return record;
		}
	}
	else
	{
		for (auto i = 0; i < records->size(); ++i)
		{
			auto record = &records->at(i);

			if (!record->valid())
				continue;

			return record;
		}
	}

	return nullptr;
}

int aim::get_minimum_damage(bool visible, int health)
{
	auto minimum_damage = 1;

	if (visible)
	{
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage > 100)
			minimum_damage = health + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage - 100;
		else
			minimum_damage = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage, 1, health);
	}
	else
	{
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage > 100)
			minimum_damage = health + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage - 100;
		else
			minimum_damage = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage, 1, health);
	}

	if (key_binds::get().get_key_bind_state(4 + g_ctx.globals.current_weapon))
	{
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage > 100)
			minimum_damage = health + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage - 100;
		else
			minimum_damage = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage, 1, health);
	}

	return minimum_damage;
}
void aim::scan_targets()
{
	if (targets.empty())
		return;

	for (auto& target : targets)
	{
		if (target.history_record->valid())
		{
			scan_data last_data;

			if (target.last_record->valid())
			{
				target.last_record->adjust_player();
				scan(target.last_record, last_data);
			}

			scan_data history_data;

			target.history_record->adjust_player();
			scan(target.history_record, history_data);

			if (last_data.valid() && last_data.damage > history_data.damage)
				scanned_targets.emplace_back(scanned_target(target.last_record, last_data));
			else if (history_data.valid())
				scanned_targets.emplace_back(scanned_target(target.history_record, history_data));
		}
		else
		{
			if (!target.last_record->valid())
				continue;

			scan_data last_data;

			target.last_record->adjust_player();
			scan(target.last_record, last_data);

			if (!last_data.valid())
				continue;

			scanned_targets.emplace_back(scanned_target(target.last_record, last_data));
		}
	}
}

bool aim::automatic_stop(CUserCmd* cmd)
{
	if (!should_stop)
		return true;

	if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop)
		return true;

	if (g_ctx.globals.slowwalking)
		return true;

	if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND && engineprediction::get().backup_data.flags & FL_ONGROUND)) //-V807
		return true;

	if (g_ctx.globals.weapon->is_empty())
		return true;

	if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_BETWEEN_SHOTS] && !g_ctx.globals.weapon->can_fire(false))
		return true;

	auto animlayer = g_ctx.local()->get_animlayers()[1];

	if (animlayer.m_nSequence)
	{
		auto activity = g_ctx.local()->sequence_activity(animlayer.m_nSequence);

		if (activity == ACT_CSGO_RELOAD && animlayer.m_flWeight > 0.0f)
			return true;
	}

	auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();

	if (!weapon_info)
		return true;

	auto max_speed = 0.33f * (g_ctx.globals.scoped ? weapon_info->flMaxPlayerSpeedAlt : weapon_info->flMaxPlayerSpeed);

	if (engineprediction::get().backup_data.velocity.Length2D() < max_speed)
		slowwalk::get().create_move(cmd);
	else
	{
		Vector direction;
		Vector real_view;

		math::vector_angles(engineprediction::get().backup_data.velocity, direction);
		m_engine()->GetViewAngles(real_view);

		direction.y = real_view.y - direction.y;

		Vector forward;
		math::angle_vectors(direction, forward);

		static auto cl_forwardspeed = m_cvar()->FindVar(crypt_str("cl_forwardspeed"));
		static auto cl_sidespeed = m_cvar()->FindVar(crypt_str("cl_sidespeed"));

		auto negative_forward_speed = -cl_forwardspeed->GetFloat();
		auto negative_side_speed = -cl_sidespeed->GetFloat();

		auto negative_forward_direction = forward * negative_forward_speed;
		auto negative_side_direction = forward * negative_side_speed;

		cmd->m_forwardmove = negative_forward_direction.x;
		cmd->m_sidemove = negative_side_direction.y;

		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_FORCE_ACCURACY])
			return false;
	}

	return true;
}

static bool compare_points(const scan_point& first, const scan_point& second)
{
	return !first.center && first.hitbox == second.hitbox;
}

int aim::GetTicksToShoot() {
	if (g_ctx.globals.weapon->can_fire(true))
		return -1;

	auto flServerTime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);
	auto flNextPrimaryAttack = g_ctx.local()->m_hActiveWeapon()->m_flNextPrimaryAttack();

	return TIME_TO_TICKS(fabsf(flNextPrimaryAttack - flServerTime));
}

int aim::GetTicksToStop() {
	static auto predict_velocity = [](Vector* velocity)
	{
		float speed = velocity->Length2D();
		static auto sv_friction = m_cvar()->FindVar("sv_friction");
		static auto sv_stopspeed = m_cvar()->FindVar("sv_stopspeed");

		if (speed >= 1.f)
		{
			float friction = sv_friction->GetFloat();
			float stop_speed = std::max< float >(speed, sv_stopspeed->GetFloat());
			float time = std::max< float >(m_globals()->m_intervalpertick, m_globals()->m_frametime);
			*velocity *= std::max< float >(0.f, speed - friction * stop_speed * time / speed);
		}
	};
	Vector vel = g_ctx.local()->m_vecVelocity();
	int ticks_to_stop = 0;
	for (;;)
	{
		if (vel.Length2D() < 1.f)
			break;
		predict_velocity(&vel);
		ticks_to_stop++;
	}
	return ticks_to_stop;
}


int aim::GetDamage(int health) {
	int bullets = g_ctx.globals.weapon->m_iClip1();
	auto minimum_damage = key_binds::get().get_key_bind_state(4 + g_ctx.globals.current_weapon) ? g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage : g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage;

	if (bullets == 1) {
		minimum_damage = health + 1;
		return minimum_damage;
	}

	if (minimum_damage > 100)
		minimum_damage = health + minimum_damage - 100;
	else if (minimum_damage > health)
		minimum_damage = health + 1;

	return minimum_damage;
}

void aim::scan(adjust_data* record, scan_data& data, const Vector& shoot_position, bool optimized)
{
	if (!g_ctx.globals.weapon)
		return;

	auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();

	if (!weapon_info)
		return;

	auto hitboxes = get_hitboxes(record);

	if (hitboxes.empty())
		return;

	auto force_safe_points = key_binds::get().get_key_bind_state(3) || g_cfg.player_list.force_safe_points[record->i];
	auto best_damage = 0;

	auto minimum_damage = GetDamage(record->player->m_iHealth());
	auto minimum_visible_damage = GetDamage(record->player->m_iHealth());

	auto get_hitgroup = [](const int& hitbox)
	{
		if (hitbox == HITBOX_HEAD)
			return 0;
		else if (hitbox == HITBOX_PELVIS)
			return 1;
		else if (hitbox == HITBOX_STOMACH)
			return 2;
		else if (hitbox >= HITBOX_LOWER_CHEST && hitbox <= HITBOX_UPPER_CHEST)
			return 3;
		else if (hitbox >= HITBOX_RIGHT_THIGH && hitbox <= HITBOX_LEFT_FOOT)
			return 4;
		else if (hitbox >= HITBOX_RIGHT_HAND && hitbox <= HITBOX_LEFT_FOREARM)
			return 5;

		return -1;
	};

	std::vector <scan_point> points;
	auto start = g_ctx.local()->get_shoot_position();
	for (auto& hitbox : hitboxes)
	{
		auto current_points = get_points(record, hitbox);

		for (auto& point : current_points)
		{
			//point.safe = (hitbox_intersection(record->player, record->matrixes_data.main, hitbox, shoot_position, point.point) && hitbox_intersection(record->player, record->matrixes_data.first, hitbox, shoot_position, point.point) && hitbox_intersection(record->player, record->matrixes_data.second, hitbox, shoot_position, point.point));
			//point.safe = IsSafePoint(record, shoot_position, point.point, hitbox);
			if (!force_safe_points || point.safe)
				points.emplace_back(point);
		}
	}

	for (auto& point : points)
	{
		if (points.empty())
			return;

		if (point.hitbox == HITBOX_HEAD)
			continue;

		for (auto it = points.begin(); it != points.end(); ++it)
		{
			if (point.point == it->point)
				continue;

			auto first_angle = math::calculate_angle(shoot_position, point.point);
			auto second_angle = math::calculate_angle(shoot_position, it->point);

			auto distance = shoot_position.DistTo(point.point);
			auto fov = math::fast_sin(DEG2RAD(math::get_fov(first_angle, second_angle))) * distance;

			if (fov < 5.0f)
			{
				points.erase(it);
				break;
			}
		}
	}

	if (points.empty())
		return;

	auto body_hitboxes = true;

	for (auto& point : points)
	{
		if (body_hitboxes && (point.hitbox < HITBOX_PELVIS || point.hitbox > HITBOX_UPPER_CHEST))
		{
			body_hitboxes = false;

			if (g_cfg.player_list.force_body_aim[record->i])
				break;

			if (key_binds::get().get_key_bind_state(22))
				break;

			if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim && best_damage >= 1)
				break;
		}

		FireBulletData_t fire_data = { };

		fire_data.damage = CAutoWall::GetDamage(shoot_position, g_ctx.local(), point.point, &fire_data);

		//auto fire_data = autowall::get().wall_penetration(shoot_position, point.point, record->player);

		//if (!fire_data.valid)
			//continue;

		if (fire_data.damage < 1)
			continue;

		if (!fire_data.visible && !g_cfg.ragebot.autowall)
			continue;

		if (get_hitgroup(fire_data.hitbox) != get_hitgroup(point.hitbox))
			continue;

		if (force_safe_points && !point.safe)
			continue;

		auto current_minimum_damage = fire_data.visible ? minimum_visible_damage : minimum_damage;



		if (fire_data.damage >= current_minimum_damage && fire_data.damage >= best_damage)
		{
			should_stop = GetTicksToShoot() <= GetTicksToStop() || g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers.at(1) && !g_ctx.globals.weapon->can_fire(true);


			if (((record->flags & FL_ONGROUND && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points && point.safe) || (record->flags & FL_ONGROUND && point.hitbox >= HITBOX_PELVIS && point.hitbox <= HITBOX_UPPER_CHEST) || ((!(record->flags & FL_ONGROUND) || record->shot) && point.hitbox == HITBOX_HEAD)) && fire_data.damage >= record->player->m_iHealth())
			{
				best_damage = fire_data.damage;

				data.point = point;
				data.visible = fire_data.visible;
				data.damage = fire_data.damage;
				data.hitbox = fire_data.hitbox;
				return;
			}
			else
			{
				best_damage = fire_data.damage;

				data.point = point;
				data.visible = fire_data.visible;
				data.damage = fire_data.damage;
				data.hitbox = fire_data.hitbox;
			}
		}

		//
		else if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points) {
			if (point.safe && point.hitbox >= HITBOX_PELVIS && point.hitbox <= HITBOX_UPPER_CHEST && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim) {
				if ((fire_data.damage > best_damage && body_hitboxes != point.hitbox) || fire_data.damage > best_damage + 20.f) {
					if (fire_data.damage >= minimum_damage)
					{
						best_damage = best_damage = fire_data.damage;
						data.point = point;
						data.visible = fire_data.visible;
						data.damage = fire_data.damage;
						data.hitbox = fire_data.hitbox;
						body_hitboxes = point.hitbox;
					}
				}
			}
		}
		else if (point.hitbox >= HITBOX_PELVIS && point.hitbox <= HITBOX_UPPER_CHEST && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim) {
			if ((fire_data.damage > best_damage && body_hitboxes != point.hitbox) || fire_data.damage > best_damage + 20.f) {
				if (fire_data.damage >= minimum_damage)
				{
					best_damage = best_damage = fire_data.damage;
					data.point = point;
					data.visible = fire_data.visible;
					data.damage = fire_data.damage;
					data.hitbox = fire_data.hitbox;
					body_hitboxes = point.hitbox;
				}
			}
		}//
	}
}

std::vector <int> aim::get_hitboxes(adjust_data* record, bool optimized)
{
	std::vector <int> hitboxes; //-V827

	if (optimized)
	{
		hitboxes.emplace_back(HITBOX_HEAD);
		hitboxes.emplace_back(HITBOX_CHEST);
		hitboxes.emplace_back(HITBOX_STOMACH);
		hitboxes.emplace_back(HITBOX_PELVIS);

		return hitboxes;
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(1))
		hitboxes.emplace_back(HITBOX_UPPER_CHEST);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(2))
		hitboxes.emplace_back(HITBOX_CHEST);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(3))
		hitboxes.emplace_back(HITBOX_LOWER_CHEST);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(4))
		hitboxes.emplace_back(HITBOX_STOMACH);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(5))
		hitboxes.emplace_back(HITBOX_PELVIS);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(0))
		hitboxes.emplace_back(HITBOX_HEAD);

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(6))
	{
		hitboxes.emplace_back(HITBOX_RIGHT_UPPER_ARM);
		hitboxes.emplace_back(HITBOX_LEFT_UPPER_ARM);
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(7))
	{
		hitboxes.emplace_back(HITBOX_RIGHT_THIGH);
		hitboxes.emplace_back(HITBOX_LEFT_THIGH);

		hitboxes.emplace_back(HITBOX_RIGHT_CALF);
		hitboxes.emplace_back(HITBOX_LEFT_CALF);
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(8))
	{
		hitboxes.emplace_back(HITBOX_RIGHT_FOOT);
		hitboxes.emplace_back(HITBOX_LEFT_FOOT);
	}

	return hitboxes;
}

std::vector <scan_point> aim::get_points(adjust_data* record, int hitbox, bool from_aim)
{
	std::vector <scan_point> points; //-V827
	auto model = record->player->GetModel();

	if (!model)
		return points;

	auto hdr = m_modelinfo()->GetStudioModel(model);

	if (!hdr)
		return points;

	auto set = hdr->pHitboxSet(record->player->m_nHitboxSet());

	if (!set)
		return points;

	auto bbox = set->pHitbox(hitbox);

	if (!bbox)
		return points;

	auto center = (bbox->bbmin + bbox->bbmax) * 0.5f;

	if (bbox->radius <= 0.0f)
	{
		auto rotation_matrix = math::angle_matrix(bbox->rotation);

		matrix3x4_t matrix;
		math::concat_transforms(record->matrixes_data.main[bbox->bone], rotation_matrix, matrix);

		auto origin = matrix.GetOrigin();

		if (hitbox == HITBOX_RIGHT_FOOT || hitbox == HITBOX_LEFT_FOOT)
		{
			auto side = (bbox->bbmin.z - center.z) * 0.875f;

			if (hitbox == HITBOX_LEFT_FOOT)
				side = -side;

			points.emplace_back(scan_point(Vector(center.x, center.y, center.z + side), hitbox, true));

			auto min = (bbox->bbmin.x - center.x) * 0.875f;
			auto max = (bbox->bbmax.x - center.x) * 0.875f;

			points.emplace_back(scan_point(Vector(center.x + min, center.y, center.z), hitbox, false));
			points.emplace_back(scan_point(Vector(center.x + max, center.y, center.z), hitbox, false));
		}
	}
	else
	{
		auto scale = 0.0f;

		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].static_point_scale)
		{
			if (hitbox == HITBOX_HEAD)
				scale = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].head_scale;
			else
				scale = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].body_scale;
		}
		else
		{
			auto transformed_center = center;
			math::vector_transform(transformed_center, record->matrixes_data.main[bbox->bone], transformed_center);

			auto spread = g_ctx.globals.spread + g_ctx.globals.inaccuracy;
			auto distance = transformed_center.DistTo(g_ctx.globals.eye_pos);

			distance /= math::fast_sin(DEG2RAD(90.0f - RAD2DEG(spread)));
			spread = math::fast_sin(spread);

			auto radius = max(bbox->radius - distance * spread, 0.0f);
			scale = math::clamp(radius / bbox->radius, 0.0f, 1.0f);
		}

		if (scale <= 0.0f) //-V648
		{
			math::vector_transform(center, record->matrixes_data.main[bbox->bone], center);
			points.emplace_back(scan_point(center, hitbox, true));

			return points;
		}

		auto final_radius = bbox->radius * scale;

		if (hitbox == HITBOX_HEAD)
		{
			auto pitch_down = math::normalize_pitch(record->angles.x) > 85.0f;
			auto backward = fabs(math::normalize_yaw(record->angles.y - math::calculate_angle(record->player->get_shoot_position(), g_ctx.local()->GetAbsOrigin()).y)) > 120.0f;

			points.emplace_back(scan_point(center, hitbox, !pitch_down || !backward));

			points.emplace_back(scan_point(Vector(bbox->bbmax.x + 0.70710678f * final_radius, bbox->bbmax.y - 0.70710678f * final_radius, bbox->bbmax.z), hitbox, false));
			points.emplace_back(scan_point(Vector(bbox->bbmax.x, bbox->bbmax.y, bbox->bbmax.z + final_radius), hitbox, false));
			points.emplace_back(scan_point(Vector(bbox->bbmax.x, bbox->bbmax.y, bbox->bbmax.z - final_radius), hitbox, false));

			points.emplace_back(scan_point(Vector(bbox->bbmax.x, bbox->bbmax.y - final_radius, bbox->bbmax.z), hitbox, false));

			if (pitch_down && backward)
				points.emplace_back(scan_point(Vector(bbox->bbmax.x - final_radius, bbox->bbmax.y, bbox->bbmax.z), hitbox, false));
		}
		else if (hitbox >= HITBOX_PELVIS && hitbox <= HITBOX_UPPER_CHEST)
		{
			points.emplace_back(scan_point(center, hitbox, true));

			points.emplace_back(scan_point(Vector(bbox->bbmax.x, bbox->bbmax.y, bbox->bbmax.z + final_radius), hitbox, false));
			points.emplace_back(scan_point(Vector(bbox->bbmax.x, bbox->bbmax.y, bbox->bbmax.z - final_radius), hitbox, false));

			points.emplace_back(scan_point(Vector(center.x, bbox->bbmax.y - final_radius, center.z), hitbox, true));
		}
		else if (hitbox == HITBOX_RIGHT_CALF || hitbox == HITBOX_LEFT_CALF)
		{
			points.emplace_back(scan_point(center, hitbox, true));
			points.emplace_back(scan_point(Vector(bbox->bbmax.x - final_radius, bbox->bbmax.y, bbox->bbmax.z), hitbox, false));
		}
		else if (hitbox == HITBOX_RIGHT_THIGH || hitbox == HITBOX_LEFT_THIGH)
			points.emplace_back(scan_point(center, hitbox, true));
		else if (hitbox == HITBOX_RIGHT_UPPER_ARM || hitbox == HITBOX_LEFT_UPPER_ARM)
		{
			points.emplace_back(scan_point(center, hitbox, true));
			points.emplace_back(scan_point(Vector(bbox->bbmax.x + final_radius, center.y, center.z), hitbox, false));
		}
	}

	for (auto& point : points)
		math::vector_transform(point.point, record->matrixes_data.main[bbox->bone], point.point);

	return points;
}

static bool compare_targets(const scanned_target& first, const scanned_target& second)
{
	if (g_cfg.player_list.high_priority[first.record->i] != g_cfg.player_list.high_priority[second.record->i])
		return g_cfg.player_list.high_priority[first.record->i];

	switch (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].selection_type)
	{
	case 1:
		return first.fov < second.fov;
	case 2:
		return first.distance < second.distance;
	case 3:
		return first.health < second.health;
	case 4:
		return first.data.damage > second.data.damage;
	}

	return false;
}

void aim::find_best_target()
{
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].selection_type)
		std::sort(scanned_targets.begin(), scanned_targets.end(), compare_targets);

	for (auto& target : scanned_targets)
	{
		if (target.fov > (float)g_cfg.ragebot.field_of_view)
			continue;

		final_target = target;
		final_target.record->adjust_player();
		break;
	}
}

/*static auto get_resolver_type = [](resolver_type type) -> std::string
{
	switch (type)
	{
	case ORIGINAL:
		return "original ";
	case BRUTEFORCE:
		return "bruteforce ";
	case LBY:
		return "lby ";
	case LAYERS:
		return "layers ";
	case DIRECTIONAL:
		return "directional ";
	case TRACE:
		return "trace ";
	}
};*/


void aim::fire(CUserCmd* cmd)
{
	if (!g_ctx.globals.weapon->can_fire(true))
		return;

	auto aim_angle = math::calculate_angle(g_ctx.globals.eye_pos, final_target.data.point.point).Clamp();

	if (!g_cfg.ragebot.silent_aim)
		m_engine()->SetViewAngles(aim_angle);

	if (!g_cfg.ragebot.autoshoot && !(cmd->m_buttons & IN_ATTACK))
		return;

	/*auto final_hitchance = 0;
	auto hitchance_amount = 0;

	if (g_ctx.globals.double_tap_aim && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].double_tap_hitchance)
		hitchance_amount = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].double_tap_hitchance_amount;
	else if (!g_ctx.globals.double_tap_aim && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance)
		hitchance_amount = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;

	if (hitchance_amount)
	{
		if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_SSG08 && !(g_ctx.local()->m_fFlags() & FL_ONGROUND) && !(engineprediction::get().backup_data.flags & FL_ONGROUND) && fabs(engineprediction::get().backup_data.velocity.z) < 5.0f && engineprediction::get().backup_data.velocity.Length2D() < 5.0f) //-V807
			final_hitchance = 101;
		else
			final_hitchance = hitchance(aim_angle);

		if (final_hitchance < hitchance_amount)
		{
			auto is_zoomable_weapon = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_SCAR20 || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_G3SG1 || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_SSG08 || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_AWP || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_AUG || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_SG553;

			if (g_cfg.ragebot.autoscope && is_zoomable_weapon && !g_ctx.globals.weapon->m_zoomLevel())
				cmd->m_buttons |= IN_ATTACK2;

			return;
		}
	}*/

	auto backtrack_ticks = 0;
	auto net_channel_info = m_engine()->GetNetChannelInfo();

	if (net_channel_info)
	{
		auto original_tickbase = g_ctx.globals.backup_tickbase;

		if (misc::get().double_tap_enabled && misc::get().double_tap_key)
			original_tickbase = g_ctx.globals.backup_tickbase + g_ctx.globals.weapon->get_max_tickbase_shift();

		static auto sv_maxunlag = m_cvar()->FindVar(crypt_str("sv_maxunlag"));

		auto correct = math::clamp(net_channel_info->GetLatency(FLOW_OUTGOING) + net_channel_info->GetLatency(FLOW_INCOMING) + util::get_interpolation(), 0.0f, sv_maxunlag->GetFloat());
		auto delta_time = correct - (TICKS_TO_TIME(original_tickbase) - final_target.record->simulation_time);

		backtrack_ticks = TIME_TO_TICKS(fabs(delta_time));
	}

	static auto get_hitbox_name = [](int hitbox, bool shot_info = false) -> std::string
	{
		switch (hitbox)
		{
		case HITBOX_HEAD:
			return shot_info ? crypt_str("Head") : crypt_str("head");
		case HITBOX_LOWER_CHEST:
			return shot_info ? crypt_str("Lower chest") : crypt_str("lower chest");
		case HITBOX_CHEST:
			return shot_info ? crypt_str("Chest") : crypt_str("chest");
		case HITBOX_UPPER_CHEST:
			return shot_info ? crypt_str("Upper chest") : crypt_str("upper chest");
		case HITBOX_STOMACH:
			return shot_info ? crypt_str("Stomach") : crypt_str("stomach");
		case HITBOX_PELVIS:
			return shot_info ? crypt_str("Pelvis") : crypt_str("pelvis");
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
		case HITBOX_RIGHT_HAND:
			return shot_info ? crypt_str("Left arm") : crypt_str("left arm");
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
		case HITBOX_LEFT_HAND:
			return shot_info ? crypt_str("Right arm") : crypt_str("right arm");
		case HITBOX_RIGHT_THIGH:
		case HITBOX_RIGHT_CALF:
			return shot_info ? crypt_str("Left leg") : crypt_str("left leg");
		case HITBOX_LEFT_THIGH:
		case HITBOX_LEFT_CALF:
			return shot_info ? crypt_str("Right leg") : crypt_str("right leg");
		case HITBOX_RIGHT_FOOT:
			return shot_info ? crypt_str("Left foot") : crypt_str("left foot");
		case HITBOX_LEFT_FOOT:
			return shot_info ? crypt_str("Right foot") : crypt_str("right foot");
		}
	};

	/*
	
	ORIGINAL,
	BRUTEFORCE,
	LBY,
	LAYERS,
	TRACE,
	DIRECTIONAL
	
	*/

	auto hitchance_amount = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;
	// @note: in next update :)
	//bool shifted_recent = (m_globals()->m_realtime - lastshifttime) < 0.25f;
	//if (/*shifted_recent && UseDoubleTapHitchance() && */ g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].double_tap_hitchance)
	//	hitchance_amount = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].double_tap_hitchance_amount;

	bool Hitchance_Valid = g_hit_chance->can_hit(final_target.record, g_ctx.globals.weapon, aim_angle, final_target.data.hitbox);


	if (!Hitchance_Valid)
	{
		if (g_cfg.ragebot.autoscope && g_ctx.local()->m_hActiveWeapon()->is_sniper() && !g_ctx.local()->m_hActiveWeapon()->m_zoomLevel())
			cmd->m_buttons |= IN_ATTACK2;

		return;
	}

	player_info_t player_info;
	m_engine()->GetPlayerInfo(final_target.record->i, &player_info);

#if BETA
	std::stringstream log;

	/*log << crypt_str("fired shot at ") + (std::string)player_info.szName + crypt_str(": ");
	log << crypt_str("hitchance: ") + (hitchance_amount == 101 ? crypt_str("NA") : std::to_string(hitchance_amount)) + crypt_str(", ");
	log << crypt_str("hitbox: ") + get_hitbox_name(final_target.data.hitbox) + crypt_str(", ");
	log << crypt_str("damage: ") + std::to_string(final_target.data.damage) + crypt_str(", ");
	log << crypt_str("safe: ") + std::to_string((bool)final_target.data.point.safe) + crypt_str(", ");
	log << crypt_str("backtrack: ") + std::to_string(backtrack_ticks) + crypt_str(", ");*/

	//std::stringstream log;

	log << crypt_str("[~] ") + (std::string)player_info.szName + crypt_str(" | ");
	log << crypt_str("hc: [") + (hitchance_amount == 101 ? crypt_str("error hc") : std::to_string(hitchance_amount)) + crypt_str("] | ");
	log << crypt_str("[") + get_hitbox_name(final_target.data.hitbox) + crypt_str("] | ");
	log << crypt_str("dmg: ") + std::to_string(final_target.data.damage) + crypt_str(" | ");
	log << crypt_str("ticks: ") + std::to_string(backtrack_ticks) + crypt_str(" ");

	if (g_cfg.misc.events_to_log[EVENTLOG_HIT])
		eventlogs::get().add(log.str());


	//log << crypt_str("resolver type: ") + get_resolver_type(final_target.record->type);// + std::to_string(final_target.record->side);
	//if (debugcheat_now)

	//if (g_cfg.misc.events_to_log[EVENTLOG_HIT])
	//	eventlogs::get().add(log.str());
#endif
	cmd->m_viewangles = aim_angle;
	cmd->m_buttons |= IN_ATTACK;
	cmd->m_tickcount = TIME_TO_TICKS(final_target.record->simulation_time + util::get_interpolation());

	last_target_index = final_target.record->i;
	last_shoot_position = g_ctx.globals.eye_pos;
	last_target[last_target_index] = Last_target
	{
		*final_target.record, final_target.data, final_target.distance
	};

	auto shot = &g_ctx.shots.emplace_back();

	shot->last_target = last_target_index;
	shot->side = final_target.record->side;
	shot->fire_tick = m_globals()->m_tickcount;
	shot->shot_info.target_name = player_info.szName;
	shot->shot_info.client_hitbox = get_hitbox_name(final_target.data.hitbox, true);
	shot->shot_info.client_damage = final_target.data.damage;
	shot->shot_info.hitchance = hitchance_amount;
	shot->shot_info.backtrack_ticks = backtrack_ticks;
	shot->shot_info.aim_point = final_target.data.point.point;
	//shot->shot_info.typ = final_target.record->type;

	g_ctx.globals.aimbot_working = true;
	g_ctx.globals.revolver_working = false;
	g_ctx.globals.last_aimbot_shot = m_globals()->m_tickcount;
}

//bool aim::hitchance(const Vector& aim_angle)
//{
//	auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();
//
//	if (!weapon_info)
//		return 0;
//
//	if ((g_ctx.globals.eye_pos - final_target.data.point.point).Length() > weapon_info->flRange)
//		return 0;
//
//	auto forward = ZERO;
//	auto right = ZERO;
//	auto up = ZERO;
//
//	math::angle_vectors(aim_angle, &forward, &right, &up);
//
//	math::fast_vec_normalize(forward);
//	math::fast_vec_normalize(right);
//	math::fast_vec_normalize(up);
//
//	static auto setup_spread_values = true;
//	static float spread_values[256][6];
//	auto hitchance_valid_seeds = 0;
//
//	if (setup_spread_values)
//	{
//		setup_spread_values = false;
//
//		for (auto i = 0; i < 256; ++i)
//		{
//			math::random_seed(i);
//
//			auto a = math::random_float(0.0f, 1.0f);
//			auto b = math::random_float(0.0f, DirectX::XM_2PI);
//			auto c = math::random_float(0.0f, 1.0f);
//			auto d = math::random_float(0.0f, DirectX::XM_2PI);
//
//			spread_values[i][0] = a;
//			spread_values[i][1] = c;
//
//			auto sin_b = 0.0f, cos_b = 0.0f;
//			DirectX::XMScalarSinCos(&sin_b, &cos_b, b);
//
//			auto sin_d = 0.0f, cos_d = 0.0f;
//			DirectX::XMScalarSinCos(&sin_d, &cos_d, d);
//
//			spread_values[i][2] = sin_b;
//			spread_values[i][3] = cos_b;
//			spread_values[i][4] = sin_d;
//			spread_values[i][5] = cos_d;
//		}
//	}
//
//	for (auto i = 0; i < 256; ++i)
//	{
//		auto inaccuracy = spread_values[i][0] * g_ctx.globals.inaccuracy;
//		auto spread = spread_values[i][1] * g_ctx.globals.spread;
//
//		auto spread_x = spread_values[i][3] * inaccuracy + spread_values[i][5] * spread;
//		auto spread_y = spread_values[i][2] * inaccuracy + spread_values[i][4] * spread;
//
//		auto direction = ZERO;
//
//		direction.x = forward.x + right.x * spread_x + up.x * spread_y;
//		direction.y = forward.y + right.y * spread_x + up.y * spread_y;
//		direction.z = forward.z + right.z * spread_x + up.z * spread_y; //-V778
//
//		auto end = g_ctx.globals.eye_pos + direction * weapon_info->flRange;
//
//		CGameTrace tr;
//		Ray_t ray(g_ctx.globals.eye_pos, end);
//
//		m_trace()->ClipRayToEntity(ray, MASK_SHOT | CONTENTS_GRATE, final_target.record->player, &tr);
//
//		if (hitbox_intersection(final_target.record->player, final_target.record->matrixes_data.main, final_target.data.hitbox, g_ctx.globals.eye_pos, end))
//		{
//			if (tr.hit_entity == final_target.record->player)
//				++hitchance_valid_seeds;
//		}
//	}
//
//	return (int)((float)hitchance_valid_seeds / 2.56f);
//}

void hit_chance::build_seed_table() {
	constexpr float pi_2 = 2.0f * (float)M_PI;
	for (size_t i = 0; i < 256; ++i) {
		math::random_seed(i);

		const float rand_a = math::random_float(0.0f, 1.0f);
		const float rand_pi_a = math::random_float(0.0f, pi_2);
		const float rand_b = math::random_float(0.0f, 1.0f);
		const float rand_pi_b = math::random_float(0.0f, pi_2);

		hit_chance_records[i] = {
			{  rand_a, rand_b                                 },
			{  std::cos(rand_pi_a), std::sin(rand_pi_a)     },
			{  std::cos(rand_pi_b), std::sin(rand_pi_b)     }
		};
	}
}

bool hit_chance::intersects_bb_hitbox(Vector start, Vector delta, Vector min, Vector max) {
	float d1, d2, f;
	auto start_solid = true;
	auto t1 = -1.0, t2 = 1.0;

	const float _start[3] = { start.x, start.y, start.z };
	const float _delta[3] = { delta.x, delta.y, delta.z };
	const float mins[3] = { min.x, min.y, min.z };
	const float maxs[3] = { max.x, max.y, max.z };

	for (auto i = 0; i < 6; ++i) {
		if (i >= 3) {
			const auto j = (i - 3);

			d1 = _start[j] - maxs[j];
			d2 = d1 + _delta[j];
		}
		else {
			d1 = -_start[i] + mins[i];
			d2 = d1 - _delta[i];
		}

		if (d1 > 0 && d2 > 0) {
			start_solid = false;
			return false;
		}

		if (d1 <= 0 && d2 <= 0)
			continue;

		if (d1 > 0)
			start_solid = false;

		if (d1 > d2) {
			f = d1;
			if (f < 0)
				f = 0;

			f /= d1 - d2;
			if (f > t1)
				t1 = f;
		}
		else {
			f = d1 / (d1 - d2);
			if (f < t2)
				t2 = f;
		}
	}

	return start_solid || (t1 < t2 && t1 >= 0.0f);
}

bool __vectorcall hit_chance::intersects_hitbox(Vector eye_pos, Vector end_pos, Vector min, Vector max, float radius) {
	auto dist = math::segment_to_segment(eye_pos, end_pos, min, max);

	return (dist < radius);
}

std::vector<hit_chance::hitbox_data_t> hit_chance::get_hitbox_data(adjust_data* log, int hitbox) {
	std::vector<hitbox_data_t> hitbox_data;
	auto target = static_cast<player_t*>(m_entitylist()->GetClientEntity(log->player->EntIndex()));

	const auto model = target->GetClientRenderable()->GetModel();

	if (!model)
		return {};


	auto hdr = m_modelinfo()->GetStudioModel(model);

	if (!hdr)
		return {};

	auto set = hdr->pHitboxSet(log->player->m_nHitboxSet());

	if (!set)
		return {};

	//we use 128 bones that not proper use aim matrix there
	auto bone_matrix = log->matrixes_data.main;

	Vector min, max;

	if (hitbox == -1) {
		for (int i = 0; i < set->numhitboxes; ++i) {
			const auto box = set->pHitbox(i);

			if (!box)
				continue;

			float radius = box->radius;
			const auto is_capsule = radius != -1.f;

			if (is_capsule) {
				math::vector_transform(box->bbmin, bone_matrix[box->bone], min);
				math::vector_transform(box->bbmax, bone_matrix[box->bone], max);
			}
			else {
				math::vector_transform(math::vector_rotate(box->bbmin, box->rotation), bone_matrix[box->bone], min);
				math::vector_transform(math::vector_rotate(box->bbmax, box->rotation), bone_matrix[box->bone], max);
				radius = min.DistTo(max);
			}

			hitbox_data.emplace_back(hitbox_data_t{ min, max, radius, box, box->bone, box->rotation });
		}
	}
	else {
		const auto box = set->pHitbox(hitbox);

		if (!box)
			return {};

		float radius = box->radius;
		const auto is_capsule = radius != -1.f;

		if (is_capsule) {
			math::vector_transform(box->bbmin, bone_matrix[box->bone], min);
			math::vector_transform(box->bbmax, bone_matrix[box->bone], max);
		}
		else {
			math::vector_transform(math::vector_rotate(box->bbmin, box->rotation), bone_matrix[box->bone], min);
			math::vector_transform(math::vector_rotate(box->bbmax, box->rotation), bone_matrix[box->bone], max);
			radius = min.DistTo(max);
		}

		hitbox_data.emplace_back(hitbox_data_t{ min, max, radius, box, box->bone, box->rotation });
	}

	return hitbox_data;
}

Vector hit_chance::get_spread_direction(weapon_t* weapon, Vector angles, int seed) {
	if (!weapon)
		return Vector();

	const int   rnsd = (seed & 0xFF);
	const auto* data = &hit_chance_records[rnsd];

	if (!data)
		return Vector();

	float rand_a = data->random[0];
	float rand_b = data->random[1];

	if (weapon->m_iItemDefinitionIndex() == WEAPON_NEGEV) {
		auto weapon_info = weapon ? g_ctx.globals.weapon->get_csweapon_info() : nullptr;

		if (weapon_info && weapon_info->iRecoilSeed < 3) {
			rand_a = 1.0f - std::pow(rand_a, static_cast<float>(3 - weapon_info->iRecoilSeed + 1));
			rand_b = 1.0f - std::pow(rand_b, static_cast<float>(3 - weapon_info->iRecoilSeed + 1));
		}
	}

	//must write from predition
	const float rand_inaccuracy = rand_a * g_ctx.globals.inaccuracy;
	const float rand_spread = rand_b * g_ctx.globals.spread;

	const float spread_x = data->inaccuracy[0] * rand_inaccuracy + data->spread[0] * rand_spread;
	const float spread_y = data->inaccuracy[1] * rand_inaccuracy + data->spread[1] * rand_spread;

	Vector forward, right, up;
	math::angle_vectors(angles, &forward, &right, &up);

	return forward + right * spread_x + up * spread_y;
}

bool hit_chance::can_intersect_hitbox(const Vector start, const Vector end, Vector spread_dir, adjust_data* log, int hitbox)
{
	const auto hitbox_data = get_hitbox_data(log, hitbox);

	if (hitbox_data.empty())
		return false;

	auto intersected = false;
	Vector delta;
	Vector start_scaled;

	for (const auto& it : hitbox_data) {
		const auto is_capsule = it.m_radius != -1.f;
		if (!is_capsule) {
			math::vector_i_transform(start, log->matrixes_data.main[it.m_bone], start_scaled);
			math::vector_i_rotate(spread_dir * 8192.f, log->matrixes_data.main[it.m_bone], delta);
			if (intersects_bb_hitbox(start_scaled, delta, it.m_min, it.m_max)) {
				intersected = true;
				break; //note - AkatsukiSun: cannot hit more than one hitbox.
			}
		}
		else if (intersects_hitbox(start, end, it.m_min, it.m_max, it.m_radius)) {
			intersected = true;
			break;//note - AkatsukiSun: cannot hit more than one hitbox.
		}
		else {
			intersected = false;
			break;
		}
	}

	return intersected;
}

bool hit_chance::can_hit(adjust_data* log, weapon_t* weapon, Vector angles, int hitbox) {

	auto target = static_cast<player_t*>(m_entitylist()->GetClientEntity(log->player->EntIndex()));

	if (!target || !weapon)
		return false;

	auto weapon_info = weapon ? g_ctx.globals.weapon->get_csweapon_info() : nullptr;

	if (!weapon_info)
		return false;

	build_seed_table();

	if ((weapon->m_iItemDefinitionIndex() == WEAPON_SSG08 || weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER) && !(g_ctx.local()->m_fFlags() & FL_ONGROUND)) {
		if ((g_ctx.globals.inaccuracy < 0.009f)) {
			return true;
		}
	}

	const Vector eye_pos = g_ctx.local()->get_eye_pos();
	Vector start_scaled = { };
	const auto hitchance_cfg = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;
	const int hits_needed = (hitchance_cfg * 256) / 100;
	int hits = 0;

	for (int i = 0; i < 256; ++i) {

		Vector spread_dir = get_spread_direction(weapon, angles, i);
		Vector end_pos = eye_pos + (spread_dir * 8192.f);

		if (can_intersect_hitbox(eye_pos, end_pos, spread_dir, log, hitbox))
			hits++;

		if (hits >= hits_needed)
			return true;
	}

	return false;
}

static int clip_ray_to_hitbox(const Ray_t& ray, mstudiobbox_t* hitbox, matrix3x4_t& matrix, trace_t& trace)
{
	static auto fn = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 F3 0F 10 42"));

	trace.fraction = 1.0f;
	trace.startsolid = false;

	return reinterpret_cast <int(__fastcall*)(const Ray_t&, mstudiobbox_t*, matrix3x4_t&, trace_t&)> (fn)(ray, hitbox, matrix, trace);
}

bool aim::hitbox_intersection(player_t* e, matrix3x4_t* matrix, int hitbox, const Vector& start, const Vector& end, float* safe)
{
	auto model = e->GetModel();

	if (!model)
		return false;

	auto studio_model = m_modelinfo()->GetStudioModel(model);

	if (!studio_model)
		return false;

	auto studio_set = studio_model->pHitboxSet(e->m_nHitboxSet());

	if (!studio_set)
		return false;

	auto studio_hitbox = studio_set->pHitbox(hitbox);

	if (!studio_hitbox)
		return false;

	Vector min, max;

	const auto is_capsule = studio_hitbox->radius != -1.f;

	if (is_capsule)
	{
		math::vector_transform(studio_hitbox->bbmin, matrix[studio_hitbox->bone], min);
		math::vector_transform(studio_hitbox->bbmax, matrix[studio_hitbox->bone], max);
		const auto dist = math::segment_to_segment(start, end, min, max);

		if (dist < studio_hitbox->radius)
			return true;
	}
	else
	{
		math::vector_transform(math::vector_rotate(studio_hitbox->bbmin, studio_hitbox->rotation), matrix[studio_hitbox->bone], min);
		math::vector_transform(math::vector_rotate(studio_hitbox->bbmax, studio_hitbox->rotation), matrix[studio_hitbox->bone], max);

		math::vector_i_transform(start, matrix[studio_hitbox->bone], min);
		math::vector_i_rotate(end, matrix[studio_hitbox->bone], max);

		if (math::intersect_line_with_bb(min, max, studio_hitbox->bbmin, studio_hitbox->bbmax))
			return true;
	}

	return false;
}