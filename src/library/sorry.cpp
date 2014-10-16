/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "kernel/type_checker.h"
#include "kernel/environment.h"
#include "library/module.h"

namespace lean {
static name * g_sorry_name = nullptr;
static name * g_l          = nullptr;
static expr * g_sorry_type = nullptr;

void initialize_sorry() {
    g_sorry_name = new name("sorry");
    g_l          = new name("l");
    g_sorry_type = new expr(mk_pi("A", mk_sort(mk_param_univ(*g_l)), mk_var(0), binder_info(true)));
}

void finalize_sorry() {
    delete g_sorry_type;
    delete g_l;
    delete g_sorry_name;
}

bool has_sorry(environment const & env) {
    auto decl = env.find(*g_sorry_name);
    return decl && is_equal(decl->get_type(), *g_sorry_type);
}

environment declare_sorry(environment const & env) {
    if (auto decl = env.find(*g_sorry_name)) {
        if (!is_equal(decl->get_type(), *g_sorry_type))
            throw exception("failed to declare 'sorry', environment already has an object named 'sorry'");
        return env;
    } else {
        return module::add(env, check(env, mk_constant_assumption(*g_sorry_name, list<name>(*g_l), *g_sorry_type)));
    }
}

expr mk_sorry() { return mk_constant(*g_sorry_name); }
name const & get_sorry_name() { return *g_sorry_name; }
bool is_sorry(expr const & e) { return is_constant(e) && const_name(e) == get_sorry_name(); }
}
