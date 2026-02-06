#ifndef INI_H
#define INI_H

#include "main.h"
#include "globals.h"

class Config;
class Overlay;

class Ini {
    class Impl;
    Impl* impl;

public:
    Ini(LPCTSTR file_name, Overlay* overlay = nullptr, Config* config = nullptr);
    ~Ini();

    void set_config(Config* config);
    void set_overlay(Overlay* overlay);
};

//extern Ini* default_ini;

#endif // INI_H
