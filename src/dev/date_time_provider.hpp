#pragma once
#include "crescent/time.h"
#include "sched/mutex.hpp"

struct DateTimerProvider {
	virtual int get_date(CrescentDateTime& res) = 0;
};

extern Mutex<DateTimerProvider*> DATE_TIME_PROVIDER;
