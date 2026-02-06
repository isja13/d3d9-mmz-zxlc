#ifndef BACKBUFFER_H
#define BACKBUFFER_H

// Forward declaration of MyIDXGISwapChain
class MyIDXGISwapChain;

class BackBuffer {
public:
    MyIDXGISwapChain*& get_sc() { static MyIDXGISwapChain* sc = nullptr; return sc; }
    void Release() {}
};

#endif // BACKBUFFER_H
