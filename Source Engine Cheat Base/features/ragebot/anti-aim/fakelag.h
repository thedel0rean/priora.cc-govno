#pragma once

#include "..\..\..\win_includes.hpp"
#include "antiaim.h"

class fakelag : public singleton <fakelag>
{
public:
	void Fakelag(CUserCmd* m_pcmd);
	void Createmove();
	bool FakelagCondition(CUserCmd* m_pcmd);

	bool condition = true;
	bool started_peeking = false;

	int max_choke = 0;
};