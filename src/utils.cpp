#include "utils.hpp"

namespace gd {
    uintptr_t base = 0;
}

namespace cocos2d {
    uintptr_t base = 0;
    HMODULE handle = nullptr;
}

void memory::init() {
    gd::base = (uintptr_t) GetModuleHandle(nullptr);
    cocos2d::handle = GetModuleHandleA("libcocos2d.dll");
    cocos2d::base = (uintptr_t) cocos2d::handle;
}

void memory::writeProtected(uintptr_t address, BYTE *bytes, size_t len, bool autofree = true) {
    DWORD old;
    VirtualProtect((LPVOID) address, len, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)address, bytes, len);
    VirtualProtect((LPVOID) address, len, old, &old);
    if(autofree) {
        delete[] bytes;
    }
}

void memory::midhook(uintptr_t dst, uintptr_t src, size_t len, uintptr_t *returnAddress) {
    // since we write bytes we should get relatives
    auto relativeAddress = src - (dst + 5);

    *returnAddress = dst + len;

    auto data = new BYTE[len];
    data[0] = 0xE9; // jmp
    *(uintptr_t *) (data + 1) = relativeAddress;
    for (size_t i = 5; i < len; i++) {
        data[i] = 0x90; // nop
    }

    writeProtected(dst, data, len);
}

int memory::hook(uintptr_t address, void *replaceCall, void **originCall) {
    return DobbyHook((void *) address, replaceCall, originCall);
}

#ifndef IU_NO_UTF8_VALIDATION
#ifndef IU_INVALID_UTF8_MARKER
#define IU_INVALID_UTF8_MARKER "?"
#endif

void appendInvalidChar(std::string &str) {
    str += IU_INVALID_UTF8_MARKER;
}

// https://stackoverflow.com/questions/17316506/strip-invalid-utf8-from-string-in-c-c
std::string correctUtf8(std::string& str) {
    auto f_size = str.size();
    unsigned char c2 = 0;
    std::string to;
    to.reserve(f_size);

    for (auto i = 0; i < f_size; i++) {
        auto c = (unsigned char) str[i];
        if (c < 32) { // control char
            if (c == 9 || c == 10 || c == 13) { // allow only \t \n \r
                to.append(1, c);
            } else {
                appendInvalidChar(to);
            }
            continue;
        } else if (c < 127) { // normal ASCII
            to.append(1, c);
            continue;
        } else if (c < 160) { // control char (nothing should be defined here either ASCII, ISO_8859-1 or UTF8, so skipping)
            if (c2 == 128) { // fix microsoft mess, add euro
                to.append(1, 226);
                to.append(1, 130);
                to.append(1, 172);
            } else if (c2 == 133) { // fix IBM mess, add NEL = \n\r
                to.append(1, 10);
                to.append(1, 13);
            } else {
                appendInvalidChar(to);
            }
            continue;
        } else if (c < 192) { // invalid for UTF8, converting ASCII
            appendInvalidChar(to);
            continue;
        } else if (c < 194) { // invalid for UTF8, converting ASCII
            appendInvalidChar(to);
            continue;
        } else if (c < 224 && i + 1 < f_size) { // possibly 2byte UTF8
            c2 = (unsigned char) str[i + 1];
            if (c2 > 127 && c2 < 192) { // valid 2byte UTF8
                if (c == 194 && c2 < 160) { // control char, skipping
                    appendInvalidChar(to);
                } else {
                    to.append(1, c);
                    to.append(1, c2);
                }
                i++;
                continue;
            }
        } else if (c < 240 && i + 2 < f_size) { // possibly 3byte UTF8
            c2 = (unsigned char) str[i + 1];
            auto c3 = (unsigned char) str[i + 2];
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192) { // valid 3byte UTF8
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                i += 2;
                continue;
            }
        } else if (c < 245 && i + 3 < f_size) { // possibly 4byte UTF8
            c2 = (unsigned char) str[i + 1];
            auto c3 = (unsigned char) str[i + 2];
            auto c4 = (unsigned char) str[i + 3];
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192 && c4 > 127 && c4 < 192) { // valid 4byte UTF8
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                to.append(1, c4);
                i += 3;
                continue;
            }
        }
        // invalid UTF8 (c>245 || string too short for multi-byte))
        appendInvalidChar(to);
    }
    return to;
}
#endif