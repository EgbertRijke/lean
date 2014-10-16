/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "kernel/expr.h"
#include "library/expr_lt.h"

namespace lean {
bool is_lt(expr const & a, expr const & b, bool use_hash) {
    if (is_eqp(a, b))                    return false;
    unsigned wa = get_weight(a);
    unsigned wb = get_weight(b);
    if (wa < wb)                         return true;
    if (wa > wb)                         return false;
    if (a.kind() != b.kind())            return a.kind() < b.kind();
    if (use_hash) {
        if (a.hash() < b.hash())         return true;
        if (a.hash() > b.hash())         return false;
    }
    if (is_equal(a, b))                  return false;
    switch (a.kind()) {
    case expr_kind::Var:
        return var_idx(a) < var_idx(b);
    case expr_kind::Constant:
        if (const_name(a) != const_name(b))
            return const_name(a) < const_name(b);
        else
            return is_lt(const_levels(a), const_levels(b), use_hash);
    case expr_kind::App:
        if (!is_equal(app_fn(a), app_fn(b)))
            return is_lt(app_fn(a), app_fn(b), use_hash);
        else
            return is_lt(app_arg(a), app_arg(b), use_hash);
    case expr_kind::Lambda: case expr_kind::Pi:
        if (!is_equal(binding_domain(a), binding_domain(b)))
            return is_lt(binding_domain(a), binding_domain(b), use_hash);
        else
            return is_lt(binding_body(a), binding_body(b), use_hash);
    case expr_kind::Sort:
        return is_lt(sort_level(a), sort_level(b), use_hash);
    case expr_kind::Local: case expr_kind::Meta:
        if (mlocal_name(a) != mlocal_name(b))
            return mlocal_name(a) < mlocal_name(b);
        else
            return is_lt(mlocal_type(a), mlocal_type(b), use_hash);
    case expr_kind::Macro:
        if (macro_def(a) != macro_def(b))
            return macro_def(a) < macro_def(b);
        if (macro_num_args(a) != macro_num_args(b))
            return macro_num_args(a) < macro_num_args(b);
        for (unsigned i = 0; i < macro_num_args(a); i++) {
            if (!is_equal(macro_arg(a, i), macro_arg(b, i)))
                return is_lt(macro_arg(a, i), macro_arg(b, i), use_hash);
        }
        return false;
    }
    lean_unreachable(); // LCOV_EXCL_LINE
}
}
