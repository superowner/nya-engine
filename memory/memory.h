//https://code.google.com/p/nya-engine/

#pragma once

#include "log/log.h"

namespace nya_memory
{

void set_log(nya_log::log_base *l);
nya_log::log_base &log();

}
