#include "common/string-utils.h"

void string_utils_wide2narrow(wchar_t* wide, char* narrow, size_t narrow_size) {
    setlocale(LC_ALL, "");

    if (wcstombs(narrow, wide, narrow_size -1) < 0) {
        return;
    }
}