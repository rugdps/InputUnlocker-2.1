#ifndef INPUTUNLOCKER_UTILS_HPP
#define INPUTUNLOCKER_UTILS_HPP

#include <Windows.h>
#include <dobby.h>

namespace gd {
    extern uintptr_t base;
}

namespace cocos2d {
    extern uintptr_t base;
    extern HMODULE handle;
}

namespace memory {
    void init(DWORD procId);
    void writeProtected(uintptr_t address, BYTE *bytes, size_t len);
    void midhook(uintptr_t dst, uintptr_t src, size_t len, uintptr_t* returnAddress);
    int hook(uintptr_t address, void* replaceCall, void** originCall);
}

#endif //INPUTUNLOCKER_UTILS_HPP
