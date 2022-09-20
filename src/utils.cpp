#include <cstring>
#include <sys/mman.h>
#include "utils.hpp"

static long pageSize;

uintptr_t getLibBase(const char *name)
{
    FILE *fp;
    uintptr_t addr = 0;
    char filename[32], buffer[1024];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", getpid());
    fp = fopen(filename, "rt");
    if (fp != nullptr)
    {
        while (fgets(buffer, sizeof(buffer), fp))
        {
            if (strstr(buffer, name))
            {
                addr = (uintptr_t)strtoul(buffer, nullptr, 16);
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

namespace memory {
    uintptr_t base = 0;
}

void memory::init() {
    pageSize = sysconf(_SC_PAGE_SIZE);
    memory::base = getLibBase(cocos2dLibName);
}

void memory::protect(uintptr_t addr, int flags) {
    auto addrFixed = addr & ~(pageSize - 1);
    mprotect((void *)addrFixed, pageSize, flags);
}

void memory::writeProtected(uintptr_t addr, char* data, size_t len) {
    protect(addr, PROT_WRITE | PROT_EXEC);
    memcpy((void*)addr, data, len);
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
    // reserve crashes for some reason
    std::string to = str;
    to.clear();
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