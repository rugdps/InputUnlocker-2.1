#include "utils.hpp"

namespace gd {
    uintptr_t base = 0;
}

namespace cocos2d {
    uintptr_t base = 0;
    HMODULE handle = nullptr;
}

static HANDLE handle;

void memory::init(DWORD procId) {
    // I'm actually not sure if there's any way of writing protected memory without this
    handle = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, procId);

    gd::base = (uintptr_t)GetModuleHandle(nullptr);
    cocos2d::handle = GetModuleHandleA("libcocos2d.dll");
    cocos2d::base = (uintptr_t) cocos2d::handle;
}

void memory::writeProtected(uintptr_t address, BYTE *bytes, size_t len) {
    DWORD old;
    VirtualProtectEx(handle, (LPVOID)address, len, PAGE_EXECUTE_READWRITE, &old);
    WriteProcessMemory(handle, (LPVOID)address, bytes, len, nullptr);
    VirtualProtectEx(handle, (LPVOID)address, len, old, &old);
}

void memory::midhook(uintptr_t dst, uintptr_t src, size_t len, uintptr_t* returnAddress) {
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

int memory::hook(uintptr_t address, void* replaceCall, void** originCall) {
    return DobbyHook((void*) address, replaceCall, originCall);
}