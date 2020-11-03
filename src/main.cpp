#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <filesystem>

#include <clang-c/Index.h>

#include "helper.h"
#include "emitter.h"
#include "source_code.h"

namespace fs = std::filesystem;
namespace chrono = std::chrono;

static const char* DEFAULT_NAMESPACE = "tt";
static const bool DEFAULT_NO_EXTRAS = false;
static const bool DEFAULT_NO_EMPTY = false;
static const bool DEFAULT_NO_RECURSIVE = false;

void print_help(int argc, char** argv);

int main(int argc, char** argv) {
    if (argc == 1) {
        print_help(argc, argv);
        return 0;
    }

    std::string namespace_name = DEFAULT_NAMESPACE;
    bool gen_extras = !DEFAULT_NO_EXTRAS;
    bool del_empty = !DEFAULT_NO_EMPTY;
    bool do_recurse = !DEFAULT_NO_RECURSIVE;

    fs::path output_dir;
    std::vector<std::string> files;
    std::vector<std::string> clang_args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Output directory
        if (i == 1) {
            try {
                if (!fs::exists(arg)) {
                    std::cout <<
                        "[" << draw_symbol('/', Color::Yellow) << "] output directory '" <<
                        arg << "' does not exist, creating\n";

                    fs::create_directory(arg);
                }
            } catch (...) {
                std::cout << "[" << draw_symbol('!', Color::Red) << "] failed to create directory '" << arg << "'\n";
                return 1;
            }

            if (arg.back() != '/' && arg.back() != '\\') {
                arg += '/';
            }

            output_dir = fs::path(arg, fs::path::generic_format);
            continue;
        }

        if (arg.front() == '-') {
            if (arg == "-help") {
                print_help(argc, argv);
                return 2;
            }

            if (arg == "-namespace") {
                if (i + 1 >= argc) {
                    std::cout << "[" << draw_symbol('!', Color::Red) << "] no argument provided for option '-namespace'\n";
                    return 3;
                }

                namespace_name = arg;
                continue;
            }

            if (arg == "-no-extras") {
                gen_extras = false;
                continue;
            }

            if (arg == "-no-empty") {
                del_empty = false;
                continue;
            }

            if (arg == "-no-recursive") {
                do_recurse = false;
                continue;
            }


            if (arg == "-clang") {
                for (int j = i; j < argc; j++) {
                    clang_args.push_back(argv[j]);
                }

                i = argc;
                continue;
            }

            std::cout << "[" << draw_symbol('!', Color::Red) << "] unknown argument '" << arg << "'\n";
            continue;
        }

        if (arg.find('*') != std::string::npos) {
            fs::path search_path = fs::path(arg, fs::path::format::generic_format);

            fs::path wildcard_check = search_path;
            wildcard_check.replace_extension("");

            if (wildcard_check.filename() != "*") {
                std::cout <<
                    "[" << draw_symbol('!', Color::Red) << "] invalid wildcard path '" <<
                    arg << "', only filename can be wildcard, example: 'src/*.h'\n";
                continue;
            }

            auto do_search = [&](auto iterator) {
                for (auto& entry : iterator) {
                    if (entry.is_regular_file() && entry.path().extension() == search_path.extension()) {
                        std::string p = entry.path().filename().generic_string();
                        // Don't add any type_titan.*.h or *.tt.h files
                        bool is_type_titan = p.rfind("type_titan.", 0) == 0;
                        bool is_tt = p.rfind("tt.h", p.size() - 4) == p.size() - 4;
                        if (!is_type_titan && !is_tt) {
                            files.push_back(entry.path().generic_string());
                        }
                    }
                }
            };

            try {
                if (do_recurse) {
                    do_search(fs::recursive_directory_iterator(search_path.parent_path()));
                } else {
                    do_search(fs::directory_iterator(search_path.parent_path()));
                }
            } catch (...) {
                std::cout << "[" << draw_symbol('!', Color::Red) << "] failed to (recursively) search directories\n";
                return 4;
            }

        } else {
            files.push_back(arg);
        }
    }

    // Create a string with all options
    std::string clang_args_line = "";

    if (clang_args.empty()) {
        for (int i = 0; i < DEFAULT_ARGS_COUNT; i++) {
            clang_args.push_back(DEFAULT_ARGS[i]);
        }
    }

    for (int i = 0; i < clang_args.size(); i++) {
        clang_args_line += clang_args[i];
        if (i + 1 < clang_args.size()) {
            clang_args_line += ", ";
        }
    }

    std::cout << std::boolalpha <<
        "[" << draw_symbol('?', Color::Blue) << "] output directory : " << output_dir << "\n"
        "[" << draw_symbol('?', Color::Blue) << "] namespace        : " << namespace_name << "\n"
        "[" << draw_symbol('?', Color::Blue) << "] generate extras  : " << gen_extras << "\n"
        "[" << draw_symbol('?', Color::Blue) << "] delete empty     : " << del_empty << "\n"
        "[" << draw_symbol('?', Color::Blue) << "] clang arguments  : " << clang_args_line << "\n\n";

    auto start_time = chrono::high_resolution_clock::now();

    // Remove old type_titan.inc.h and write a temporary one
    // so we get no errors if it's included in a header
    if (gen_extras) {
        fs::path inc_path = fs::path(output_dir, fs::path::generic_format);
        inc_path.replace_filename("type_titan.inc.h");

        fs::remove(inc_path);

        std::ofstream inc_file(inc_path);
        inc_file <<
            "#pragma once\n"
            "#include \"type_titan.h\"";
    }

    add_common_primitives(clang_args);

    if (files.empty()) {
        std::cout << "[" << draw_symbol('*', Color::Green) << "] no files specified, only generating core files\n";
    }

    for (auto& file : files) {
        fs::path new_path = fs::path(file, fs::path::generic_format);

        new_path.replace_extension("tt" + new_path.extension().generic_string());

        std::ifstream in(file);

        if (!in.is_open()) {
            std::cout << "[" << draw_symbol('!', Color::Red) << "] could not open file '" << file << "'\n";
            continue;
        }

        std::ostringstream sstr;
        sstr << in.rdbuf();

        std::cout << "[" << draw_symbol('*', Color::Green) << "] parsing '" << file << "'\n";
        CXTranslationUnit unit = parse_translation_unit(sstr.str().c_str(), file.c_str(), clang_args);

        if (!unit) {
            continue;
        }

        std::cout << "[" << draw_symbol('*', Color::Green) << "] generating '" << new_path.generic_string() << "'\n";
        std::ofstream out(new_path);

        if (!out.is_open()) {
            std::cout << "[" << draw_symbol('!', Color::Red) << "] could not create file '" << new_path.generic_string() << "'\n";
            continue;
        }

        fs::path rel = fs::relative(output_dir, new_path.parent_path());

        if (rel.generic_string().length() > 0) {
            rel += "/";
        }

        std::string filename = fs::path(file).filename().generic_string();
        emit_common_file_start(out, rel.generic_string().c_str(), filename.c_str(), namespace_name.c_str(), false);
        int emitted = emit_eligable_children(out, clang_getTranslationUnitCursor(unit));
        out << "\n}\n";

        out.close();

        if (del_empty && emitted == 0) {
            fs::remove(new_path);
        }
    }

    // Generate core file
    std::cout << "[" << draw_symbol('*', Color::Green) << "] generating 'type_titan.h'\n";

    fs::path core_path = output_dir;
    core_path.replace_filename("type_titan.h");
    std::ofstream out(core_path);

    emit_common_file_start(out, "", "", namespace_name.c_str(), true);
    out << definitions <<
        "const int TYPE_TABLE_LENGTH = " << get_type_count() << ";\n"
        "static const TypeInfo* type_table[TYPE_TABLE_LENGTH];\n\n" <<
        core_functions;

    emit_all_primitives(out);
    out << "}\n";
    out.close();

    if (gen_extras) {
        // Initializer
        std::cout << "[" << draw_symbol('*', Color::Green) << "] generating 'type_titan.init.h'\n";

        fs::path init_path = output_dir;
        init_path.replace_filename("type_titan.init.h");
        out.open(init_path);

        emit_initializer(out);
        out.close();

        // Include
        std::cout << "[" << draw_symbol('*', Color::Green) << "] generating 'type_titan.inc.h'\n";

        fs::path inc_path = output_dir;
        inc_path.replace_filename("type_titan.inc.h");
        out.open(inc_path);

        out <<
            "// This file was generated by TypeTitan\n"
            "#pragma once\n"
            "#include \"type_titan.h\"\n\n";

        for (auto& file : files) {
            fs::path tt_file = file;
            tt_file.replace_extension("tt" + tt_file.extension().generic_string());

            fs::path rel = fs::path(fs::relative(tt_file, output_dir), fs::path::generic_format);
            out << "#include \"" << rel.generic_string() << "\"\n";
        }

        out.close();

        // Utilities
        std::cout << "[" << draw_symbol('*', Color::Green) << "] generating 'type_titan.util.h'\n";

        fs::path util_path = output_dir;
        util_path.replace_filename("type_titan.util.h");
        out.open(util_path);

        out << utils_h;
        out.close();
    }

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

    std::cout <<
        "\n[" << draw_symbol('~', Color::Cyan) << "] finished in " << (double)duration.count() / 1000.0 << "\n";

    return 0;
}

void print_help(int argc, char** argv) {
    std::string default_commands = "";
    for (int i = 0; i < DEFAULT_ARGS_COUNT; i++) {
        default_commands += DEFAULT_ARGS[i];

        if (i + 1 < DEFAULT_ARGS_COUNT) {
            default_commands += ", ";
        }
    }

    std::cout << std::boolalpha <<
        "Usage: " << argv[0] << " <output dir> [options...] <files...>\n"
        " [ARGUMENTS]        :\n"
        "                    :\n"
        "    <output dir>    : the directory where the core type titan files will be generated\n"
        "                    : each .tt.h file will include the files in this directory\n"
        "                    : it will also recursively scan from this directory for clean-up\n"
        "                    :\n"
        "    [options...]    : the command-line options, keep in mind that these are parsed as\n"
        "                    : they come in, making `src/one/*.h -no-recursive src/two/*.h` possible\n"
        "                    :\n"
        "    <files...>      : the input files, wildcards like `src/*.h` are possible\n"
        "                    :\n"
        " [OPTIONS]          :\n"
        "                    :\n"
        "    -help           : shows this help screen\n"
        "                    : default: false\n"
        "                    :\n"
        "    -namespace      : sets the output namespace\n"
        "                    : default: " << DEFAULT_NAMESPACE << "\n"
        "                    :\n"
        "    -no-extas       : disables the generation of inc, init and util files\n"
        "                    : default: " << DEFAULT_NO_EXTRAS << "\n"
        "                    :\n"
        "    -no-empty       : deletes any files with 0 exported types\n"
        "                    : default: " << DEFAULT_NO_EMPTY << "\n"
        "                    :\n"
        "    -no-recursive   : disables recursive search on wildcard entries\n"
        "                    : default: " << DEFAULT_NO_RECURSIVE << "\n"
        "                    :\n"
        "    -clang          : passes all subsequent commands to the clang parser\n"
        "                    : default: " << default_commands << "\n"
        "                    :\n";
}
