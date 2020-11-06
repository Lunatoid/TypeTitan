#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include <clang-c/Index.h>

// Emits all marked children of this cursor
int emit_eligable_children(std::ostream& output, CXCursor cursor);

// Adds all the common primitives, e.g. int8_t, int16_t, uint64_t, float, etc...
void add_common_primitives(const std::vector<std::string>& clang_args);

// Emits the includes, namespace and more
// `is_core_file` should only be true if this is "type_titan.h" 
void emit_common_file_start(std::ostream& output, const char* type_titan_inc, const char* orig_file_name,
                            const char* namespace_name, bool is_core_file);

// Emits all primitives
void emit_all_primitives(std::ostream& output);
