#include "print.h"

#include <cstdio>
#include <string.h>
#include <stddef.h>

#include "type_titan.util.h"

void print_internal(const tt::TypeInfo* ti, void* data, int indentation);

#define PRINT_PRIMITIVE(type, format) if (ti->type_id == tt::type_of<type>()->type_id) {\
    printf(format, *((type*)data));\
    return;\
}

void print_primitive(const tt::TypeInfo* ti, void* data) {
    if (ti->type_id == 0) {
        printf("(unindexed)");
        return;
    }

    if (ti->type_id == tt::type_of<wchar_t>()->type_id) {
        wprintf(L"%c", *((wchar_t*)data));
        return;
    }

    if (ti->type_id == tt::type_of<char>()->type_id) {
        wprintf(L"%c", *((char*)data));
        return;
    }

    if (ti->type_id == tt::type_of<bool>()->type_id) {
        printf("%s", *((bool*)data) ? "true" : "false");
        return;
    }

    PRINT_PRIMITIVE(int8_t, "%d");
    PRINT_PRIMITIVE(int16_t, "%d");
    PRINT_PRIMITIVE(int32_t, "%ld");
    PRINT_PRIMITIVE(int64_t, "%lld");

    PRINT_PRIMITIVE(uint8_t, "%d");
    PRINT_PRIMITIVE(uint16_t, "%u");
    PRINT_PRIMITIVE(uint32_t, "%lu");
    PRINT_PRIMITIVE(uint64_t, "%llu");

    PRINT_PRIMITIVE(float, "%f");
    PRINT_PRIMITIVE(double, "%f");
}

#undef PRINT_PRIMITIVE

void print_array(const tt::TypeInfo* ti, void* data, int indentation = 0) {
    const tt::TypeInfoArray* tia = tt::cast<tt::TypeInfoArray>(ti);

    if (!tia) return;

    printf("[\n");
    for (int i = 0; i < tia->length; i++) {
        for (int indent = 0; indent < indentation; indent++) {
            printf("    ");
        }
        print_internal(tia->underlying, ((uint8_t*)data) + (uint64_t)tia->underlying->size * i, indentation);
        printf("\n");
    }

    for (int indent = 0; indent < indentation - 1; indent++) {
        printf("    ");
    }

    printf("]");
}

void print_indirect(const tt::TypeInfo* ti, void* data) {
    const tt::TypeInfoIndirect* tii = tt::cast<tt::TypeInfoIndirect>(ti);

    if (!tii) return;

    if (tii->indirect_type == tt::IndirectType::Pointer) {
        if (tii->type_id == tt::type_of<wchar_t*>()->type_id ||
            tii->type_id == tt::type_of<const wchar_t*>()->type_id) {
            wprintf(L"\"%s\"", *((wchar_t**)data));
            return;
        } else if (tii->type_id == tt::type_of<char*>()->type_id ||
            tii->type_id == tt::type_of<const char*>()->type_id) {
            printf("\"%s\"", *((char**)data));
            return;
        }
    }

    printf("0x%p", *((void**)data));
}

void print_function(const tt::TypeInfo* ti) {
    const tt::TypeInfoFunction* tif = tt::cast<tt::TypeInfoFunction>(ti);

    if (!tif) return;

    printf("%s %s(\n", tif->return_type->type_name, tif->type_name);

    for (auto& param : tt::get_parameters(ti)) {
        printf("    %s %s", param.type_info->type_name, param.name);

        if (param.has_value) {
            printf(" [has value]");
        }

        printf("\n");
    }
}

void print_record(const tt::TypeInfo* ti, void* data, int indentation, bool use_braces) {
    if (use_braces) {
        printf("{\n");
    }

    // We only print the first parent because we know it starts at offset 0
    int parent_count = 0;
    for (auto& parent : tt::get_parents(ti)) {
        if (parent_count == 0) {
            print_record(parent, data, indentation, false);
        }

        parent_count += 1;
    }

    const tt::TypeInfoRecord* tir = tt::cast<tt::TypeInfoRecord>(ti);

    if (!tir) return;

    if (tir->record_type == tt::RecordType::Union) {
        for (int indent = 0; indent < indentation; indent++) {
            printf("    ");
        }
        printf("[union]\n");
    } else {
        for (auto& field : tt::get_fields(tir)) {
            if (tt::has_tag(&field, "DoNotPrint")) {
                continue;
            }

            for (int indent = 0; indent < indentation; indent++) {
                printf("    ");
            }

            printf("%s = ", field.name);
            print_internal(field.type_info, ((uint8_t*)data) + field.offset, indentation);
            printf("\n");
        }
    }

    for (int indent = 0; indent < indentation - 1; indent++) {
        printf("    ");
    }

    if (use_braces) {
        printf("}");
    }
}

void print_enum(const tt::TypeInfo* ti, int value) {
    int i = 0;
    for (auto& enum_value : tt::get_enumerator_values(ti)) {
        if (value == enum_value) {
            printf("%s::%s", ti->type_name, tt::get_enumerators(ti)[i]);
            return;
        }

        i += 1;
    }

    // Fallback
    printf("%d", value);
}

void print_internal(const tt::TypeInfo* ti, void* data, int indentation) {
    if (!ti) return;

    if (has_tag(ti, "DoNotPrint")) {
        return;
    }

    switch (ti->type) {
        case tt::TypeInfoType::Primitive:
            print_primitive(ti, data);
            break;

        case tt::TypeInfoType::Indirect:
            print_indirect(ti, data);
            break;

        case tt::TypeInfoType::Array:
            print_array(ti, data, indentation + 1);
            break;

        case tt::TypeInfoType::Function:
            print_function(ti);
            break;

        case tt::TypeInfoType::Record:
            print_record(ti, data, indentation + 1, true);
            break;

        case tt::TypeInfoType::Enum:
            print_enum(ti, *(int*)(data));
            break;
    }
}
