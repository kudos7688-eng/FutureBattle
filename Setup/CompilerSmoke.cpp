#include <windows.h>
#include <iostream>

int main()
{
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    std::cout << "FutureBattle C++ toolchain OK; logical processors: "
              << info.dwNumberOfProcessors << '\n';
    return info.dwNumberOfProcessors > 0 ? 0 : 1;
}
