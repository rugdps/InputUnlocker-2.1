#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

#ifndef WIN32
#error "WIN32 is not defined. You should add \"-A win32\" as CMake argument"
#endif

static_assert(sizeof(std::string) == 24, "std::string size is incorrect. "
                                         "You have to use Release mode. "
                                         "If you want to debug it, use RelWithDebInfo profile. "
                                         "MinSizeRel also works");

extern "C" {
    // meh, I don't like this overbloat of minhook and forced dynamic linking
    // works fine if you use bepinex's fork
    extern auto DobbyHook(void *function_address, void *replace_call, void **origin_call) -> int;
}

void (*dispatchChar_o)(void *, WPARAM);

// I don't use cocos2d.h because of some weird issues on some machines where it fails to link on startup
// we only need 2 functions, so who cares
typedef void *(__cdecl *CCIMEDispatcher_sharedDispatcher_f)();
typedef void (__thiscall *CCIMEDispatcher_dispatchInsertText_f)(void *, const char *, size_t);

CCIMEDispatcher_sharedDispatcher_f CCIMEDispatcher_sharedDispatcher;
CCIMEDispatcher_dispatchInsertText_f CCIMEDispatcher_dispatchInsertText;

uintptr_t writeWidthReturnAddress;
uintptr_t readWidthReturnAddress;
uintptr_t applyWidthReturnAddress;

void dispatchChar_h(void *idk, WPARAM code) {
    // this exists in cocos2d 2.2.3
    // rob probably wiped it out on purpose
    if (code > 127) {
        char szUtf8[8] = {0};
        int nLen = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) &code, 1, szUtf8, sizeof(szUtf8), NULL, NULL);
        CCIMEDispatcher_dispatchInsertText(CCIMEDispatcher_sharedDispatcher(), szUtf8, nLen);
        return;
    }
    dispatchChar_o(idk, code);
}

void __declspec(naked) writeWidth_mh() {
    __asm {
        // chunk of overwritten code
            cmp dword ptr ss :[ebp - 0x90], 0x10
        // map array to new memory to not overwrite class fields
            mov[ecx + edi * 4 + 0x6C8], eax
            jmp writeWidthReturnAddress
    }
}

void __declspec(naked) readWidth_mh() {
    __asm {
        // map FontObject's width array
            lea edx,[eax + 0x6C8]
        // map MultilineBitmapFont's width array
            lea ecx,[esi + 0x6C8]
            jmp[readWidthReturnAddress]
    }
}

// this can be done more easily with full function hook
// but this was my first implementation (and it only supports cyrillic)
// we use this in production for now to make it harder to reverse for russian skiddies who like to steal stuff and not credit authors
void __declspec(naked) applyWidth_mh() {
    __asm {
        // use movzx to not have 0xFF in upper bytes if utf
            movzx ebx, byte ptr ds :[eax + edi]
        // see if a char is utf8
            cmp ebx, 0x7F
            jle end_write
        // see if current byte has another utf8 byte after (first bits are 11)
            cmp bl, 0xC0
        // actually skip parent bytes
        // previously I did otherwise but it didn't work great with slicing if you have a long string with no spaces
        // because in that case string will always end on invalid utf8
            jge skip
        // since we start with last, obtain utf8 char from before
        // this only supports utf16 for obvious reasons
            movzx eax, byte ptr ds :[eax + edi - 1]
        // obtain char code - *stackoverflow link here*
            and al, 0x1F
            and bl, 0x3F
            shl ax, 0x6
            or ebx, eax
            jmp end_write
            skip:
        // set width to 0 for utf8 parts unless last byte
            xor eax, eax // null
            jmp end
            end_write:
        // also don't forget to map array to new memory
            mov eax, dword ptr[esi + ebx * 4 + 0x6C8]
            end:
            jmp applyWidthReturnAddress
    }
}

void (__thiscall* ShareCommentLayer_updateCharCountLabel_o)(void* pThis);
void __fastcall ShareCommentLayer_updateCharCountLabel_h(char* pThis) {
    auto sizePtr = (int*)(pThis + 0x1FC);
    auto oldSize = *sizePtr;
    auto str = *(std::string*)(pThis + 0x1EC);

    auto fixedSize = 0;
    for(auto i = 0; i < str.size(); i++) {
        unsigned char ch = str[i];
        // skip parent bytes
        // first bit means utf8
        // second bit is set when utf8 char need at least one extra byte
        // so we will count only last bytes which start with 0b10xxxxxx
        // or just 0b0xxxxxxx bytes that correspond to ASCII
        if(ch < 0b11000000) {
            fixedSize++;
        }
    }

    // string is never accessed in hooked function so changing std::string's "internals" won't UB
    *sizePtr = fixedSize;
    ShareCommentLayer_updateCharCountLabel_o(pThis);
    // but we should recover it for obvious reasons
    *sizePtr = oldSize;
}

void (__thiscall* GJWriteMessagePopup_updateCharCountLabel_o)(void* pThis, bool isBody);
void __fastcall GJWriteMessagePopup_updateCharCountLabel_h(char* pThis, void* edx, bool isBody) {
    int* sizePtr;
    std::string str;

    if(isBody) {
        str = *(std::string*)(pThis + 0x1F8);
        sizePtr = (int*)(pThis + 0x208);
    } else {
        str = *(std::string*)(pThis + 0x210);
        sizePtr = (int*)(pThis + 0x220);
    }

    auto oldSize = *sizePtr;
    auto fixedSize = 0;
    for(auto i = 0; i < str.size(); i++) {
        unsigned char ch = str[i];
        if(ch < 0b11000000) {
            fixedSize++;
        }
    }

    *sizePtr = fixedSize;
    GJWriteMessagePopup_updateCharCountLabel_o(pThis, isBody);
    *sizePtr = oldSize;
}

static HANDLE handle;

void writeProtected(LPVOID address, BYTE *bytes, size_t len) {
    DWORD old;
    VirtualProtectEx(handle, address, len, PAGE_EXECUTE_READWRITE, &old);
    WriteProcessMemory(handle, address, bytes, len, nullptr);
    VirtualProtectEx(handle, address, len, old, &old);
}

void midhook(uintptr_t dst, uintptr_t src, size_t len, uintptr_t* returnAddress) {
    // since we write bytes we should get relatives
    auto relativeAddress = src - (dst + 5);

    *returnAddress = dst + len;

    auto data = new BYTE[len];
    data[0] = 0xE9; // jmp
    *(uintptr_t *) (data + 1) = relativeAddress;
    for (size_t i = 5; i < len; i++) {
        data[i] = 0x90; // nop
    }

    writeProtected((LPVOID) dst, data, len);
}

DWORD WINAPI MainThread(PVOID) {
    HWND okno;
    do {
        // GeometryDash is using GLFW 3.0 (or glfw with opengl 3.0 idk)
        // so we are looking for this class instead of window name because mods and custom clients:tm:
        okno = FindWindowA("GLFW30", nullptr);
    } while (!okno);
    DWORD procId;
    GetWindowThreadProcessId(okno, &procId);
    // I'm actually not sure if there's any way of writing protected memory without this
    handle = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, procId);

    auto gdBaseAddress = (uintptr_t) GetModuleHandle(nullptr);

    writeProtected((LPVOID) (gdBaseAddress + 0x1030B), new BYTE{0xF4}, 1); // increase FontObject size
    writeProtected((LPVOID) (gdBaseAddress + 0x2A5E8), new BYTE{0xF6}, 1); // increase MultilineBitmapFont size
    writeProtected((LPVOID) (gdBaseAddress + 0x10814), new BYTE[3]{0x90, 0x90, 0x90},3); // nop 300 ids check in config parse
    writeProtected((LPVOID) (gdBaseAddress + 0x2A77F), new BYTE[2]{0x52, 0x04},2); // increase loaded char ids in MultilineBitmapFont

    // magic nop
    // was the last piece to get stuff working
    // apperantly GD's fnt parser slices ids to 3 digits (all of cyrillics are 1024 at least)
    writeProtected((LPVOID) (gdBaseAddress + 0x107CB), new BYTE[2]{0x90, 0x90}, 2); // nop string subtraction

    // I don't think we need some complicated stuff like in applyWidth to be fine
    // because afaik this code piece only triggers on \n which is always ascii
    writeProtected((LPVOID) (gdBaseAddress + 0x2B695), new BYTE[2]{0xC8, 0x06}, 2); // map space width to new memory

    midhook(gdBaseAddress + 0x10845, (uintptr_t) &writeWidth_mh, 11, &writeWidthReturnAddress);
    midhook(gdBaseAddress + 0x2A783, (uintptr_t) &readWidth_mh, 9, &readWidthReturnAddress);
    midhook(gdBaseAddress + 0x2B55D, (uintptr_t) &applyWidth_mh, 11, &applyWidthReturnAddress);

    auto cocos2d = GetModuleHandleA("libcocos2d.dll");

    writeProtected((LPVOID)((uintptr_t)cocos2d + 0x9C96D),
            new BYTE[10] {
                0xBF, 0x3F, 0x00, 0x00, 0x00, // mov edi, 3F ; set character to '?' if it's not present in bitmap set
                0xE9, 0xCF, 0x00, 0x00, 0x00, // jmp back to code flow
            },10);

    CCIMEDispatcher_sharedDispatcher = (CCIMEDispatcher_sharedDispatcher_f) GetProcAddress(cocos2d,"?sharedDispatcher@CCIMEDispatcher@cocos2d@@SAPAV12@XZ");
    CCIMEDispatcher_dispatchInsertText = (CCIMEDispatcher_dispatchInsertText_f) GetProcAddress(cocos2d,"?dispatchInsertText@CCIMEDispatcher@cocos2d@@QAEXPBDH@Z");

    DobbyHook((LPVOID) ((uintptr_t) cocos2d + 0xC3C70), (LPVOID) dispatchChar_h, (LPVOID*) &dispatchChar_o);

    DobbyHook((LPVOID) (gdBaseAddress + 0x24CDC0), (LPVOID) ShareCommentLayer_updateCharCountLabel_h, (LPVOID*) &ShareCommentLayer_updateCharCountLabel_o);
    DobbyHook((LPVOID) (gdBaseAddress + 0x142750), (LPVOID) GJWriteMessagePopup_updateCharCountLabel_h, (LPVOID*) &GJWriteMessagePopup_updateCharCountLabel_o);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            CreateThread(nullptr, 0x1000, MainThread, hModule, NULL, nullptr);
            DisableThreadLibraryCalls(hModule);
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}