/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "util/flet.h"
#include "kernel/replace_fn.h"
#include "kernel/free_vars.h"
#include "kernel/abstract.h"
#include "kernel/instantiate.h"
#include "library/annotation.h"
#include "library/aliases.h"
#include "library/scoped_ext.h"
#include "library/coercion.h"
#include "library/expr_pair.h"
#include "library/placeholder.h"
#include "library/private.h"
#include "library/explicit.h"
#include "library/typed_expr.h"
#include "library/num.h"
#include "library/let.h"
#include "library/print.h"
#include "frontends/lean/pp.h"
#include "frontends/lean/pp_options.h"
#include "frontends/lean/token_table.h"
#include "frontends/lean/builtin_exprs.h"

namespace lean {
static format * g_ellipsis_n_fmt  = nullptr;
static format * g_ellipsis_fmt    = nullptr;
static format * g_placeholder_fmt = nullptr;
static format * g_lambda_n_fmt    = nullptr;
static format * g_lambda_fmt      = nullptr;
static format * g_forall_n_fmt    = nullptr;
static format * g_forall_fmt      = nullptr;
static format * g_pi_n_fmt        = nullptr;
static format * g_pi_fmt          = nullptr;
static format * g_arrow_n_fmt     = nullptr;
static format * g_arrow_fmt       = nullptr;
static format * g_let_fmt         = nullptr;
static format * g_in_fmt          = nullptr;
static format * g_assign_fmt      = nullptr;
static format * g_have_fmt        = nullptr;
static format * g_from_fmt        = nullptr;
static format * g_visible_fmt     = nullptr;
static format * g_show_fmt        = nullptr;
static format * g_explicit_fmt    = nullptr;
static name   * g_tmp_prefix      = nullptr;

void initialize_pp() {
    g_ellipsis_n_fmt  = new format(highlight(format("\u2026")));
    g_ellipsis_fmt    = new format(highlight(format("...")));
    g_placeholder_fmt = new format(highlight(format("_")));
    g_lambda_n_fmt    = new format(highlight_keyword(format("\u03BB")));
    g_lambda_fmt      = new format(highlight_keyword(format("fun")));
    g_forall_n_fmt    = new format(highlight_keyword(format("\u2200")));
    g_forall_fmt      = new format(highlight_keyword(format("forall")));
    g_pi_n_fmt        = new format(highlight_keyword(format("Π")));
    g_pi_fmt          = new format(highlight_keyword(format("Pi")));
    g_arrow_n_fmt     = new format(highlight_keyword(format("\u2192")));
    g_arrow_fmt       = new format(highlight_keyword(format("->")));
    g_let_fmt         = new format(highlight_keyword(format("let")));
    g_in_fmt          = new format(highlight_keyword(format("in")));
    g_assign_fmt      = new format(highlight_keyword(format(":=")));
    g_have_fmt        = new format(highlight_keyword(format("have")));
    g_from_fmt        = new format(highlight_keyword(format("from")));
    g_visible_fmt     = new format(highlight_keyword(format("[visible]")));
    g_show_fmt        = new format(highlight_keyword(format("show")));
    g_explicit_fmt    = new format(highlight_keyword(format("@")));
    g_tmp_prefix      = new name(name::mk_internal_unique_name());
}

void finalize_pp() {
    delete g_ellipsis_n_fmt;
    delete g_ellipsis_fmt;
    delete g_placeholder_fmt;
    delete g_lambda_n_fmt;
    delete g_lambda_fmt;
    delete g_forall_n_fmt;
    delete g_forall_fmt;
    delete g_pi_n_fmt;
    delete g_pi_fmt;
    delete g_arrow_n_fmt;
    delete g_arrow_fmt;
    delete g_let_fmt;
    delete g_in_fmt;
    delete g_assign_fmt;
    delete g_have_fmt;
    delete g_from_fmt;
    delete g_visible_fmt;
    delete g_show_fmt;
    delete g_explicit_fmt;
    delete g_tmp_prefix;
}

name pretty_fn::mk_metavar_name(name const & m) {
    if (auto it = m_purify_meta_table.find(m))
        return *it;
    name new_m = m_meta_prefix.append_after(m_next_meta_idx);
    m_next_meta_idx++;
    m_purify_meta_table.insert(m, new_m);
    return new_m;
}

name pretty_fn::mk_local_name(name const & n, name const & suggested) {
    if (auto it = m_purify_local_table.find(n))
        return *it;
    unsigned i = 1;
    name r = suggested;
    while (m_purify_used_locals.contains(r)) {
        r = suggested.append_after(i);
        i++;
    }
    m_purify_used_locals.insert(r);
    m_purify_local_table.insert(n, r);
    return r;
}

level pretty_fn::purify(level const & l) {
    if (!m_universes || !has_meta(l))
        return l;
    return replace(l, [&](level const & l) {
            if (!has_meta(l))
                return some_level(l);
            if (is_meta(l))
                return some_level(mk_meta_univ(mk_metavar_name(meta_id(l))));
            return none_level();
        });
}

/** \brief Make sure that all metavariables have reasonable names,
    and for all local constants l1 l2, local_pp_name(l1) != local_pp_name(l2).

    \remark pretty_fn will create new local constants when pretty printing,
    but it will make sure that the new constants will not produce collisions.
*/
expr pretty_fn::purify(expr const & e) {
    if (!has_expr_metavar(e) && !has_local(e) && (!m_universes || !has_univ_metavar(e)))
        return e;
    return replace(e, [&](expr const & e, unsigned) {
            if (!has_expr_metavar(e) && !has_local(e) && (!m_universes || !has_univ_metavar(e)))
                return some_expr(e);
            else if (is_metavar(e))
                return some_expr(mk_metavar(mk_metavar_name(mlocal_name(e)), mlocal_type(e)));
            else if (is_local(e))
                return some_expr(mk_local(mlocal_name(e), mk_local_name(mlocal_name(e), local_pp_name(e)), mlocal_type(e), local_info(e)));
            else if (is_constant(e))
                return some_expr(update_constant(e, map(const_levels(e), [&](level const & l) { return purify(l); })));
            else if (is_sort(e))
                return some_expr(update_sort(e, purify(sort_level(e))));
            else
                return none_expr();
        });
}

void pretty_fn::set_options_core(options const & o) {
    m_options       = o;
    m_indent        = get_pp_indent(o);
    m_max_depth     = get_pp_max_depth(o);
    m_max_steps     = get_pp_max_steps(o);
    m_implict       = get_pp_implicit(o);
    m_unicode       = get_pp_unicode(o);
    m_coercion      = get_pp_coercions(o);
    m_notation      = get_pp_notation(o);
    m_universes     = get_pp_universes(o);
    m_full_names    = get_pp_full_names(o);
    m_private_names = get_pp_private_names(o);
    m_metavar_args  = get_pp_metavar_args(o);
    m_beta          = get_pp_beta(o);
}

void pretty_fn::set_options(options const & o) {
    if (is_eqp(o, m_options))
        return;
    set_options_core(o);
}

format pretty_fn::pp_level(level const & l) {
    return ::lean::pp(l, m_unicode, m_indent);
}

bool pretty_fn::is_implicit(expr const & f) {
    if (m_implict)
        return false; // showing implicit arguments
    if (!closed(f)) {
        // the Lean type checker assumes expressions are closed.
        return false;
    }
    try {
        binder_info bi = binding_info(m_tc.ensure_pi(m_tc.infer(f).first).first);
        return bi.is_implicit() || bi.is_strict_implicit() || bi.is_inst_implicit();
    } catch (exception &) {
        return false;
    }
}

bool pretty_fn::is_prop(expr const & e) {
    try {
        return m_env.impredicative() && m_tc.is_prop(e).first;
    } catch (exception &) {
        return false;
    }
}

auto pretty_fn::pp_coercion_fn(expr const & e, unsigned sz) -> result {
    if (sz == 1) {
        return pp_child(app_arg(e), max_bp()-1);
    } else if (is_app(e) && is_implicit(app_fn(e))) {
        return pp_coercion_fn(app_fn(e), sz-1);
    } else {
        expr const & fn = app_fn(e);
        result res_fn   = pp_coercion_fn(fn, sz-1);
        format fn_fmt   = res_fn.first;
        if (m_implict && sz == 2 && has_implicit_args(fn))
            fn_fmt = compose(*g_explicit_fmt, fn_fmt);
        result res_arg  = pp_child(app_arg(e), max_bp());
        return mk_result(group(compose(fn_fmt, nest(m_indent, compose(line(), res_arg.first)))), max_bp()-1);
    }
}

auto pretty_fn::pp_coercion(expr const & e, unsigned bp) -> result {
    buffer<expr> args;
    expr const & f = get_app_args(e, args);
    optional<pair<name, unsigned>> r = is_coercion(m_env, f);
    lean_assert(r);
    if (r->second >= args.size()) {
        return pp_child_core(e, bp);
    } else if (r->second == args.size() - 1) {
        return pp_child(args.back(), bp);
    } else {
        unsigned sz = args.size() - r->second;
        lean_assert(sz >= 2);
        auto r = pp_coercion_fn(e, sz);
        if (r.second < bp) {
            return mk_result(paren(r.first));
        } else {
            return r;
        }
    }
}

auto pretty_fn::pp_child_core(expr const & e, unsigned bp) -> result {
    result r = pp(e);
    if (r.second < bp) {
        return mk_result(paren(r.first));
    } else {
        return r;
    }
}

auto pretty_fn::pp_child(expr const & e, unsigned bp) -> result {
    if (is_app(e) && is_implicit(app_fn(e))) {
        return pp_child(app_fn(e), bp);
    } else if (is_app(e) && !m_coercion && is_coercion(m_env, get_app_fn(e))) {
        return pp_coercion(e, bp);
    } else {
        return pp_child_core(e, bp);
    }
}

auto pretty_fn::pp_var(expr const & e) -> result {
    unsigned vidx = var_idx(e);
    return mk_result(compose(format("#"), format(vidx)));
}

auto pretty_fn::pp_sort(expr const & e) -> result {
    if (m_env.impredicative() && is_equal(e, mk_Prop())) {
        return mk_result(format("Prop"));
    } else if (m_universes) {
        return mk_result(group(format("Type.{") + nest(6, pp_level(sort_level(e))) + format("}")));
    } else {
        return mk_result(format("Type"));
    }
}

optional<name> pretty_fn::is_aliased(name const & n) const {
    if (auto it = is_expr_aliased(m_env, n)) {
        // must check if we are not shadow by current namespace
        for (name const & ns : get_namespaces(m_env)) {
            if (!ns.is_anonymous() && m_env.find(ns + *it))
                return optional<name>();
        }
        return it;
    } else {
        return optional<name>();
    }
}

auto pretty_fn::pp_const(expr const & e) -> result {
    name n = const_name(e);
    if (!m_full_names) {
        if (auto it = is_aliased(n)) {
            n = *it;
        } else {
            for (name const & ns : get_namespaces(m_env)) {
                if (!ns.is_anonymous()) {
                    name new_n = n.replace_prefix(ns, name());
                    if (new_n != n && !new_n.is_anonymous()) {
                        n = new_n;
                        break;
                    }
                }
            }
        }
    }
    if (!m_private_names) {
        if (auto n1 = hidden_to_user_name(m_env, n))
            n = *n1;
    }
    if (m_universes && !empty(const_levels(e))) {
        format r = compose(format(n), format(".{"));
        bool first = true;
        for (auto const & l : const_levels(e)) {
            format l_fmt = pp_level(l);
            if (is_max(l) || is_imax(l))
                l_fmt = paren(l_fmt);
            if (first)
                r += nest(m_indent, l_fmt);
            else
                r += nest(m_indent, compose(line(), l_fmt));
            first = false;
        }
        r += format("}");
        return mk_result(group(r));
    } else {
        return mk_result(format(n));
    }
}

auto pretty_fn::pp_meta(expr const & e) -> result {
    return mk_result(compose(format("?"), format(mlocal_name(e))));
}

auto pretty_fn::pp_local(expr const & e) -> result {
    return mk_result(format(local_pp_name(e)));
}

bool pretty_fn::has_implicit_args(expr const & f) {
    if (!closed(f)) {
        // the Lean type checker assumes expressions are closed.
        return false;
    }
    name_generator ngen(*g_tmp_prefix);
    try {
        expr type = m_tc.whnf(m_tc.infer(f).first).first;
        while (is_pi(type)) {
            binder_info bi = binding_info(type);
            if (bi.is_implicit() || bi.is_strict_implicit() || bi.is_inst_implicit())
                return true;
            expr local = mk_local(ngen.next(), binding_name(type), binding_domain(type), binding_info(type));
            type = m_tc.whnf(instantiate(binding_body(type), local)).first;
        }
        return false;
    } catch (exception &) {
        return false;
    }
}

auto pretty_fn::pp_app(expr const & e) -> result {
    expr const & fn = app_fn(e);
    result res_fn = pp_child(fn, max_bp()-1);
    format fn_fmt = res_fn.first;
    if (m_implict && !is_app(fn) && has_implicit_args(fn))
        fn_fmt = compose(*g_explicit_fmt, fn_fmt);
    result res_arg = pp_child(app_arg(e), max_bp());
    return mk_result(group(compose(fn_fmt, nest(m_indent, compose(line(), res_arg.first)))), max_bp()-1);
}

format pretty_fn::pp_binder_block(buffer<name> const & names, expr const & type, binder_info const & bi) {
    format r;
    if (bi.is_implicit()) r += format("{");
    else if (bi.is_inst_implicit()) r += format("[");
    else if (bi.is_strict_implicit() && m_unicode) r += format("⦃");
    else if (bi.is_strict_implicit() && !m_unicode) r += format("{{");
    else r += format("(");
    for (name const & n : names) {
        r += format(n);
        r += space();
    }
    r += compose(colon(), nest(m_indent, compose(line(), pp_child(type, 0).first)));
    if (bi.is_implicit()) r += format("}");
    else if (bi.is_inst_implicit()) r += format("]");
    else if (bi.is_strict_implicit() && m_unicode) r += format("⦄");
    else if (bi.is_strict_implicit() && !m_unicode) r += format("}}");
    else r += format(")");
    return group(r);
}

format pretty_fn::pp_binders(buffer<expr> const & locals) {
    unsigned num     = locals.size();
    buffer<name> names;
    expr local       = locals[0];
    expr   type      = mlocal_type(local);
    binder_info bi   = local_info(local);
    names.push_back(local_pp_name(local));
    format r;
    for (unsigned i = 1; i < num; i++) {
        expr local = locals[i];
        if (is_equal(mlocal_type(local), type) && local_info(local) == bi) {
            names.push_back(local_pp_name(local));
        } else {
            r += group(compose(line(), pp_binder_block(names, type, bi)));
            names.clear();
            type = mlocal_type(local);
            bi   = local_info(local);
            names.push_back(local_pp_name(local));
        }
    }
    r += group(compose(line(), pp_binder_block(names, type, bi)));
    return r;
}

auto pretty_fn::pp_lambda(expr const & e) -> result {
    expr b = e;
    buffer<expr> locals;
    while (is_lambda(b)) {
        auto p = binding_body_fresh(b, true);
        locals.push_back(p.second);
        b = p.first;
    }
    format r = m_unicode ? *g_lambda_n_fmt : *g_lambda_fmt;
    r += pp_binders(locals);
    r += compose(comma(), nest(m_indent, compose(line(), pp_child(b, 0).first)));
    return mk_result(r, 0);
}

/** \brief Similar to #is_arrow, but only returns true if binder_info is the default one.
    That is, we don't want to lose binder info when pretty printing.
*/
static bool is_default_arrow(expr const & e) {
    return is_arrow(e) && binding_info(e) == binder_info();
}

auto pretty_fn::pp_pi(expr const & e) -> result {
    if (is_default_arrow(e)) {
        result lhs = pp_child(binding_domain(e), get_arrow_prec());
        result rhs = pp_child(lift_free_vars(binding_body(e), 1), get_arrow_prec()-1);
        format r   = group(lhs.first + space() + (m_unicode ? *g_arrow_n_fmt : *g_arrow_fmt) + line() + rhs.first);
        return mk_result(r, get_arrow_prec()-1);
    } else {
        expr b = e;
        buffer<expr> locals;
        while (is_pi(b) && !is_default_arrow(b)) {
            auto p = binding_body_fresh(b, true);
            locals.push_back(p.second);
            b = p.first;
        }
        format r;
        if (is_prop(b))
            r = m_unicode ? *g_forall_n_fmt : *g_forall_fmt;
        else
            r = m_unicode ? *g_pi_n_fmt : *g_pi_fmt;
        r += pp_binders(locals);
        r += compose(comma(), nest(m_indent, compose(line(), pp_child(b, 0).first)));
        return mk_result(r, 0);
    }
}

static bool is_have(expr const & e) { return is_app(e) && is_have_annotation(app_fn(e)); }
static bool is_show(expr const & e) {
    return is_show_annotation(e) && is_app(get_annotation_arg(e)) &&
        is_lambda(app_fn(get_annotation_arg(e)));
}

auto pretty_fn::pp_have(expr const & e) -> result {
    expr proof   = app_arg(e);
    expr binding = get_annotation_arg(app_fn(e));
    auto p       = binding_body_fresh(binding, true);
    expr local   = p.second;
    expr body    = p.first;
    name const & n = local_pp_name(local);
    format type_fmt  = pp_child(mlocal_type(local), 0).first;
    format proof_fmt = pp_child(proof, 0).first;
    format body_fmt  = pp_child(body, 0).first;
    format r = *g_have_fmt + space() + format(n) + space();
    if (binding_info(binding).is_contextual())
        r += compose(*g_visible_fmt, space());
    r += colon() + nest(m_indent, line() + type_fmt + comma() + space() + *g_from_fmt);
    r = group(r);
    r += nest(m_indent, line() + proof_fmt + comma());
    r = group(r);
    r += line() + body_fmt;
    return mk_result(r, 0);
}

auto pretty_fn::pp_show(expr const & e) -> result {
    lean_assert(is_show(e));
    expr s           = get_annotation_arg(e);
    expr proof       = app_arg(s);
    expr type        = binding_domain(app_fn(s));
    format type_fmt  = pp_child(type, 0).first;
    format proof_fmt = pp_child(proof, 0).first;
    format r = *g_show_fmt + space() + nest(5, type_fmt) + comma() + space() + *g_from_fmt;
    r = group(r);
    r += nest(m_indent, compose(line(), proof_fmt));
    return mk_result(group(r), 0);
}

auto pretty_fn::pp_explicit(expr const & e) -> result {
    result res_arg = pp_child(get_explicit_arg(e), max_bp());
    return mk_result(compose(*g_explicit_fmt, res_arg.first), max_bp());
}

auto pretty_fn::pp_macro(expr const & e) -> result {
    if (is_explicit(e)) {
        return pp_explicit(e);
    } else {
        // TODO(Leo): have macro annotations
        // fix macro<->pp interface
        format r = compose(format("["), format(macro_def(e).get_name()));
        for (unsigned i = 0; i < macro_num_args(e); i++)
            r += nest(m_indent, compose(line(), pp_child(macro_arg(e, i), max_bp()).first));
        r += format("]");
        return mk_result(group(r));
    }
}

auto pretty_fn::pp_let(expr e) -> result {
    buffer<pair<name, expr>> decls;
    while (true) {
        if (!is_let(e))
            break;
        name n   = get_let_var_name(e);
        expr v   = get_let_value(e);
        expr b   = get_let_body(e);
        lean_assert(closed(b));
        expr b1  = abstract(b, v);
        if (closed(b1)) {
            e = b1;
        } else {
            n = pick_unused_name(b1, n);
            decls.emplace_back(n, v);
            e = instantiate(b1, mk_constant(n));
        }
    }
    if (decls.empty())
        return pp(e);
    format r    = *g_let_fmt;
    unsigned sz = decls.size();
    for (unsigned i = 0; i < sz; i++) {
        name const & n = decls[i].first;
        expr const & v = decls[i].second;
        format beg     = i == 0 ? space() : line();
        format sep     = i < sz - 1 ? comma() : format();
        format entry   = format(n);
        format v_fmt   = pp_child(v, 0).first;
        entry += space() + *g_assign_fmt + nest(m_indent, line() + v_fmt + sep);
        r += nest(3 + 1, beg + group(entry));
    }
    format b = pp_child(e, 0).first;
    r += line() + *g_in_fmt + space() + nest(2 + 1, b);
    return mk_result(r, 0);
}

auto pretty_fn::pp_num(mpz const & n) -> result {
    return mk_result(format(n));
}

auto pretty_fn::pp(expr const & e) -> result {
    if (m_depth > m_max_depth || m_num_steps > m_max_steps)
        return mk_result(m_unicode ? *g_ellipsis_n_fmt : *g_ellipsis_fmt);
    flet<unsigned> let_d(m_depth, m_depth+1);
    m_num_steps++;

    if (is_placeholder(e))  return mk_result(*g_placeholder_fmt);
    if (is_show(e))         return pp_show(e);
    if (is_have(e))         return pp_have(e);
    if (is_let(e))          return pp_let(e);
    if (is_typed_expr(e))   return pp(get_typed_expr_expr(e));
    if (is_let_value(e))    return pp(get_let_value_expr(e));
    if (auto n = to_num(e)) return pp_num(*n);
    if (!m_metavar_args && is_meta(e))
        return pp_meta(get_app_fn(e));

    switch (e.kind()) {
    case expr_kind::Var:       return pp_var(e);
    case expr_kind::Sort:      return pp_sort(e);
    case expr_kind::Constant:  return pp_const(e);
    case expr_kind::Meta:      return pp_meta(e);
    case expr_kind::Local:     return pp_local(e);
    case expr_kind::App:       return pp_app(e);
    case expr_kind::Lambda:    return pp_lambda(e);
    case expr_kind::Pi:        return pp_pi(e);
    case expr_kind::Macro:     return pp_macro(e);
    }
    lean_unreachable(); // LCOV_EXCL_LINE
}

pretty_fn::pretty_fn(environment const & env, options const & o):
    m_env(env), m_tc(env) {
    set_options_core(o);
    m_meta_prefix   = "M";
    m_next_meta_idx = 1;
}

format pretty_fn::operator()(expr const & e) {
    m_depth = 0; m_num_steps = 0;
    if (m_beta)
        return pp_child(purify(beta_reduce(e)), 0).first;
    else
        return pp_child(purify(e), 0).first;
}

formatter_factory mk_pretty_formatter_factory() {
    return [](environment const & env, options const & o) { // NOLINT
        auto fn_ptr = std::make_shared<pretty_fn>(env, o);
        return formatter(o, [=](expr const & e, options const & new_o) {
                fn_ptr->set_options(new_o);
                return (*fn_ptr)(e);
            });
    };
}
}
