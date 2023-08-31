#pragma once

#include "..\..\..\win_includes.hpp"
#include "..\..\misc\animation fix\animation_system.h"

class slowwalk : public singleton <slowwalk> 
{
public:
	void create_move(CUserCmd* m_pcmd, float custom_speed = -1.0f);
};