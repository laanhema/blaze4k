// Proves that the sanitizers of the asan-ubsan preset are on (#136). Each mode
// makes one deliberate error of a kind that code review once found by reading.
// ctest passes a mode only when the sanitizer's report is in the output and the
// program stopped there (tests/CMakeLists.txt). Built only with
// -DBLAZE_SANITIZE=ON, so no other build compiles or runs it.
//
// Usage: sanitizer_canary <use-after-free|float-cast>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

// Reads heap memory after it was freed: AddressSanitizer's heap-use-after-free.
// The pointer goes through a volatile so the compiler cannot see the error and
// remove it.
int use_after_free() {
    int* volatile slot = new int(7);
    int* owner = slot;
    delete owner;
    return *slot;
}

// Converts a double that no int64_t can hold: UndefinedBehaviorSanitizer's
// float-cast-overflow.
std::int64_t float_cast() {
    volatile double huge = 1e300;
    return static_cast<std::int64_t>(huge);
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "use-after-free") {
        std::cout << "read " << use_after_free() << "\n";
    } else if (mode == "float-cast") {
        std::cout << "converted to " << float_cast() << "\n";
    } else {
        std::cerr << "usage: sanitizer_canary <use-after-free|float-cast>\n";
        return 2;
    }
    // Reached only when the sanitizer did not stop the program.
    std::cout << "canary survived: no sanitizer stopped '" << mode << "'\n";
    return 0;
}
