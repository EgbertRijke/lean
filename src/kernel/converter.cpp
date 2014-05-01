/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "util/interrupt.h"
#include "util/lbool.h"
#include "kernel/converter.h"
#include "kernel/max_sharing.h"
#include "kernel/expr_maps.h"
#include "kernel/instantiate.h"
#include "kernel/free_vars.h"

namespace lean {
bool converter::is_conv(expr const & t, expr const & s, context & c) {
    delayed_justification j([]() { return justification(); });
    return is_conv(t, s, c, j);
}

bool converter::is_def_eq(expr const & t, expr const & s, context & c) {
    delayed_justification j([]() { return justification(); });
    return is_def_eq(t, s, c, j);
}

/** \brief Do nothing converter */
struct dummy_converter : public converter {
    virtual expr whnf(expr const & e, context &) { return e; }
    virtual bool is_conv(expr const &, expr const &, context &, delayed_justification &) { return true; }
    virtual bool is_def_eq(expr const &, expr const &, context &, delayed_justification &) { return true; }
};

std::unique_ptr<converter> mk_dummy_converter() {
    return std::unique_ptr<converter>(new dummy_converter());
}

struct default_converter : public converter {
    environment           m_env;
    optional<module_idx>  m_module_idx;
    bool                  m_memoize;
    name_set              m_extra_opaque;
    max_sharing_fn        m_sharing;
    expr_map<expr>        m_whnf_core_cache;
    expr_map<expr>        m_whnf_cache;

    default_converter(environment const & env, optional<module_idx> mod_idx, bool memoize, name_set const & extra_opaque):
        m_env(env), m_module_idx(mod_idx), m_memoize(memoize), m_extra_opaque(extra_opaque) {}

    class extended_context : public extension_context {
        default_converter & m_conv;
        context &           m_ctx;
    public:
        extended_context(default_converter & conv, context &  ctx):m_conv(conv), m_ctx(ctx) {}
        virtual environment const & env() const { return m_conv.m_env; }
        virtual expr whnf(expr const & e) { return m_conv.whnf(e, m_ctx); }
        virtual expr infer_type(expr const & e) { return m_ctx.infer_type(e); }
        virtual name mk_fresh_name() { return m_ctx.mk_fresh_name(); }
        virtual void add_cnstr(constraint const & c) { m_ctx.add_cnstr(c); }
    };

    bool check_memoized(expr const & e) const { return !m_memoize || m_sharing.already_processed(e); }
    expr max_sharing(expr const & e) { return m_memoize ? m_sharing(e) : e; }
    expr instantiate(expr const & e, unsigned n, expr const * s) { return max_sharing(lean::instantiate(e, n, s)); }
    expr instantiate(expr const & e, expr const & s) { return max_sharing(lean::instantiate(e, s)); }
    expr mk_app(expr const & f, unsigned num, expr const * args) { return max_sharing(lean::mk_app(f, num, args)); }
    expr mk_app(expr const & f, expr const & a) { return max_sharing(lean::mk_app(f, a)); }
    expr mk_rev_app(expr const & f, unsigned num, expr const * args) { return max_sharing(lean::mk_rev_app(f, num, args)); }
    expr mk_app_vars(expr const & f, unsigned num) { return max_sharing(lean::mk_app_vars(f, num)); }
    optional<expr> expand_macro(expr const & m, context & c) {
        lean_assert(is_macro(m));
        extended_context xctx(*this, c);
        if (auto new_m = macro_def(m).expand(macro_num_args(m), macro_args(m), xctx))
            return some_expr(max_sharing(*new_m));
        else
            return none_expr();
    }
    expr instantiate_params(expr const & e, param_names const & ps, levels const & ls) {
        return max_sharing(lean::instantiate_params(e, ps, ls));
    }
    /** \brief Apply normalizer extensions to \c e. */
    optional<expr> norm_ext(expr const & e, context & c) {
        extended_context xctx(*this, c);
        if (auto new_e = m_env.norm_ext()(e, xctx))
            return some_expr(max_sharing(*new_e));
        else
            return none_expr();
    }

    /** \brief Weak head normal form core procedure. It does not perform delta reduction nor normalization extensions. */
    expr whnf_core(expr const & e, context & c) {
        check_system("whnf");
        lean_assert(check_memoized(e));

        // handle easy cases
        switch (e.kind()) {
        case expr_kind::Var:    case expr_kind::Sort: case expr_kind::Meta: case expr_kind::Local:
        case expr_kind::Lambda: case expr_kind::Pi:   case expr_kind::Constant:
            return e;
        case expr_kind::Macro:  case expr_kind::Let:  case expr_kind::App:
            break;
        }

        // check cache
        if (m_memoize) {
            auto it = m_whnf_core_cache.find(e);
            if (it != m_whnf_core_cache.end())
                return it->second;
        }

        // do the actual work
        expr r;
        switch (e.kind()) {
        case expr_kind::Var:    case expr_kind::Sort: case expr_kind::Meta: case expr_kind::Local:
        case expr_kind::Lambda: case expr_kind::Pi:   case expr_kind::Constant:
            lean_unreachable(); // LCOV_EXCL_LINE
        case expr_kind::Macro:
            if (auto m = expand_macro(e, c))
                r = whnf_core(*m, c);
            else
                r = e;
            break;
        case expr_kind::Let:
            r = whnf_core(instantiate(let_body(e), let_value(e)), c);
            break;
        case expr_kind::App: {
            buffer<expr> args;
            expr const * it = &e;
            while (is_app(*it)) {
                args.push_back(app_arg(*it));
                it = &(app_fn(*it));
            }
            expr f = whnf_core(*it, c);
            if (is_lambda(f)) {
                unsigned m = 1;
                unsigned num_args = args.size();
                while (is_lambda(binder_body(f)) && m < num_args) {
                    f = binder_body(f);
                    m++;
                }
                lean_assert(m <= num_args);
                r = whnf_core(mk_rev_app(instantiate(binder_body(f), m, args.data() + (num_args - m)), num_args - m, args.data()), c);
            } else {
                r = is_eqp(f, *it) ? e : mk_rev_app(f, args.size(), args.data());
            }
            break;
        }}

        if (m_memoize)
            m_whnf_core_cache.insert(mk_pair(e, r));
        return r;
    }

    /**
       \brief Predicate for deciding whether \c d is an opaque definition or not.

       Here is the basic idea:

       1) Each definition has an opaque flag. This flag cannot be modified after a definition is added to the environment.
       The opaque flag affects the convertability check. The idea is to minimize the number of delta-reduction steps.
       We also believe it increases the modularity of Lean developments by minimizing the dependency on how things are defined.
       We should view non-opaque definitions as "inline definitions" used in programming languages such as C++.

       2) Whenever type checking an expression, the user can provide an additional set of definition names (m_extra_opaque) that
       should be considered opaque. Note that, if \c t type checks when using an extra_opaque set S, then t also type checks
       (modulo resource constraints) with the empty set. Again, the purpose of extra_opaque is to mimimize the number
       of delta-reduction steps.

       3) To be able to prove theorems about an opaque definition, we treat an opaque definition D in a module M as
       transparent when we are type checking another definition/theorem D' also in M. This rule only applies if
       D is not a theorem, nor D is in the set m_extra_opaque. To implement this feature, this class has a field
       m_module_idx that is not none when this rule should be applied.
    */
    bool is_opaque(definition const & d) const {
        lean_assert(d.is_definition());
        if (d.is_theorem()) return true;                                       // theorems are always opaque
        if (m_extra_opaque.contains(d.get_name())) return true;                // extra_opaque set overrides opaque flag
        if (!d.is_opaque()) return false;                                      // d is a transparent definition
        if (m_module_idx && d.get_module_idx() == *m_module_idx) return false; // the opaque definitions in module_idx are considered transparent
        return true;                                                           // d is opaque
    }

    /** \brief Expand \c e if it is non-opaque constant with weight >= w */
    expr unfold_name_core(expr e, unsigned w) {
        if (is_constant(e)) {
            if (auto d = m_env.find(const_name(e))) {
                if (d->is_definition() && !is_opaque(*d) && d->get_weight() >= w)
                    return unfold_name_core(instantiate_params(d->get_value(), d->get_params(), const_level_params(e)), w);
            }
        }
        return e;
    }

    /**
       \brief Expand constants and application where the function is a constant.

       The unfolding is only performend if the constant corresponds to
       a non-opaque definition with weight >= w.
    */
    expr unfold_names(expr const & e, unsigned w) {
        if (is_app(e)) {
            expr const * it = &e;
            while (is_app(*it)) {
                it = &(app_fn(*it));
            }
            expr f = unfold_name_core(*it, w);
            if (is_eqp(f, *it)) {
                return e;
            } else {
                buffer<expr> args;
                expr const * it = &e;
                while (is_app(*it)) {
                    args.push_back(app_arg(*it));
                    it = &(app_fn(*it));
                }
                return mk_rev_app(f, args.size(), args.data());
            }
        } else {
            return unfold_name_core(e, w);
        }
    }

    /** \brief Auxiliary method for \c is_delta */
    optional<definition> is_delta_core(expr const & e) {
        if (is_constant(e)) {
            if (auto d = m_env.find(const_name(e)))
                if (d->is_definition() && !is_opaque(*d))
                    return d;
        }
        return none_definition();
    }

    /**
        \brief Return some definition \c d iff \c e is a target for delta-reduction, and the given definition is the one
        to be expanded.
    */
    optional<definition> is_delta(expr const & e) {
        if (is_app(e)) {
            expr const * it = &e;
            while (is_app(*it)) {
                it = &(app_fn(*it));
            }
            return is_delta_core(*it);
        } else {
            return is_delta_core(e);
        }
    }

    /**
        \brief Weak head normal form core procedure that perform delta reduction for non-opaque constants with
        weight greater than or equal to \c w.

        This method is based on <tt>whnf_core(expr const &)</tt> and \c unfold_names.

        \remark This method does not use normalization extensions attached in the environment.
    */
    expr whnf_core(expr e, unsigned w, context & c) {
        while (true) {
            expr new_e = unfold_names(whnf_core(e, c), w);
            if (is_eqp(e, new_e))
                return e;
            e = new_e;
        }
    }

    /** \brief Put expression \c t in weak head normal form */
    virtual expr whnf(expr const & e_prime, context & c) {
        expr e = e_prime;
        // check cache
        if (m_memoize) {
            e = max_sharing(e);
            auto it = m_whnf_cache.find(e);
            if (it != m_whnf_cache.end())
                return it->second;
        }

        expr t = e;
        while (true) {
            expr t1 = whnf_core(t, 0, c);
            auto new_t = norm_ext(t1, c);
            if (new_t) {
                t = *new_t;
            } else {
                if (m_memoize)
                    m_whnf_cache.insert(mk_pair(e, t1));
                return t1;
            }
        }
    }

    /** \brief Eta-expand \c s and check if it is definitionally equal to \c t. */
    bool try_eta(expr const & t, expr const & s, context & c, delayed_justification & jst) {
        lean_assert(is_lambda(t));
        lean_assert(!is_lambda(s));
        expr t_s = whnf(c.infer_type(s), c);
        if (is_pi(t_s)) {
            // new_s := lambda x : domain(t_s), s x
            expr new_s = mk_lambda(c.mk_fresh_name(), binder_domain(t_s), mk_app(lift_free_vars(s, 1), mk_var(0)));
            return is_def_eq_core(t, new_s, c, jst);
        } else {
            return false;
        }
    }

    /**
        \brief Given lambda/Pi expressions \c t and \c s, return true iff \c t is convertible to \c s.

        The argument \c def_eq is used to decide whether the body of the binder is checked for
        definitional equality or convertability.

        If t and s are lambda expressions, then then t is convertible to s
           iff
        domain(t) is definitionally equal to domain(s)
        and
        body(t) is definitionally equal to body(s)

        For Pi expressions, it is slighly different.
        If t and s are Pi expressions, then then t is convertible to s
           iff
        domain(t) is definitionally equal to domain(s)
        and
        body(t) is convertible to body(s)
    */
    bool is_conv_binder(expr t, expr s, bool def_eq, context & c, delayed_justification & jst) {
        lean_assert(t.kind() == s.kind());
        lean_assert(is_binder(t));
        expr_kind k = t.kind();
        buffer<expr> subst;
        do {
            expr var_t_type = instantiate(binder_domain(t), subst.size(), subst.data());
            expr var_s_type = instantiate(binder_domain(s), subst.size(), subst.data());
            if (!is_def_eq_core(var_t_type, var_s_type, c, jst))
                return false;
            subst.push_back(mk_local(c.mk_fresh_name() + binder_name(s), var_s_type));
            t = binder_body(t);
            s = binder_body(s);
        } while (t.kind() == k && s.kind() == k);
        return is_conv(instantiate(t, subst.size(), subst.data()), instantiate(s, subst.size(), subst.data()), def_eq, c, jst);
    }

    /**
       \brief This is an auxiliary method for is_conv. It handles the "easy cases".

       If \c def_eq is true, then it checks for definitional equality.
    */
    lbool quick_is_conv(expr const & t, expr const & s, bool def_eq, context & c, delayed_justification & jst) {
        if (t == s)
            return l_true; // t and s are structurally equal
        if (is_meta(t) || is_meta(s)) {
            // if t or s is a metavariable (or the application of a metavariable), then add constraint
            if (def_eq)
                c.add_cnstr(mk_eq_cnstr(t, s, jst.get()));
            else
                c.add_cnstr(mk_conv_cnstr(t, s, jst.get()));
            return l_true;
        }
        if (t.kind() == s.kind()) {
            switch (t.kind()) {
            case expr_kind::Lambda:
                return to_lbool(is_conv_binder(t, s, true, c, jst));
            case expr_kind::Pi:
                return to_lbool(is_conv_binder(t, s, def_eq, c, jst));
            case expr_kind::Sort:
                // t and s are Sorts
                if (is_trivial(sort_level(t), sort_level(s)) && (!def_eq || is_trivial(sort_level(s), sort_level(t))))
                    return l_true;
                c.add_cnstr(mk_level_cnstr(sort_level(t), sort_level(s), jst.get()));
                if (def_eq)
                    c.add_cnstr(mk_level_cnstr(sort_level(s), sort_level(t), jst.get()));
                return l_true;
            case expr_kind::Meta:
                lean_unreachable(); // LCOV_EXCL_LINE
            case expr_kind::Var: case expr_kind::Local: case expr_kind::App:
            case expr_kind::Constant: case expr_kind::Macro:  case expr_kind::Let:
                // We do not handle these cases in this method.
                break;
            }
        }
        return l_undef; // This is not an "easy case"
    }

    /**
       \brief Return true if arguments of \c t are definitionally equal to arguments of \c s.
       Constraint generation is disabled when performing this test.
       This method is used to implement an optimization in the method \c is_conv.
    */
    bool is_def_eq_args(expr t, expr s, context & c, delayed_justification & jst) {
        context::disable_cnstrs_scope scope(c);
        try {
            while (is_app(t) && is_app(s)) {
                if (!is_def_eq_core(app_arg(t), app_arg(s), c, jst))
                    return false;
                t = app_fn(t);
                s = app_fn(s);
            }
            return !is_app(t) && !is_app(s);
        } catch (add_cnstr_exception &) {
            return false;
        }
    }

    /**
        \brief If def_eq is false, then return true iff t is convertible to s.
               If def_eq is true,  then return true iff t is definitionally equal to s.
    */
    bool is_conv(expr const & t, expr const & s, bool def_eq, context & c, delayed_justification & jst) {
        check_system("is_convertible");
        lbool r = quick_is_conv(t, s, def_eq, c, jst);
        if (r != l_undef) return r == l_true;

        // apply whnf (without using delta-reduction or normalizer extensions)
        expr t_n = whnf_core(t, c);
        expr s_n = whnf_core(s, c);
        if (!is_eqp(t_n, t) || !is_eqp(s_n, s)) {
            r = quick_is_conv(t_n, s_n, def_eq, c, jst);
            if (r != l_undef) return r == l_true;
        }

        // lazy delta-reduction and then normalizer extensions
        while (true) {
            // first, keep applying lazy delta-reduction while applicable
            while (true) {
                auto d_t = is_delta(t_n);
                auto d_s = is_delta(s_n);
                if (!d_t && !d_s) {
                    break;
                } else if (d_t && !d_s) {
                    t_n = whnf_core(unfold_names(t_n, 0), c);
                } else if (!d_t && d_s) {
                    s_n = whnf_core(unfold_names(s_n, 0), c);
                } else if (d_t->get_weight() > d_s->get_weight()) {
                    t_n = whnf_core(unfold_names(t_n, d_s->get_weight() + 1), c);
                } else if (d_t->get_weight() < d_s->get_weight()) {
                    s_n = whnf_core(unfold_names(s_n, d_t->get_weight() + 1), c);
                } else {
                    lean_assert(d_t && d_s && d_t->get_weight() == d_s->get_weight());
                    // If t_n and s_n are both applications of the same (non-opaque) definition,
                    // then we try to check if their arguments are definitionally equal.
                    // If they are, then t_n and s_n must be definitionally equal, and we can
                    // skip the delta-reduction step.
                    if (is_app(t_n) && is_app(s_n) &&
                        is_eqp(*d_t, *d_s) &&          // same definition
                        !is_opaque(*d_t) &&            // if d_t is opaque, we don't need to try this optimization
                        d_t->use_conv_opt() &&         // the flag use_conv_opt() can be used to disable this optimization
                        is_def_eq_args(t_n, s_n, c, jst)) {
                        return true;
                    }
                    t_n = whnf_core(unfold_names(t_n, d_t->get_weight() - 1), c);
                    s_n = whnf_core(unfold_names(s_n, d_s->get_weight() - 1), c);
                }
                r = quick_is_conv(t_n, s_n, def_eq, c, jst);
                if (r != l_undef) return r == l_true;
            }
            // try normalizer extensions
            auto new_t_n = norm_ext(t_n, c);
            auto new_s_n = norm_ext(s_n, c);
            if (!new_t_n && !new_s_n)
                break; // t_n and s_n are in weak head normal form
            if (new_t_n)
                t_n = whnf_core(*new_t_n, c);
            if (new_s_n)
                s_n = whnf_core(*new_s_n, c);
            r = quick_is_conv(t_n, s_n, def_eq, c, jst);
            if (r != l_undef) return r == l_true;
        }

        // At this point, t_n and s_n are in weak head normal form (modulo meta-variables)

        if (m_env.eta()) {
            lean_assert(!is_lambda(t_n) || !is_lambda(s_n));
            // Eta-reduction support
            if ((is_lambda(t_n) && try_eta(t_n, s_n, c, jst)) ||
                (is_lambda(s_n) && try_eta(s_n, t_n, c, jst)))
                return true;
        }

        if (is_app(t_n) && is_app(s_n)) {
            expr it1 = t_n;
            expr it2 = s_n;
            bool ok  = true;
            do {
                if (!is_def_eq_core(app_arg(it1), app_arg(it2), c, jst)) {
                    ok = false;
                    break;
                }
                it1 = app_fn(it1);
                it2 = app_fn(it2);
            } while (is_app(it1) && is_app(it2));
            if (ok && is_def_eq_core(it1, it2, c, jst))
                return true;
        }

        if (m_env.proof_irrel()) {
            // Proof irrelevance support
            expr t_type = c.infer_type(t);
            return is_prop(t_type, c) && is_def_eq_core(t_type, c.infer_type(s), c, jst);
        }

        return false;
    }

    bool is_prop(expr const & e, context & c) {
        return whnf(c.infer_type(e), c) == Bool;
    }

    bool is_def_eq_core(expr const & t, expr const & s, context & c, delayed_justification & j) {
        return is_conv(t, s, true, c, j);
    }

    virtual bool is_conv(expr const & t, expr const & s, context & c, delayed_justification & j) {
        return is_conv(max_sharing(t), max_sharing(s), false, c, j);
    }

    virtual bool is_def_eq(expr const & t, expr const & s, context & c, delayed_justification & j) {
        return is_def_eq_core(max_sharing(t), max_sharing(s), c, j);
    }
};

std::unique_ptr<converter> mk_default_converter(environment const & env, optional<module_idx> mod_idx,
                                                bool memoize, name_set const & extra_opaque) {
    return std::unique_ptr<converter>(new default_converter(env, mod_idx, memoize, extra_opaque));
}
}