#include <jni.h>
#include <string>
#include "dobby.h"
#include "utils.hpp"
#include <dlfcn.h>
#ifndef IU_NO_UTF8_VALIDATION
#include <vector>
#endif
#ifdef IU_NW_VERSION
#include <sstream>
#endif

int (*FontObject_getFontWidth_o)(char* pThis, int code);

int FontObject_getFontWidth_h(char* pThis, int code) {
    if(code > 127) {
        // there is no easy way to patch 300 chars limit in loading fonts to MultilineBitmapFont,
        // so instead we do mappings which basically limits this mod to only 1 extra language (russian in this case)
        // it also reduces memory usage
        // if we want to do full utf8 support we should midhook (or most likely rewrite) whole calling function
        // magic number 896 is picked so 128 corresponds to first cyrillic char - code 1024
        return FontObject_getFontWidth_o(pThis, code + 896);
    }
    return FontObject_getFontWidth_o(pThis, code);
}

typedef char*(*CCString_create_f)(const std::string&);
CCString_create_f CCString_create;

std::string(*MultilineBitmapFont_stringWithMaxWidth_o)(char* pThis, std::string str, float scaledFontWidth, float scale);
// this function is rewritten to avoid midhooks
// the hell are midhooks in thumb
std::string MultilineBitmapFont_stringWithMaxWidth_h(char* pThis, std::string str, float scaledFontWidth, float scale) {
    int pos = 0;
    float fontWidthScale = 0.0f, unk = 0.0f;

    std::string str2;
    auto startsWithSpace = !str.empty() && str.at(0) == ' ';
    auto fontWidths = (int*)(pThis + 0x6C8);

    for (; pos < str.size() && fontWidthScale < (2 * scaledFontWidth); pos++)
    {
        auto ch = str[pos];
        // InputUnlockerHack start
        // do not process utf meta bytes
        if(ch > 0b11000000) {
            str2 += ch;
            continue;
        }

        size_t code = ch;
        if(ch > 127) {
            // valid utf8 string will never UB here
            size_t upperByte = str[pos - 1];
            // convert 2byte utf8 to char code
            // I'm sure there's getchar or smth but who cares
            upperByte &= 0b00011111;
            upperByte <<= 6;
            size_t lowerByte = ch;
            lowerByte &= 0b00111111;
            // also map this value (explained in FontObject_getFontWidth_h)
            code = (upperByte | lowerByte) - 896;
        }
        // InputUnlockerHack end

        auto fontWidth = (float)fontWidths[code];

        if (ch == '\n')
        {
            str2 += ch;
            fontWidthScale += scale * fontWidth;
            pos = 1000;
            break;
        }

        if (startsWithSpace && ch != '\xE2')
        {
            unk = fontWidthScale * (fontWidth * scale);
        }

        str2 += ch;

        fontWidthScale += scale * fontWidth;
    }

    float half = scaledFontWidth / 2;

    auto m_fHalfScaledFontWidth = (float*)(pThis + 0x6A4);
    if (!*(bool*)(pThis + 0x6A8) && fontWidthScale < (2 * scaledFontWidth))
        *m_fHalfScaledFontWidth = (fontWidthScale / 2) - unk;

    if (half > *m_fHalfScaledFontWidth)
        *m_fHalfScaledFontWidth = half;

    if (str.size() > pos)
    {
        int temp = 0, temp2 = pos;

        while (temp <= str.size())
        {
            if (temp2 <= 0)
                break;

            if (str2.at(temp2 - 1) == ' ')
                break;

            ++temp;
            --temp2;
        }

        if (pos - temp > 1)
            str2 = str2.substr(0, str2.size() - temp);
    }


    return *(std::string*)(CCString_create(str2) + 0x20);
}

typedef void(*GJWriteMessagePopup_updateCharCountLabel_f)(void* pThis, bool isBody);
GJWriteMessagePopup_updateCharCountLabel_f GJWriteMessagePopup_updateCharCountLabel;

void (*ShareCommentLayer_updateCharCountLabel_o)(void* pThis);
void ShareCommentLayer_updateCharCountLabel_h(char* pThis) {
    auto strPtr = (std::string*)(pThis + 0x1EC);

    auto fixedSize = 0;
    for(unsigned char ch : *strPtr) {
        // skip parent bytes
        // first bit means utf8
        // second bit is set when utf8 char need at least one extra byte,
        // so we will count only last bytes which start with 0b10xxxxxx
        // or just 0b0xxxxxxx bytes that correspond to ASCII
        if(ch < 0b11000000) {
            fixedSize++;
        }
    }


    auto sizePtr = (int *)(*(char **)strPtr - 0xC);
    auto oldSize = *sizePtr;
    // string is never accessed in hooked function so changing std::string's "internals" won't UB
    *sizePtr = fixedSize;
    ShareCommentLayer_updateCharCountLabel_o(pThis);
    // but we should recover it for obvious reasons
    *sizePtr = oldSize;
}

// this is a workaround since I couldn't hook GJWriteMessagePopup::updateCharCountLabel for some reason
// the failed-to-hook function is only being called here so should be still fine
void (*GJWriteMessagePopup_updateText_o)(void* pThis, const char** str, bool isBody);
void GJWriteMessagePopup_updateText_h(char* pThis, const char** str, bool isBody) {
    GJWriteMessagePopup_updateText_o(pThis, str, isBody);
    auto fixedSize = 0;
    auto len = strlen(*str);
    for(auto i = 0; i < len; i++) {
        if((*str)[i] < 0b11000000) {
            fixedSize++;
        }
    }

    auto sizePtr = (int*)(*(char**)(pThis + (isBody ? 0x1F8 : 0x1FC)) - 0xC);
    auto oldSize = *sizePtr;
    *sizePtr = fixedSize;
    GJWriteMessagePopup_updateCharCountLabel(pThis, isBody);
    *sizePtr = oldSize;
}

#ifndef IU_NO_UTF8_VALIDATION
void (*TextArea_setString_o)(void* pThis, std::string str);
void TextArea_setString_h(void* pThis, std::string str) {
    TextArea_setString_o(pThis, correctUtf8(str));
}
#endif

#ifdef IU_NW_VERSION
std::string IU_NW_DATA;

void (*CCHttpClient_send_o)(void* pThis, void* pRequest);
void CCHttpClient_send_h(void* pThis, char* pRequest) {
    auto data = (std::vector<char>*)(pRequest + 0x28);
    data->insert(data->end(), IU_NW_DATA.begin(), IU_NW_DATA.end());
    CCHttpClient_send_o(pThis, pRequest);
}
#endif

inline void init() {
    memory::init();

    // nop allowed chars check
    memory::writeProtected(memory::base + 0x206378, new char [2] { 0x00, 0xBF }, 2);

    // increase FontObject size
    memory::writeProtected(memory::base + 0x1EECAC + 1, new char [3] { 0xF2, 0xD0, 0x40 }, 3);
    memory::writeProtected(memory::base + 0x1EECB8 + 1, new char [3] { 0xF2, 0xD0, 0x42 }, 3);

    // increase MultilineBitmapFont size
    memory::writeProtected(memory::base + 0x1FD1AC + 1, new char[2] { 0xF2, 0xC8 }, 2);
    // map font width array to new memory
    // magic number 0x6C8 - previous object size
    memory::writeProtected(memory::base + 0x1FCCF2 + 2, new char[2] { 0xC8, 0x06 }, 2);

    // nop 300 id check
    // noping this check also means UB if we load fonts with really high char ids
    // increasing FontObject size should cover utf8, but font files can have any ids
    memory::writeProtected(memory::base + 0x1EEBF0 + 1, new char { 0xBF }, 1);
    // fix id parsing
    memory::writeProtected(memory::base + 0x1EEBD8, new char[2] { 0x00, 0xBF }, 2);

#ifndef IU_NO_MISSING_MARKER
#ifndef IU_MISSING_MARKER
#define IU_MISSING_MARKER '?'
#endif
    memory::writeProtected(memory::base + 0x3BFE4A, new char[20] {
            0x61, 0x2C, // cmp r4, 'a'
            0x04, 0xD3, // blo 0xE
            0x7A, 0x2C, // cmp r4, 'z'
            0x02, 0xD8, // bhi 0xE
            0xA4, 0xF1, 0x20, 0x04, // sub r4, 0x20
            0x01, 0xE0, // b 0x12
            0x4F, 0xF0, IU_MISSING_MARKER, 0x04, // mov r3, IU_MISSING_MARKER
            0x3B, 0xE0, // b 0x8C ; jump back to code flow
    }, 20);
#endif
    CCString_create = (CCString_create_f) DobbySymbolResolver(cocos2dLibName, "_ZN7cocos2d8CCString6createERKSs");

    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN19MultilineBitmapFont18stringWithMaxWidthESsff"), (void*)MultilineBitmapFont_stringWithMaxWidth_h, (void**)&MultilineBitmapFont_stringWithMaxWidth_o);
    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN10FontObject12getFontWidthEi"), (void*)FontObject_getFontWidth_h, (void**)&FontObject_getFontWidth_o);
    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN17ShareCommentLayer20updateCharCountLabelEv"), (void*)ShareCommentLayer_updateCharCountLabel_h, (void**)&ShareCommentLayer_updateCharCountLabel_o);
#ifndef IU_NO_UTF8_VALIDATION
    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN8TextArea9setStringESs"), (void*)TextArea_setString_h, (void**)&TextArea_setString_o);
#endif
#ifdef IU_NW_VERSION
    std::ostringstream s;
    s << "inputUnlockerVersion=" << IU_NW_VERSION;
    IU_NW_DATA = std::string(s.str());
    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN7cocos2d9extension12CCHttpClient4sendEPNS0_13CCHttpRequestE"), (void*)CCHttpClient_send_h, (void**)&CCHttpClient_send_o);
#endif

    // nop original call for our workaround
    // this just avoids unnecessary calls
    memory::writeProtected(memory::base + 0x342F76, new char[2] { 0x00, 0xBF }, 2);
    GJWriteMessagePopup_updateCharCountLabel = (GJWriteMessagePopup_updateCharCountLabel_f) DobbySymbolResolver(cocos2dLibName, "_ZN19GJWriteMessagePopup20updateCharCountLabelEi");
    DobbyHook(DobbySymbolResolver(cocos2dLibName, "_ZN19GJWriteMessagePopup10updateTextESsi"), (void*)GJWriteMessagePopup_updateText_h, (void**)&GJWriteMessagePopup_updateText_o);
}

[[maybe_unused]] JNIEXPORT jint JNI_OnLoad(JavaVM*, void*) {
    init();
    return JNI_VERSION_1_4;
}