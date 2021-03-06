/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "library/head_map.h"

namespace lean {
head_index::head_index(expr const & e) {
    expr const & f = get_app_fn(e);
    m_kind = f.kind();
    if (is_constant(f))
        m_const_name = const_name(f);
}

int head_index::cmp::operator()(head_index const & i1, head_index const & i2) const {
    if (i1.m_kind != i2.m_kind || i1.m_kind != expr_kind::Constant)
        return static_cast<int>(i1.m_kind) - static_cast<int>(i2.m_kind);
    else
        return quick_cmp(i1.m_const_name, i2.m_const_name);
}
}
