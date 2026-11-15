/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include <string.h>

#include "result/result.h"
#include "stringview.h"

arc_stringview_t arc_stringview_make(const char* data, size_t length) {
    if(!data && length > 0)
        arc_abort("arc StringView: invalid data pointer for non-zero length");

    if(data && length == 0)
        arc_abort("arc StringView: invalid length for non-null data pointer");

    return (arc_stringview_t){
        .data   = data,
        .length = length,
    };
}

arc_stringview_t arc_stringview(const char* cstr) {
    if(!cstr)
        return (arc_stringview_t) {0};
    
    return arc_stringview_make(cstr, strlen(cstr));
}

uint64_t arc_stringview_hash(const arc_stringview_t* sv) {
    // no idea bro
    return 0;
}

uint64_t arc_stringview_hash_ex(const arc_stringview_t* sv, uint64_t seed) {
    // no idea bro
    return 0;
}

bool arc_stringview_equals(const arc_stringview_t* sv1, const arc_stringview_t* sv2) {
    if(!sv1 || !sv2)
        return false;

    if(sv1 == sv2)
        return true;

    if(sv1->length != sv2->length)
        return false;

    if(sv1->data == sv2->data)
        return true;

    return (memcmp(sv1->data, sv2->data, sv1->length) == 0);
}
