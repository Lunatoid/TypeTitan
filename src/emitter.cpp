#include "emitter.h"

#include <cstdint>
#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <memory>

#include "helper.h"
#include "source_code.h"

struct Primitive {
    CXTypeKind kind;
    std::string type_name;
    std::string qualified_type_name;

    // If this primitive is Foo*** then `deepest` is Foo
    std::shared_ptr<Primitive> deepest;

    std::string underlying_name;
    long long array_length;
};

// All the primitives to emit
// key: qualified name
// val: primitive type handle
static std::map<std::string, Primitive> primitives_to_emit;

// All the free functions that have been emitted
// key: qualified name/signature
// val: the function name
static std::unordered_map<std::string, std::string> emitted_functions;

// If `type` is a primitive type, add it to all types that are going to be emitted
bool add_primitive_type(CXType type);

// Emits the start of the `Type<>` class and `info()` function with basic info filled out
void emit_common_start(std::ostream& output, std::string type_kind, std::string type_name,
                       std::string qualified_type_name, std::string template_args = "");

// Ends the `info()` function, does not end the `Type<>` class
void emit_common_end(std::ostream& output);

// Emits tags for whatever the current cursor is
void emit_tags(std::ostream& output, CXCursor cursor, std::vector<std::string>& args,
               std::string lhs = "type", std::string array_name = "tags");

// Emits the parameters for the function
void emit_parameters(std::ostream& output, CXCursor type, std::string lhs = "type", std::string array_name = "parameters");

// Adds nested types for ConstantArrays, Pointers and L/R references
void add_nested_types(CXType type);

// Adds all nested types, e.g.
// int*** -> int** -> int* -> int
// int[3][3][3] -> int[3][3] -> int[3] -> int
// int&& -> int
// int& -> int
bool add_nested_types(CXType type, CXTypeKind kind, CXType(get_nested)(CXType));

// Creates a `Primitive` struct from `type`
void create_primitive(CXType type, Primitive& p);

// int*** -> int
CXType get_deepest_type(CXType type);

// Emitting the specific types
void emit_cursor(std::ostream& output, CXCursor cursor, std::vector<std::string>& args);
void emit_record_generic(std::ostream& output, CXCursor cursor, std::vector<std::string>& args,
                         std::string qualified_name, std::string template_args);
void emit_template_record(std::ostream& output, CXCursor cursor, std::vector<std::string>& args);
void emit_record(std::ostream& output, CXCursor cursor, std::vector<std::string>& args);
void emit_enum(std::ostream& output, CXCursor cursor, std::vector<std::string>& args);
void emit_function(std::ostream& output, CXCursor cursor, std::vector<std::string>& args);
void emit_primitive(std::ostream& output, const Primitive& type);
int emit_dependent_types(std::ostream& output);

//
// Implementations
//

int emit_eligable_children(std::ostream& output, CXCursor cursor) {
    struct ChildData {
        int emitted = 0;
        std::ostream& output;
    } cd = { 0, output };

    clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData data) {
        ChildData* cd = (ChildData*)data;

        // Only parse things from the main file
        if (!clang_Location_isFromMainFile(clang_getCursorLocation(c))) {
            return CXChildVisit_Continue;
        }

        std::vector<std::string> args;
        if (get_args(c, args)) {
            std::ostringstream tmp;

            cd->emitted += 1;
            emit_cursor(tmp, c, args);
            cd->emitted += emit_dependent_types(cd->output);
            cd->output << tmp.str();
        }

        return CXChildVisit_Recurse;
    }, &cd);

    cd.emitted += emit_dependent_types(output);
    return cd.emitted;
}

void emit_cursor(std::ostream& output, CXCursor cursor, std::vector<std::string>& args) {

    // Check if we're a nested type of a template type
    // e.g.
    //
    // >| template<typename T>
    // >| struct Foo {
    // >|
    // >|     int a;
    // >|
    // >|     //!!
    // >|     struct Bar {
    // >|         int b;
    // >|     };
    // >|
    // >| };
    //
    // We can't create TypeInfo for Bar because it's a dependent type
    // of Foo.

    CXCursor parent = clang_getCursorLexicalParent(cursor);
    while (parent.kind != CXCursor_TranslationUnit) {
        if (parent.kind == CXCursor_ClassTemplate) {
            std::cout <<
                "[" << draw_symbol('!', Color::Red) <<
                "] can't index '" << ClangStr(clang_getCursorSpelling(cursor)).c_str() <<
                "' because it's a dependent type\n";
            return;
        }

        parent = clang_getCursorLexicalParent(parent);
    }

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_TypedefDecl: {
            CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
            add_primitive_type(clang_getCanonicalType(underlying));
            break;
        }

        case CXCursor_ClassTemplate:
            emit_template_record(output, cursor, args);
            break;

        case CXCursor_StructDecl:
        case CXCursor_ClassDecl:
        case CXCursor_UnionDecl:
            emit_record(output, cursor, args);
            break;

        case CXCursor_EnumDecl:
            emit_enum(output, cursor, args);
            break;

        case CXCursor_FunctionDecl:
            emit_function(output, cursor, args);
            break;
    }
}

void add_common_primitives(const std::vector<std::string>& clang_args) {
    CXTranslationUnit unit = parse_translation_unit(common_primitives_h, "primitives.h", clang_args);

    if (!unit) return;

    std::ostringstream dummy;
    emit_eligable_children(dummy, clang_getTranslationUnitCursor(unit));
}

void emit_common_file_start(std::ostream& output, const char* type_titan_inc, const char* orig_file_name,
                            const char* namespace_name, bool is_core_file) {
    output <<
        "// This file was generated by TypeTitan\n"
        "#pragma once\n";

    if (is_core_file) {
        output <<
            "#include <stdint.h>\n"
            "#include <string.h>\n";
    } else {
        output <<
            "#include " << type_titan_inc << "\n"
            "#include \"" << orig_file_name << "\"\n";
    }
    output << "\nnamespace " << namespace_name << " {\n\n";
}

void emit_all_primitives(std::ostream& output) {
    for (auto& type : primitives_to_emit) {
        emit_primitive(output, type.second);
    }
}

void emit_template_record(std::ostream& output, CXCursor cursor, std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
        return;
    }

    // clang_getCursorSpelling doesn't return the namespace so we construct
    // it on our own
    CXCursor parent = clang_getCursorLexicalParent(cursor);
    std::string prefix = "";

    while (parent.kind != CXCursor_TranslationUnit) {
        prefix += ClangStr(clang_getCursorSpelling(parent)).c_str();
        prefix += "::";

        parent = clang_getCursorLexicalParent(parent);
    }

    std::string type_name = ClangStr(clang_getCursorSpelling(cursor)).c_str();

    struct TemplateData {
        std::vector<CXCursor> templates;
        std::vector<std::string> template_type;
    } data;

    clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData data) {
        TemplateData* td = (TemplateData*)data;

        switch (clang_getCursorKind(c)) {
            case CXCursor_TemplateTypeParameter:
                td->templates.push_back(c);
                td->template_type.push_back("typename");
                break;

            case CXCursor_NonTypeTemplateParameter:
                td->templates.push_back(c);

                ClangStr type = clang_getTypeSpelling(clang_getCursorType(c));
                td->template_type.push_back(type.c_str());
                break;
        }

        return CXChildVisit_Continue;
    }, &data);


    std::string template_decl = ""; // typename T, int i
    std::string template_args = ""; // T, i

    for (int i = 0; i < data.templates.size(); i++) {

        ClangStr qualified_t = clang_getCursorSpelling(data.templates[i]);

        template_decl += data.template_type[i] + " " + qualified_t.c_str();
        template_args += qualified_t.c_str();

        if (i + 1 < data.templates.size()) {
            template_args += ", ";
            template_decl += ", ";
        }
    }

    std::string qualified_name = prefix + ClangStr(clang_getCursorSpelling(cursor)).c_str();
    qualified_name += "<" + template_args + ">";

    emit_common_start(output, "Record", type_name, qualified_name, template_decl);

    output << "            type.size = sizeof(" << qualified_name << ");\n\n";

    emit_record_generic(output, cursor, args, qualified_name.c_str(), template_decl);
}

void emit_record(std::ostream& output, CXCursor cursor, std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
        return;
    }

    CXType type = clang_getCursorType(cursor);
    ClangStr type_name = clang_getCursorSpelling(cursor);
    ClangStr qualified_name = clang_getTypeSpelling(type);

    emit_common_start(output, "Record", type_name.c_str(), qualified_name.c_str());

    output << "            type.size = sizeof(" << qualified_name.c_str() << ");\n\n";

    emit_record_generic(output, cursor, args, qualified_name.c_str(), "");
}

void emit_record_generic(std::ostream& output, CXCursor cursor, std::vector<std::string>& args,
                         std::string qualified_name, std::string template_decl) {
    emit_tags(output, cursor, args);

    std::string record_type = "Struct";

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_ClassDecl:
            record_type = "Class";
            break;

        case CXCursor_UnionDecl:
            record_type = "Union";
            break;
    }

    output << "            type.record_type = RecordType::" << record_type << ";\n";

    struct RecordData {
        std::vector<std::string> parents;
        std::vector<CXCursor> fields;
        std::vector<CXCursor> methods;
    } data;

    clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData data) {
        RecordData* rd = (RecordData*)data;

        switch (clang_getCursorKind(c)) {
            case CXCursor_CXXBaseSpecifier: {
                ClangStr qualified_name = clang_getTypeSpelling(clang_getCursorType(c));
                rd->parents.push_back(qualified_name.c_str());
                break;
            }

            case CXCursor_FieldDecl: {
                std::vector<std::string> args;
                if (get_args(c, args)) {
                    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
                        return CXChildVisit_Continue;
                    }
                }

                rd->fields.push_back(c);
                break;
            }

            case CXCursor_CXXMethod: {
                std::vector<std::string> args;
                if (get_args(c, args)) {
                    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
                        return CXChildVisit_Continue;
                    }
                }

                rd->methods.push_back(c);
                break;
            }
        }

        return CXChildVisit_Continue;
    }, &data);

    output << "            type.parent_count = " << data.parents.size() << ";\n";

    if (!data.parents.empty()) {
        output << "            static const TypeInfo* parents[" << data.parents.size() << "];\n";

        for (int i = 0; i < data.parents.size(); i++) {
            output << "            parents[" << i << "] = type_of<" << data.parents[i] << ">();\n";
        }

        output << "\n            type.parents = parents;";
    } else {
        output << "            type.parents = nullptr;\n";
    }

    output << "\n            type.field_count = " << data.fields.size() << ";\n";

    if (!data.fields.empty()) {
        output << "            static RecordField fields[" << data.fields.size() << "];\n";

        for (int i = 0; i < data.fields.size(); i++) {
            // We get the canonical type because with some template types it would skip
            // the namespace
            CXType cursor_type = clang_getCursorType(data.fields[i]);

            // If the type is a template parameter then the canonical type is something like
            // type-parameter-0-0 instead of simply T
            CXType field_type = get_deepest_type(cursor_type);
            if (field_type.kind == CXType_Unexposed) {
                field_type = cursor_type;
            }

            ClangStr field_name = clang_getTypeSpelling(field_type);
            ClangStr name = clang_getCursorSpelling(data.fields[i]);

            output <<
                "            fields[" << i << "].type_info = type_of<" << field_name.c_str() << ">();\n"
                "            fields[" << i << "].name = \"" << name.c_str() << "\";\n";

            std::vector<std::string> field_args;
            get_args(data.fields[i], field_args);

            std::string i_str = std::to_string(i);
            emit_tags(output, data.fields[i], field_args, "fields[" + i_str + "]", "tags_" + i_str);

            long long offset = clang_Cursor_getOffsetOfField(data.fields[i]);
            output << "            fields[" << i << "].offset = " << offset / 8 << ";\n";

            std::string access = "Public";
            switch (clang_getCXXAccessSpecifier(data.fields[i])) {
                case CX_CXXPrivate:
                    access = "Public";
                    break;

                case CX_CXXProtected:
                    access = "Protected";
                    break;
            }

            output << "            fields[" << i << "].access = RecordAccess::" << access << ";\n\n";

            add_nested_types(field_type);
        }

        output << "            type.fields = fields;\n\n";
    } else {
        output << "            type.fields = nullptr;\n\n";
    }

    output << "            type.method_count = " << data.methods.size() << ";\n";

    if (!data.methods.empty()) {
        output << "            static TypeInfoFunction methods[" << data.methods.size() << "];\n";

        for (int i = 0; i < data.methods.size(); i++) {
            // We're getting the canonical type for the same reason as with the fields
            CXType method_type = clang_getCanonicalType(clang_getCursorType(data.methods[i]));
            CXType return_type = clang_getCanonicalType(clang_getResultType(method_type));

            add_nested_types(return_type);

            ClangStr type_name = clang_getTypeSpelling(method_type);
            ClangStr name = clang_getCursorSpelling(data.methods[i]);
            ClangStr return_type_qualified = clang_getTypeSpelling(return_type);


            output <<
                "            methods[" << i << "].type = TypeInfoType::Function;\n"
                "            methods[" << i << "].type_id = " << std::hash<std::string>{}(type_name.c_str()) << ";\n"
                "            methods[" << i << "].type_name = \"" << type_name.c_str() << "\";\n\n"
                "            methods[" << i << "].name = \"" << name.c_str() << "\";\n"
                "            methods[" << i << "].return_type = type_of<" << return_type_qualified.c_str() << ">();\n";

            std::vector<std::string> method_args;
            get_args(data.methods[i], method_args);

            std::string i_str = std::to_string(i);
            emit_tags(output, data.methods[i], method_args, "methods[" + i_str + "]", "param_tags_" + i_str);

            std::string method_abstractness = "Normal";
            if (clang_CXXMethod_isPureVirtual(data.methods[i])) {
                method_abstractness = "Pure";
            } else if (clang_CXXMethod_isVirtual(data.methods[i])) {
                method_abstractness = "Virtual";
            }

            output << "            methods[" << i << "].method_type = MethodType::" << method_abstractness << ";\n\n";

            emit_parameters(output, data.methods[i], "methods[" + i_str + "]", "method_params_" + i_str);

            std::string access = "Public";
            switch (clang_getCXXAccessSpecifier(data.methods[i])) {
                case CX_CXXPrivate:
                    access = "Public";
                    break;

                case CX_CXXProtected:
                    access = "Protected";
                    break;
            }

            output <<
                "            methods[" << i << "].access = RecordAccess::" << access << ";\n\n";

        }

        output << "            type.methods = methods;\n\n";
    } else {
        output << "            type.methods = nullptr;\n\n";
    }

    emit_common_end(output);

    output <<
        "\n    template<typename Result, typename... Args>\n"
        "    static Result call(" << qualified_name << "& t, bool& success, const char* name, Args... args) {\n"
        "        success = false;\n\n";

    std::unordered_map<std::string, std::vector<CXCursor>> callables;

    for (auto& method : data.methods) {
        if (clang_getCXXAccessSpecifier(method) != CX_CXXPublic) {
            continue;
        }

        ClangStr name = clang_getCursorSpelling(method);
        callables[name.c_str()].push_back(method);
    }

    for (auto& pair : callables) {
        if (pair.second.size() > 1) {
            output << "        if (strcmp(name, \"" << pair.first << "\") == 0) {\n";

            for (auto& method : pair.second) {
                std::string func_args = "";

                clang_visitChildren(method, [](CXCursor c, CXCursor parent, CXClientData data) {
                    ClangStr qualified_name = clang_getTypeSpelling(clang_getCursorType(c));

                    std::string& func_args = (*(std::string*)data);

                    if (!func_args.empty()) {
                        func_args += ", ";
                    }
                    func_args += qualified_name.c_str();

                    return CXChildVisit_Continue;
                }, &func_args);

                ClangStr return_value = clang_getTypeSpelling(clang_getResultType(clang_getCursorType(method)));

                output <<
                    "            if (Callable<" << qualified_name << ", Result, Args...>().valid("
                    "static_cast<" << return_value.c_str() << "(" << qualified_name << "::*)(" <<
                    func_args << ")>(&" << qualified_name << "::" << pair.first << "))) {\n"
                    "                "
                    "return Callable<" << qualified_name << ", Result, Args...>().call"
                    "(static_cast<" << return_value.c_str() << "(" << qualified_name << "::*)(" << func_args << ")>"
                    "(&" << qualified_name << "::" << pair.first << "), success, t, args...);\n            }\n\n";
            }

            output << "        }\n";
        } else {
            output <<
                "        if (strcmp(name, \"" << pair.first << "\") == 0) {\n"
                "            return Callable<" << qualified_name << ", Result, Args...>()"
                ".call(&" << qualified_name << "::" << pair.first << ", success, t, args...);\n"
                "        }\n";
        }
    }

    output <<
        "\n        return Result();\n"
        "    }\n"
        "};\n\n";
}

void emit_enum(std::ostream& output, CXCursor cursor, std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
        return;
    }

    CXType type = clang_getCursorType(cursor);
    ClangStr type_name = clang_getCursorSpelling(cursor);
    ClangStr qualified_name = clang_getTypeSpelling(type);

    emit_common_start(output, "Enum", type_name.c_str(), qualified_name.c_str());

    output << "            type.size = sizeof(" << qualified_name.c_str() << ");\n\n";

    emit_tags(output, cursor, args);

    struct EnumData {
        std::vector<std::pair<std::string, long long>> enums;
        CXSourceRange last_range = clang_getNullRange();
    } data;

    clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData data) {
        if (clang_getCursorKind(c) != CXCursor_EnumConstantDecl) {
            return CXChildVisit_Continue;
        }

        EnumData* ed = (EnumData*)data;

        // clang_Cursor_getBriefCommentText() likes to get the comment
        // text for previous enum constants if the current one has none,
        // so we save the range so we can see if they're actually different.
        std::vector<std::string> enum_args;
        if (get_args(c, enum_args)) {
            CXSourceRange comment_range = clang_Cursor_getCommentRange(c);

            if (!clang_Range_isNull(comment_range) &&
                !clang_equalRanges(comment_range, ed->last_range)) {
                ed->last_range = comment_range;

                if (std::find(enum_args.begin(), enum_args.end(), "DoNotIndex") != enum_args.end()) {
                    return CXChildVisit_Continue;
                }
            }
        }

        ClangStr name = clang_getCursorSpelling(c);
        long long value = clang_getEnumConstantDeclValue(c);

        ed->enums.push_back({ name.c_str(), value });

        return CXChildVisit_Continue;
    }, &data);

    CXType underlying = clang_getEnumDeclIntegerType(cursor);
    ClangStr underlying_name = clang_getTypeSpelling(underlying);

    output <<
        "            type.underlying = type_of<" << underlying_name.c_str() << ">();\n"
        "            type.enum_count = " << data.enums.size() << ";\n"
        "            static const char* enum_names[] = {\n";

    for (auto& pair : data.enums) {
        output << "            \"" << pair.first << "\",\n";
    }

    output <<
        "            };\n"
        "            type.enum_names = enum_names;\n"
        "            static int64_t enum_values[] = {\n";

    for (auto& pair : data.enums) {
        output << "                " << pair.second << ",\n";
    }

    output <<
        "            };\n"
        "            type.enum_values = enum_values;\n";

    emit_common_end(output);
    output << "};\n\n";
}

void emit_function(std::ostream& output, CXCursor cursor, std::vector<std::string>& args) {
    if (std::find(args.begin(), args.end(), "DoNotIndex") != args.end()) {
        return;
    }

    CXType type = clang_getCursorType(cursor);
    ClangStr qualified_name = clang_getTypeSpelling(type);

    if (emitted_functions.find(qualified_name.c_str()) != emitted_functions.end()) {

        printf("[/] can't index '%s', already indexed '%s' which has the same signature\n",
               qualified_name.c_str(), emitted_functions[qualified_name.c_str()].c_str());

        return;
    }

    ClangStr name = clang_getCursorSpelling(cursor);

    emitted_functions.insert({ qualified_name.c_str(), name.c_str() });

    ClangStr type_name = clang_getTypeSpelling(type);
    ClangStr return_type_qualified = clang_getTypeSpelling(clang_getResultType(type));

    emit_common_start(output, "Function", type_name.c_str(), qualified_name.c_str());

    output <<
        "\n            type.name = \"" << name.c_str() << "\";\n"
        "            type.return_type = type_of<" << return_type_qualified.c_str() << ">();\n";

    emit_parameters(output, cursor);
    output << "            type.access = RecordAccess::Public;";

    emit_common_end(output);
    output << "};\n\n";

    add_primitive_type(type);
}

void emit_common_start(std::ostream& output, std::string type_kind, std::string type_name,
                       std::string qualified_type_name, std::string template_args) {
    std::string suffix = "";
    if (type_kind != "Primitive") {
        suffix = type_kind;
    }

    output <<
        "template<" << template_args << ">\n"
        "struct Type<" << qualified_type_name << "> {\n"
        "    static const TypeInfo* info() {\n"
        "        static TypeInfo" << suffix << " type;\n\n"

        "        if (type.type_id == 0) {\n"
        "            type.type = TypeInfoType::" << type_kind << ";\n"
        "            type.type_name = \"" << type_name << "\";\n"
        "            type.type_id = " << std::hash<std::string>{}(qualified_type_name.c_str()) << ";\n";
}

void emit_common_end(std::ostream& output) {
    output <<
        "        }\n\n"
        "        return &type;\n"
        "    }\n";
}

void emit_tags(std::ostream& output, CXCursor cursor, std::vector<std::string>& args,
               std::string lhs, std::string array_name) {
    std::vector<std::string> tags;

    for (int i = 0; i < args.size(); i++) {
        if (args[i].compare("Tags=")) {
            args[i].erase(args[i].begin(), args[i].begin() + 5);

            std::stringstream ss(args[i]);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                tags.push_back(tag);
            }
            break;
        }
    }

    if (tags.size() > 0) {
        output <<
            "            " << lhs << ".tag_count = " << tags.size() << ";\n"
            "            static const char* " << array_name << "[] = {\n";

        for (auto& tag : tags) {
            output << "                \"" << tag << "\",\n";
        }

        output <<
            "            };\n\n"
            "            " << lhs << ".tags = " << array_name << ";\n";
    } else {
        output <<
            "            " << lhs << ".tag_count = 0;\n"
            "            " << lhs << ".tags = nullptr;\n";
    }
}

void emit_parameters(std::ostream& output, CXCursor cursor, std::string lhs, std::string array_name) {
    CXType type = clang_getCursorType(cursor);
    int arg_count = clang_getNumArgTypes(type);

    output << "            " << lhs << ".parameter_count = " << arg_count << ";\n";

    if (arg_count > 0) {
        output << "            static FunctionParameter " << array_name << "[" << arg_count << "];\n";

        struct ParamData {
            int i;
            std::ostream& output;
            std::string array_name;
        } param_data{ 0, output, array_name };

        clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData data) {
            CXCursorKind kind = clang_getCursorKind(c);

            if (kind != CXCursor_ParmDecl) return CXChildVisit_Continue;

            CXType type = clang_getCursorType(c);
            ClangStr qualified_name = clang_getTypeSpelling(clang_getCursorType(c));
            ClangStr name = clang_getCursorSpelling(c);

            ParamData* pd = (ParamData*)data;

            pd->output <<
                "\n            " << pd->array_name << "[" << pd->i << "].type_info = type_of<" << qualified_name.c_str() << ">();\n"
                "            " << pd->array_name << "[" << pd->i << "].name = \"" << name.c_str() << "\";\n";

            add_nested_types(type);

            pd->i += 1;
            return CXChildVisit_Continue;
        }, &param_data);

        output << "\n            " << lhs << ".parameters = " << array_name << ";\n";
    }
}

void add_nested_types(CXType type) {
    bool any_added = false;
    any_added |= add_nested_types(type, CXType_ConstantArray, clang_getElementType);
    any_added |= add_nested_types(type, CXType_Pointer, clang_getPointeeType);

    if (type.kind == CXType_LValueReference || type.kind == CXType_RValueReference) {
        CXType pointee = clang_getPointeeType(type);
        any_added |= add_primitive_type(pointee);
    }

    if (any_added) {
        add_primitive_type(type);
    }
}

bool add_nested_types(CXType type, CXTypeKind kind, CXType(get_nested)(CXType)) {
    if (type.kind == kind) {
        std::vector<CXType> types;

        CXType elem_type = type;
        while (true) {
            elem_type = get_nested(elem_type);

            if (elem_type.kind != kind) {
                break;
            }

            types.push_back(elem_type);
        }

        std::reverse(types.begin(), types.end());

        if (elem_type.kind == CXType_Unexposed) {
            // This is probably a template type,
            // we don't add those because they
            // are not resolvable primitives
            return false;
        }

        for (auto& type : types) {
            add_primitive_type(type);
        }

        add_primitive_type(elem_type);
        return true;
    }

    return false;
}

CXType get_deepest_type(CXType type) {
    if (type.kind == CXType_ConstantArray) {
        while (true) {
            type = clang_getElementType(type);

            if (type.kind != CXType_ConstantArray) {
                break;
            }
        }
    } else if (type.kind == CXType_Pointer) {
        while (true) {
            type = clang_getPointeeType(type);

            if (type.kind != CXType_Pointer) {
                break;
            }
        }
    } else if (type.kind == CXType_LValueReference || type.kind == CXType_RValueReference) {
        type = clang_getPointeeType(type);
    }

    return type;
}

void create_primitive(CXType type, Primitive& p) {
    

    p.kind = type.kind;

    p.qualified_type_name = ClangStr(clang_getTypeSpelling(type)).c_str();
    p.type_name = p.qualified_type_name;

    if (p.type_name.find(':') != std::string::npos) {
        p.type_name.erase(0, p.type_name.rfind(':') + 1);
    }

    if (type.kind == CXType_ConstantArray) {
        p.array_length = clang_getArraySize(type);
        p.underlying_name = ClangStr(clang_getTypeSpelling(clang_getElementType(type))).c_str();

        p.deepest = std::make_shared<Primitive>();
        create_primitive(get_deepest_type(type), *p.deepest);
    } else if (type.kind == CXType_Pointer ||
               type.kind == CXType_LValueReference ||
               type.kind == CXType_RValueReference) {
        p.underlying_name = ClangStr(clang_getTypeSpelling(clang_getPointeeType(type))).c_str();

        p.deepest = std::make_shared<Primitive>();
        create_primitive(get_deepest_type(type), *p.deepest);
    }
}

void emit_primitive(std::ostream& output, const Primitive& type) {
    std::string suffix = "Primitive";

    if (type.kind == CXType_ConstantArray) {
        suffix = "Array";
    } else if (type.kind == CXType_Pointer ||
               type.kind == CXType_LValueReference ||
               type.kind == CXType_RValueReference) {
        suffix = "Indirect";
    }

    emit_common_start(output, suffix, type.type_name, type.qualified_type_name);

    // We can't do sizeof(void)
    if (type.qualified_type_name != "void" &&
        type.qualified_type_name != "const void") {
        output << "\n            type.size = sizeof(" << type.qualified_type_name << ");\n";
    } else {
        output << "\n            type.size = 0;\n";
    }

    if (type.kind == CXType_ConstantArray) {
        output <<
            "            type.underlying = type_of<" << type.underlying_name << ">();\n"
            "            type.length = " << type.array_length << ";\n";
    } else if (type.kind == CXType_Pointer ||
               type.kind == CXType_LValueReference ||
               type.kind == CXType_RValueReference) {
        output << "            type.underlying = type_of<" << type.underlying_name << ">();\n";

        if (type.kind == CXType_Pointer) {
            output << "            type.indirect_type = IndirectType::Pointer;\n";
        } else if (type.kind == CXType_LValueReference) {
            output << "            type.indirect_type = IndirectType::LReference;\n";
        } else if (type.kind == CXType_RValueReference) {
            output << "            type.indirect_type = IndirectType::RReference;\n";
        }
    }

    emit_common_end(output);
    output << "};\n\n";
}

int emit_dependent_types(std::ostream& output) {
    int emitted = 0;
    // Emit primitives that are based on types from this translation unit
    // e.g. Foo* is a primitive type (pointer), but it's based on Foo (this translation unit)
    std::vector<std::string> to_erase;
    for (auto& type : primitives_to_emit) {
        if (type.second.deepest == nullptr) continue;

        if (type.second.deepest->kind < CXType_FirstBuiltin ||
            type.second.deepest->kind > CXType_LastBuiltin) {
            emit_primitive(output, type.second);

            emitted += 1;
            to_erase.push_back(type.first);
        }
    }

    for (auto& key : to_erase) {
        primitives_to_emit.erase(key);
    }

    return emitted;
}

bool add_primitive_type(CXType type) {
    bool is_primitive = (type.kind >= CXType_FirstBuiltin && type.kind <= CXType_LastBuiltin) ||
        type.kind == CXType_Pointer ||
        type.kind == CXType_ConstantArray ||
        type.kind == CXType_LValueReference ||
        type.kind == CXType_RValueReference;

    ClangStr name = clang_getTypeSpelling(type);

    if (is_primitive && primitives_to_emit.find(name.c_str()) == primitives_to_emit.end()) {
        Primitive p;
        create_primitive(type, p);

        primitives_to_emit[name.c_str()] = p;
        return true;
    }

    return false;
}
