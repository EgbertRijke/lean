/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <algorithm>
#include "util/sstream.h"
#include "kernel/abstract.h"
#include "kernel/instantiate.h"
#include "kernel/replace_fn.h"
#include "kernel/error_msgs.h"
#include "kernel/for_each_fn.h"
#include "library/scoped_ext.h"
#include "library/locals.h"
#include "library/explicit.h"
#include "frontends/lean/parser.h"
#include "frontends/lean/tokens.h"

namespace lean {
bool parse_persistent(parser & p, bool & persistent) {
    if (p.curr_is_token_or_id(get_persistent_tk())) {
        p.next();
        persistent = true;
        return true;
    } else {
        return false;
    }
}

void check_atomic(name const & n) {
    if (!n.is_atomic())
        throw exception(sstream() << "invalid declaration name '" << n << "', identifier must be atomic");
}

void check_in_context(parser const & p) {
    if (!in_context(p.env()))
        throw exception(sstream() << "invalid command, it must be used in a (local) context");
}
bool is_root_namespace(name const & n) {
    return n == get_root_tk();
}
name remove_root_prefix(name const & n) {
    return n.replace_prefix(get_root_tk(), name());
}

// Sort local names by order of occurrence, and copy the associated parameters to ps
void sort_locals(expr_struct_set const & locals, parser const & p, buffer<expr> & ps) {
    for (expr const & l : locals)
        ps.push_back(l);
    std::sort(ps.begin(), ps.end(), [&](expr const & p1, expr const & p2) {
            bool is_var1 = p.is_local_variable(p1);
            bool is_var2 = p.is_local_variable(p2);
            if (!is_var1 && is_var2)
                return true;
            else if (is_var1 && !is_var2)
                return false;
            else
                return p.get_local_index(p1) < p.get_local_index(p2);
        });
}

// Return the local levels in \c ls that are not associated with variables
levels collect_local_nonvar_levels(parser & p, level_param_names const & ls) {
    buffer<level> section_ls_buffer;
    for (name const & l : ls) {
        if (p.get_local_level_index(l) && !p.is_local_level_variable(l))
            section_ls_buffer.push_back(mk_param_univ(l));
        else
            break;
    }
    return to_list(section_ls_buffer.begin(), section_ls_buffer.end());
}

// Collect local constants occurring in type and value, sort them, and store in ctx_ps
void collect_locals(expr const & type, expr const & value, parser const & p, buffer<expr> & ctx_ps) {
    expr_struct_set ls;
    buffer<expr> include_vars;
    p.get_include_variables(include_vars);
    for (expr const & param : include_vars) {
        collect_locals(mlocal_type(param), ls);
        ls.insert(param);
    }
    collect_locals(type, ls);
    collect_locals(value, ls);
    sort_locals(ls, p, ctx_ps);
}

void remove_local_vars(parser const & p, buffer<expr> & locals) {
    unsigned j = 0;
    for (unsigned i = 0; i < locals.size(); i++) {
        expr const & param = locals[i];
        if (!is_local(param) || !p.is_local_variable(param)) {
            locals[j] = param;
            j++;
        }
    }
    locals.shrink(j);
}

list<expr> locals_to_context(expr const & e, parser const & p) {
    expr_struct_set ls;
    collect_locals(e, ls);
    buffer<expr> locals;
    sort_locals(ls, p, locals);
    std::reverse(locals.begin(), locals.end());
    return to_list(locals.begin(), locals.end());
}

expr mk_local_ref(name const & n, levels const & ctx_ls, unsigned num_ctx_params, expr const * ctx_params) {
    buffer<expr> params;
    for (unsigned i = 0; i < num_ctx_params; i++)
        params.push_back(mk_explicit(ctx_params[i]));
    return mk_as_atomic(mk_app(mk_explicit(mk_constant(n, ctx_ls)), params));
}

bool is_local_ref(expr const & e) {
    if (!is_as_atomic(e))
        return false;
    expr const & imp_arg = get_as_atomic_arg(e);
    if (!is_app(imp_arg))
        return false;
    buffer<expr> locals;
    expr const & f = get_app_args(imp_arg, locals);
    return
        is_explicit(f) &&
        is_constant(get_explicit_arg(f)) &&
        std::all_of(locals.begin(), locals.end(),
                    [](expr const & l) {
                        return is_explicit(l) && is_local(get_explicit_arg(l));
                    });
}

expr update_local_ref(expr const & e, name_set const & lvls_to_remove, name_set const & locals_to_remove) {
    lean_assert(is_local_ref(e));
    if (locals_to_remove.empty() && lvls_to_remove.empty())
        return e;
    buffer<expr> locals;
    expr const & f = get_app_args(get_as_atomic_arg(e), locals);
    lean_assert(is_explicit(f));

    expr new_f;
    if (!lvls_to_remove.empty()) {
        expr const & c = get_explicit_arg(f);
        lean_assert(is_constant(c));
        new_f = mk_explicit(update_constant(c, filter(const_levels(c), [&](level const & l) {
                        return is_param(l) && !lvls_to_remove.contains(param_id(l));
                    })));
    } else {
        new_f = f;
    }

    if (!locals_to_remove.empty()) {
        unsigned j = 0;
        for (unsigned i = 0; i < locals.size(); i++) {
            expr const & l = locals[i];
            if (!locals_to_remove.contains(mlocal_name(get_explicit_arg(l)))) {
                locals[j] = l;
                j++;
            }
        }
        locals.shrink(j);
    }

    if (locals.empty()) {
        return get_explicit_arg(new_f);
    } else {
        return mk_as_atomic(mk_app(new_f, locals));
    }
}

expr Fun(buffer<expr> const & locals, expr const & e, parser & p) {
    bool use_cache = false;
    return p.rec_save_pos(Fun(locals, e, use_cache), p.pos_of(e));
}

expr Pi(buffer<expr> const & locals, expr const & e, parser & p) {
    bool use_cache = false;
    return p.rec_save_pos(Pi(locals, e, use_cache), p.pos_of(e));
}

template<bool is_lambda>
static expr mk_binding_as_is(unsigned num, expr const * locals, expr const & b) {
    expr r     = abstract_locals(b, num, locals);
    unsigned i = num;
    while (i > 0) {
        --i;
        expr const & l = locals[i];
        expr t = abstract_locals(mlocal_type(l), i, locals);
        if (is_lambda)
            r = mk_lambda(local_pp_name(l), mk_as_is(t), r, local_info(l));
        else
            r = mk_pi(local_pp_name(l), mk_as_is(t), r, local_info(l));
    }
    return r;
}

expr Fun_as_is(buffer<expr> const & locals, expr const & e, parser & p) {
    return p.rec_save_pos(mk_binding_as_is<true>(locals.size(), locals.data(), e), p.pos_of(e));
}

expr Pi_as_is(buffer<expr> const & locals, expr const & e, parser & p) {
    return p.rec_save_pos(mk_binding_as_is<false>(locals.size(), locals.data(), e), p.pos_of(e));
}

level mk_result_level(environment const & env, buffer<level> const & r_lvls) {
    bool impredicative = env.impredicative();
    if (r_lvls.empty()) {
        return impredicative ? mk_level_one() : mk_level_zero();
    } else {
        level r = r_lvls[0];
        for (unsigned i = 1; i < r_lvls.size(); i++)
            r = mk_max(r, r_lvls[i]);
        r = normalize(r);
        if (is_not_zero(r))
            return normalize(r);
        else
            return impredicative ? normalize(mk_max(r, mk_level_one())) : normalize(r);
    }
}

bool occurs(level const & u, level const & l) {
    bool found = false;
    for_each(l, [&](level_ptr l) {
            if (found) return false;
            if (is_equal(l, u)) { found = true; return false; }
            return true;
        });
    return found;
}

/** \brief Functional object for converting the universe metavariables in an expression in new universe parameters.
    The substitution is updated with the mapping metavar -> new param.
    The set of parameter names m_params and the buffer m_new_params are also updated.
*/
class univ_metavars_to_params_fn {
    environment const &        m_env;
    local_decls<level> const & m_lls;
    substitution &             m_subst;
    name_set &                 m_params;
    buffer<name> &             m_new_params;
    unsigned                   m_next_idx;

    /** \brief Create a new universe parameter s.t. the new name does not occur in \c m_params, nor it is
        a global universe in \e m_env or in the local_decls<level> m_lls.
        The new name is added to \c m_params, and the new level parameter is returned.
        The name is of the form "l_i" where \c i >= m_next_idx.
    */
    level mk_new_univ_param() {
        name l("l");
        name r = l.append_after(m_next_idx);
        while (m_lls.contains(r) || m_params.contains(r) || m_env.is_universe(r)) {
            m_next_idx++;
            r = l.append_after(m_next_idx);
        }
        m_params.insert(r);
        m_new_params.push_back(r);
        return mk_param_univ(r);
    }

public:
    univ_metavars_to_params_fn(environment const & env, local_decls<level> const & lls, substitution & s,
                               name_set & ps, buffer<name> & new_ps):
        m_env(env), m_lls(lls), m_subst(s), m_params(ps), m_new_params(new_ps), m_next_idx(1) {}

    level apply(level const & l) {
        return replace(l, [&](level_ptr l) {
                if (!has_meta(l))
                    return some_level(level(l));
                if (is_meta(l)) {
                    if (auto it = m_subst.get_level(meta_id(l))) {
                        return some_level(*it);
                    } else {
                        level new_p = mk_new_univ_param();
                        m_subst.assign(meta_id(l), new_p);
                        return some_level(new_p);
                    }
                }
                return none_level();
            });
    }

    expr apply(expr const & e) {
        if (!has_univ_metavar(e)) {
            return e;
        } else {
            return replace(e, [&](expr const & e) {
                    if (!has_univ_metavar(e)) {
                        return some_expr(e);
                    } else if (is_sort(e)) {
                        return some_expr(update_sort(e, apply(sort_level(e))));
                    } else if (is_constant(e)) {
                        levels ls = map(const_levels(e), [&](level const & l) { return apply(l); });
                        return some_expr(update_constant(e, ls));
                    } else {
                        return none_expr();
                    }
                });
        }
    }

    expr operator()(expr const & e) { return apply(e); }
};

expr univ_metavars_to_params(environment const & env, local_decls<level> const & lls, substitution & s,
                             name_set & ps, buffer<name> & new_ps, expr const & e) {
    return univ_metavars_to_params_fn(env, lls, s, ps, new_ps)(e);
}

expr instantiate_meta(expr const & meta, substitution & subst) {
    lean_assert(is_meta(meta));
    buffer<expr> locals;
    expr mvar = get_app_args(meta, locals);
    mvar = update_mlocal(mvar, subst.instantiate_all(mlocal_type(mvar)));
    for (auto & local : locals)
        local = subst.instantiate_all(local);
    return mk_app(mvar, locals);
}

justification mk_failed_to_synthesize_jst(environment const & env, expr const & m) {
    return mk_justification(m, [=](formatter const & fmt, substitution const & subst) {
            substitution tmp(subst);
            expr new_m    = instantiate_meta(m, tmp);
            expr new_type = type_checker(env).infer(new_m).first;
            proof_state ps = to_proof_state(new_m, new_type, name_generator("dontcare"));
            return format("failed to synthesize placeholder") + line() + ps.pp(fmt);
        });
}

justification mk_type_mismatch_jst(expr const & v, expr const & v_type, expr const & t, expr const & src) {
    return mk_justification(src, [=](formatter const & fmt, substitution const & subst) {
            substitution s(subst);
            format expected_fmt, given_fmt;
            std::tie(expected_fmt, given_fmt) = pp_until_different(fmt, s.instantiate(t), s.instantiate(v_type));
            format r("type mismatch at term");
            r += pp_indent_expr(fmt, s.instantiate(v));
            r += compose(line(), format("has type"));
            r += given_fmt;
            r += compose(line(), format("but is expected to have type"));
            r += expected_fmt;
            return r;
        });
}

pair<expr, justification> update_meta(expr const & meta, substitution s) {
    buffer<expr> args;
    expr mvar = get_app_args(meta, args);
    justification j;
    auto p = s.instantiate_metavars(mlocal_type(mvar));
    mvar   = update_mlocal(mvar, p.first);
    j      = p.second;
    for (expr & arg : args) {
        auto p = s.instantiate_metavars(mlocal_type(arg));
        arg    = update_mlocal(arg, p.first);
        j      = mk_composite1(j, p.second);
    }
    return mk_pair(mk_app(mvar, args), j);
}
}
