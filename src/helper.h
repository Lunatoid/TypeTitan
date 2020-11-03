#pragma once

#include <string>
#include <vector>
#include <iostream>

#include <clang-c/Index.h>

class ClangStr {
public:
    ClangStr(CXString str) : str(str) {}
    ~ClangStr() {
        clang_disposeString(str);
    }

    const char* c_str() const {
        return clang_getCString(str);
    }

private:
    CXString str;
};

// Colors in accordance to the Windows color codes
enum class Color {
    White = 7,
    Blue = 9,
    Green = 10,
    Cyan = 11,
    Red = 12,
    Yellow = 14
};

static const char* DEFAULT_ARGS[] = {
    "-std=c++11",
    "-xc++"
};

static const int DEFAULT_ARGS_COUNT = sizeof(DEFAULT_ARGS) / sizeof(*DEFAULT_ARGS);

const unsigned int DEFAULT_FLAGS = CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_Incomplete;

CXTranslationUnit parse_translation_unit(const char* contents, const char* filename,
                                         const std::vector<std::string>& clang_args, unsigned int flags = DEFAULT_FLAGS);

bool get_args(CXCursor cursor, std::vector<std::string>& args);

std::string draw_symbol(char symbol, Color color);
