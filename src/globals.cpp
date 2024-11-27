
#include "globals.h"
#include "log.h" // Ensure that the full definition of Logger is included
#include "overlay.h" // Ensure that the full definition of Overlay and OverlayPtr are included
#include "conf.h"
#include "ini.h"

// Initialize global variables
Config* default_config = nullptr;
Ini* default_ini = nullptr;
Logger dummy_logger(_T("dummy.log"), nullptr, nullptr); // Ensure this definition matches Logger's constructor
Logger* default_logger = &dummy_logger;
OverlayPtr default_overlay;
