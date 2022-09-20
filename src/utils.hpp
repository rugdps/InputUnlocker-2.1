#ifndef INPUTUNLOCKERHACK_UTILS_H
#define INPUTUNLOCKERHACK_UTILS_H

#include <cstdlib>
#ifndef IU_NO_UTF8_VALIDATION
#include <string>
#endif

constexpr auto cocos2dLibName = "libcocos2dcpp.so";

namespace memory {
    extern uintptr_t base;
    void init();
    void protect(uintptr_t addr, int flags);
    void writeProtected(uintptr_t addr, char* data, size_t len);
}

#ifndef IU_NO_UTF8_VALIDATION
std::string correctUtf8(std::string& str);
#endif

#endif //INPUTUNLOCKERHACK_UTILS_H