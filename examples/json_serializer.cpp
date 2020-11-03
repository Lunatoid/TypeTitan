#include "json_serializer.h"

#include <cstdio>

#include "../type_titan/type_titan.util.h"

#define PRINT_PRIMITIVE(type, format) if (ti->type_id == tt::type_of<type>()->type_id) {\
    printf(format, *((type*)data));\
    return;\
}

void json_ser_primitive(const tt::TypeInfo* ti, void* data) {
    if (ti->type_id == 0) {
        printf("null");
        return;
    }

    if (ti->type_id == tt::type_of<wchar_t>()->type_id) {
        wprintf(L"\"%c\"", *((wchar_t*)data));
        return;
    }

    if (ti->type_id == tt::type_of<char>()->type_id) {
        wprintf(L"\"%c\"", *((char*)data));
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


void json_ser_array(const tt::TypeInfo* ti, void* data, int indentation) {
    const tt::TypeInfoArray* tia = tt::cast<tt::TypeInfoArray>(ti);

    if (!tia) return;

    printf("[\n");

    for (int i = 0; i < tia->length; i++) {
        for (int indent = 0; indent < indentation; indent++) {
            printf("    ");
        }
        json_serialize_internal(tia->underlying, ((uint8_t*)data) + (uint64_t)tia->underlying->size * i, indentation + i);

        if (i + 1 < tia->length) {
            printf(",");
        }

        printf("\n");
    }

    for (int indent = 0; indent < indentation - 1; indent++) {
        printf("    ");
    }
    printf("]");
}

void json_ser_indirect(const tt::TypeInfo* ti, void* data) {
    const tt::TypeInfoIndirect* tii = tt::cast<tt::TypeInfoIndirect>(ti);

    if (!tii) return;

    int id = tt::get_underlying(ti)->type_id;

    if (id == tt::type_of<char*>()->type_id ||
        id == tt::type_of<const char*>()->type_id) {
        printf("\"%s\"", *((char**)data));
        return;
    }

    if (id == tt::type_of<wchar_t*>()->type_id ||
        id == tt::type_of<const wchar_t*>()->type_id) {
        wprintf(L"\"%s\"", *((wchar_t**)data));
        return;
    }

    printf("null");
}

void json_ser_record(const tt::TypeInfo* ti, void* data, int indentation, bool use_braces = true) {
    const tt::TypeInfoRecord* tir = tt::cast<tt::TypeInfoRecord>(ti);

    if (!tir || tir->record_type == tt::RecordType::Union) {
        printf("null");
        return;
    }

    if (use_braces) {
        printf("{\n");
    }

    if (tir->parent_count == 1) {
        json_ser_record(tir->parents[0], data, indentation, false);
    }

    // Count all serializable fields
    int field_count = 0;
    for (auto& field : tt::get_fields(ti)) {
        if (!tt::has_tag(&field, "DoNotSerialize")) {
            field_count += 1;
        }
    }

    int i = 0;
    for (auto& field : tt::get_fields(ti)) {
        i += 1;

        if (tt::has_tag(&field, "DoNotSerialize")) {
            continue;
        }

        for (int indent = 0; indent < indentation; indent++) {
            printf("    ");
        }

        printf("\"%s\": ", field.name);
        json_serialize_internal(field.type_info, ((uint8_t*)data) + field.offset, indentation);

        if (i < field_count) {
            printf(",");
        }

        printf("\n");
    }

    for (int indent = 0; indent < indentation - 1; indent++) {
        printf("    ");
    }

    if (use_braces) {
        printf("}");
    }
}


void json_ser_enum(const tt::TypeInfo* ti, void* data) {
    const tt::TypeInfoEnum* tie = tt::cast<tt::TypeInfoEnum>(ti);

    if (!tie) return;

    int value = *((int*)data);

    printf("%d", value);
}

void json_serialize_internal(const tt::TypeInfo* ti, void* data, int indentation) {
    switch (ti->type) {
        case tt::TypeInfoType::Primitive:
            json_ser_primitive(ti, data);
            break;

        case tt::TypeInfoType::Array:
            json_ser_array(ti, data, indentation + 1);
            break;

        case tt::TypeInfoType::Indirect:
            json_ser_indirect(ti, data);
            break;

        case tt::TypeInfoType::Record:
            json_ser_record(ti, data, indentation + 1);
            break;

        case tt::TypeInfoType::Enum:
            json_ser_enum(ti, data);
            break;
    }
}
