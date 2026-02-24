#include "globals.h"
#include "conf.h"
#include "ini.h"
#include "log.h"

Config* default_config = nullptr;
Ini* default_ini = nullptr;

static Logger dummy_logger(_T("dummy.log"), nullptr, nullptr);
Logger* default_logger = &dummy_logger;

Overlay* default_overlay = nullptr;
