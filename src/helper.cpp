#include "helper.h"

#include <sstream>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

class Index {
public:
    Index() {
        index = clang_createIndex(1, 0);
    }

    ~Index() {
        clang_disposeIndex(index);
    }

    CXIndex get_index() const {
        return index;
    }

private:
    CXIndex index;
};

CXTranslationUnit parse_translation_unit(const char* contents, const char* filename,
                                         const std::vector<std::string>& clang_args, unsigned int flags) {
    CXUnsavedFile unsaved_file;
    unsaved_file.Contents = contents;
    unsaved_file.Length = (unsigned long)strlen(contents);
    unsaved_file.Filename = filename;

    std::vector<const char*> c_args;

    for (auto& arg : clang_args) {
        c_args.push_back(arg.c_str());
    }

    static Index index;
    CXTranslationUnit unit;
    CXErrorCode error = clang_parseTranslationUnit2(index.get_index(), filename, c_args.data(), (int)c_args.size(),
                                                    &unsaved_file, 1, flags, &unit);

    if (error != CXError_Success) {
        std::string error_string;

        for (int i = 0; i < (int)clang_getNumDiagnostics(unit); i++) {
            CXDiagnostic diag = clang_getDiagnostic(unit, i);

            CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diag);
            if (severity == CXDiagnostic_Error || severity == CXDiagnostic_Fatal) {
                error_string += "    ";

                CXString str = clang_formatDiagnostic(diag, CXDiagnostic_DisplaySourceLocation);
                error_string += clang_getCString(str);
                clang_disposeString(str);
            }
        }

        std::cout <<
            "[" << draw_symbol('!', Color::Red) << "] translation unit '" <<
            filename << "' contains errors\n    " << error_string;
        return nullptr;
    }

    return unit;
}

bool get_args(CXCursor cursor, std::vector<std::string>& args) {
    ClangStr comment = clang_Cursor_getBriefCommentText(cursor);

    if (!comment.c_str()) {
        return false;
    }

    std::stringstream ss(comment.c_str());
    std::string part;
    while (std::getline(ss, part, ' ')) {
        args.push_back(part);
    }

    if (args[0] == "!") {
        args.erase(args.begin());
        return true;
    } else {
        args.clear();
        return false;
    }
}

std::string draw_symbol(char symbol, Color color) {
#if defined(_WIN32)
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)color);
    std::cout << symbol;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)Color::White);
    return "";
#else
    std::string ansi = "\033[";

    switch (color) {
        case Color::White:
            ansi += "0";
            break;

        case Color::Blue:
            ansi += "34";
            break;

        case Color::Green:
            ansi += "32";
            break;

        case Color::Cyan:
            ansi += "36";
            break;

        case Color::Red:
            ansi += "31";
            break;

        case Color::Yellow:
            ansi += "33";
            break;
    }

    return ansi + "m" + symbol + "\033[0m";
#endif
}
