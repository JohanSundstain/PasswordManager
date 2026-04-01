#include "PasswordManager.h"

#include <windows.h>
#include <thread>

HHOOK hHook = NULL;

// Функция, которая будет работать в отдельном потоке
void HookThread()
{
    // Устанавливаем хук ВНУТРИ потока
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT 
        {
        if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
            std::cout << "Key pressed: " << pKey->vkCode << std::endl;
        }
        return CallNextHookEx(hHook, nCode, wParam, lParam);
        }, GetModuleHandle(NULL), 0);

    // Обязательный цикл сообщений для этого потока
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Снимаем хук перед выходом из потока
    UnhookWindowsHookEx(hHook);
}

int main() {
    
    PasswordManager pm;
    pm.run();

    return 0;
}