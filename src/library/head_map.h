/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include "util/rb_map.h"
#include "util/list.h"
#include "kernel/expr.h"

namespace lean {
struct head_index {
    expr_kind m_kind;
    name      m_const_name;
    explicit head_index(expr_kind k = expr_kind::Var):m_kind(k) {}
    explicit head_index(name const & c):m_kind(expr_kind::Constant), m_const_name(c) {}
    head_index(expr const & e);

    struct cmp {
        int operator()(head_index const & i1, head_index const & i2) const;
    };
};

/**
    \brief Very simple indexing data-structure that allow us to map the head symbol of an expression to
    a list of values.

    The index is the expression kind and a name (if it is a constant).

    This index is very naive, but it is effective for many applications.
*/
template<typename V>
class head_map {
    rb_map<head_index, list<V>, head_index::cmp> m_map;
public:
    bool empty() const { return m_map.empty(); }
    bool contains(head_index const & h) const { return m_map.contains(h); }
    list<V> const * find(head_index const & h) const { return m_map.find(h); }
    void erase(head_index const & h) { m_map.erase(h); }
    void erase_entry(head_index const & h, V const & v) {
        if (auto it = m_map.find(h)) {
            auto new_vs = filter(*it, [&](V const & v2) { return !is_equal(v, v2); });
            if (!new_vs)
                m_map.erase(h);
            else
                m_map.insert(h, new_vs);
        }
    }
    void insert(head_index const & h, V const & v) {
        if (auto it = m_map.find(h))
            m_map.insert(h, cons(v, filter(*it, [&](V const & v2) { return !is_equal(v, v2); })));
        else
            m_map.insert(h, to_list(v));
    }
    template<typename F> void for_each(F && f) const { m_map.for_each(f); }
    template<typename F> void for_each_entry(F && f) const {
        m_map.for_each([&](head_index const & h, list<V> const & vs) {
                for (V const & v : vs)
                    f(h, v);
            });
    }
};
}
