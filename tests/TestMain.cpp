#include "TestFramework.h"

int main()
{
    int failedTests = 0;
    for (const auto& t : gt::registry())
    {
        const int before = gt::failures();
        t.fn();
        const bool ok = gt::failures() == before;
        failedTests += ok ? 0 : 1;
        std::cout << (ok ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << "\n" << gt::registry().size() - static_cast<size_t>(failedTests) << "/" << gt::registry().size()
              << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}
