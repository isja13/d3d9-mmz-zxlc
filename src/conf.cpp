#include "conf.h"
#include "globals.h"

class Config::Impl {
    cs_wrapper cs; // Assuming cs_wrapper is a critical section wrapper for thread safety
    friend class Config;
};

void Config::begin_config() {
    impl->cs.begin_cs();
}

void Config::end_config() {
    impl->cs.end_cs();
}

Config::Config() : impl(new Impl()) {}

Config::~Config() {
    delete impl;
}
