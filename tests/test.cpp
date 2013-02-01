#include "test.h"

int main() {
    int res = 0;
    for (TestFunction *func = test_funcs; *func != 0 && res == 0; func++) {
        res = (**func)();
        std::wcout << "\n";
    }
    if (res == 0) {
        std::wcout << "Finished\n";
        return 0;
    }
    std::wcout << "Finished with errors\n";
    return res;
}
