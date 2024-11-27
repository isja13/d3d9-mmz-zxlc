#ifndef GLOBALS_H
#define GLOBALS_H

// Forward declarations
class Config;
struct OverlayPtr; // Forward declaration for OverlayPtr
class Ini;
class Logger;
class Overlay;

extern Config* default_config;
extern Ini* default_ini;
extern Logger* default_logger;
extern OverlayPtr default_overlay;

#endif // GLOBALS_H
