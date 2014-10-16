/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <algorithm>
#include "util/sstream.h"
#include "util/name_map.h"
#include "kernel/replace_fn.h"
#include "kernel/type_checker.h"
#include "kernel/instantiate.h"
#include "kernel/inductive/inductive.h"
#include "kernel/abstract.h"
#include "kernel/free_vars.h"
#include "library/scoped_ext.h"
#include "library/locals.h"
#include "library/deep_copy.h"
#include "library/placeholder.h"
#include "library/aliases.h"
#include "library/protected.h"
#include "library/explicit.h"
#include "library/reducible.h"
#include "frontends/lean/decl_cmds.h"
#include "frontends/lean/util.h"
#include "frontends/lean/class.h"
#include "frontends/lean/parser.h"
#include "frontends/lean/tokens.h"

namespace lean {
using inductive::intro_rule;
using inductive::inductive_decl;
using inductive::inductive_decl_name;
using inductive::inductive_decl_type;
using inductive::inductive_decl_intros;
using inductive::intro_rule_name;
using inductive::intro_rule_type;

inductive_decl update_inductive_decl(inductive_decl const & d, expr const & t) {
    return inductive_decl(inductive_decl_name(d), t, inductive_decl_intros(d));
}

inductive_decl update_inductive_decl(inductive_decl const & d, buffer<intro_rule> const & irs) {
    return inductive_decl(inductive_decl_name(d), inductive_decl_type(d), to_list(irs.begin(), irs.end()));
}

intro_rule update_intro_rule(intro_rule const & r, expr const & t) {
    return intro_rule(intro_rule_name(r), t);
}

static name * g_tmp_prefix = nullptr;
static name * g_inductive  = nullptr;
static name * g_intro      = nullptr;
static name * g_recursor   = nullptr;

void initialize_inductive_cmd() {
    g_tmp_prefix = new name(name::mk_internal_unique_name());
    g_inductive  = new name("inductive");
    g_intro      = new name("intro");
    g_recursor   = new name("recursor");
}

void finalize_inductive_cmd() {
    delete g_recursor;
    delete g_intro;
    delete g_inductive;
    delete g_tmp_prefix;
}

struct inductive_cmd_fn {
    typedef std::unique_ptr<type_checker> type_checker_ptr;
    struct modifiers {
        bool m_is_class;
        modifiers():m_is_class(false) {}
        void parse(parser & p) {
            while (true) {
                if (p.curr_is_token(get_class_tk())) {
                    m_is_class = true;
                    p.next();
                } else {
                    break;
                }
            }
        }
    };

    enum class implicit_infer_kind { Implicit, RelaxedImplicit, None };
    typedef name_map<implicit_infer_kind> implicit_infer_map;

    parser &            m_p;
    environment         m_env;
    type_checker_ptr    m_tc;
    name                m_namespace; // current namespace
    pos_info            m_pos; // current position for reporting errors
    bool                m_first; // true if parsing the first inductive type in a mutually recursive inductive decl.
    buffer<name>        m_explicit_levels;
    buffer<name>        m_levels;
    buffer<expr>        m_params; // parameters
    unsigned            m_num_params; // number of parameters
    bool                m_using_explicit_levels; // true if the user is providing explicit universe levels
    level               m_u; // temporary auxiliary global universe used for inferring the result universe of
                             // an inductive datatype declaration.
    bool                m_infer_result_universe;
    implicit_infer_map  m_implicit_infer_map; // implicit argument inference mode
    name_map<modifiers> m_modifiers;
    typedef std::tuple<name, name, pos_info> decl_info;
    buffer<decl_info> m_decl_info; // auxiliary buffer used to populate declaration_index


    inductive_cmd_fn(parser & p):m_p(p) {
        m_env   = p.env();
        m_first = true;
        m_using_explicit_levels = false;
        m_num_params = 0;
        name u_name(*g_tmp_prefix, "u");
        m_env = m_env.add_universe(u_name);
        m_u = mk_global_univ(u_name);
        m_infer_result_universe = false;
        m_namespace = get_namespace(m_env);
        m_tc = mk_type_checker(m_env, m_p.mk_ngen(), false);
    }

    [[ noreturn ]] void throw_error(char const * error_msg) { throw parser_error(error_msg, m_pos); }
    [[ noreturn ]] void throw_error(sstream const & strm) { throw parser_error(strm, m_pos); }

    implicit_infer_kind get_implicit_infer_kind(name const & n) {
        if (auto it = m_implicit_infer_map.find(n))
            return *it;
        else
            return implicit_infer_kind::Implicit;
    }

    name mk_rec_name(name const & n) {
        return n + name("rec");
    }

    /** \brief Parse the name of an inductive datatype or introduction rule,
        prefix the current namespace to it and return it.
    */
    pair<name, name> parse_decl_name(optional<name> const & ind_name) {
        m_pos   = m_p.pos();
        name id = m_p.check_id_next("invalid declaration, identifier expected");
        if (ind_name) {
            // for intro rules, we append the name of the inductive datatype
            check_atomic(id);
            name full_id = *ind_name + id;
            m_decl_info.emplace_back(full_id, *g_intro, m_pos);
            return mk_pair(id, full_id);
        } else {
            name full_id = m_namespace + id;
            m_decl_info.emplace_back(full_id, *g_inductive, m_pos);
            m_decl_info.emplace_back(mk_rec_name(full_id), *g_recursor, m_pos);
            return mk_pair(id, full_id);
        }
    }

    pair<name, name> parse_inductive_decl_name() { return parse_decl_name(optional<name>()); }
    name parse_intro_decl_name(name const & ind_name) { return parse_decl_name(optional<name>(ind_name)).second; }

    /** \brief Parse inductive declaration universe parameters.
        If this is the first declaration in a mutually recursive block, then this method
        stores the levels in m_explicit_levels, and set m_using_explicit_levels to true (iff they were provided).

        If this is not the first declaration, then this function just checks if the user
        is not providing explicit universe levels again.
    */
    void parse_inductive_univ_params() {
        buffer<name> curr_ls_buffer;
        if (parse_univ_params(m_p, curr_ls_buffer)) {
            if (!m_first) {
                throw_error("invalid mutually recursive declaration, "
                            "explicit universe levels should only be provided to first inductive type in this declaration");
            }
            m_using_explicit_levels = true;
            m_explicit_levels.append(curr_ls_buffer);
        }
    }

    /** \brief Parse the type of an inductive datatype */
    expr parse_datatype_type() {
        expr type;
        buffer<expr> ps;
        m_pos = m_p.pos();
        if (m_p.curr_is_token(get_assign_tk())) {
            type = mk_sort(mk_level_placeholder());
        } else if (m_first && !m_p.curr_is_token(get_colon_tk())) {
            lean_assert(m_params.empty());
            m_p.parse_binders(ps);
            m_num_params = ps.size();
            if (m_p.curr_is_token(get_colon_tk())) {
                m_p.next();
                type = m_p.parse_scoped_expr(ps);
            } else {
                type = mk_sort(mk_level_placeholder());
            }
            type = Pi(ps, type, m_p);
        } else {
            m_p.check_token_next(get_colon_tk(), "invalid mutually recursive inductive declaration, "
                                 "':' expected (remark: parameters should be provided only to first datatype)");
            type = m_p.parse_expr();
        }
        if (!m_first)
            type = Pi(m_params, type, m_p);
        return type;
    }

    /** \brief Return the universe level of the given type, if it is not a sort, then raise an exception. */
    level get_datatype_result_level(expr d_type) {
        d_type = m_tc->whnf(d_type).first;
        while (is_pi(d_type)) {
            d_type = m_tc->whnf(binding_body(d_type)).first;
        }
        if (!is_sort(d_type))
            throw_error(sstream() << "invalid inductive datatype, resultant type is not a sort");
        return sort_level(d_type);
    }

    /** \brief Create a local constant based on the given binding */
    expr mk_local_for(expr const & b) {
        return mk_local(m_p.mk_fresh_name(), binding_name(b), binding_domain(b), binding_info(b));
    }

    /** \brief Set explicit datatype parameters as local constants in m_params */
    void set_params(expr d_type) {
        lean_assert(m_params.empty());
        for (unsigned i = 0; i < m_num_params; i++) {
            expr l = mk_local(binding_name(d_type), binding_name(d_type), binding_domain(d_type), binding_info(d_type));
            m_params.push_back(l);
            d_type = instantiate(binding_body(d_type), l);
        }
    }

    /** \brief Add the parameters (in m_params) to parser local scope */
    void add_params_to_local_scope() {
        for (expr const & l : m_params)
            m_p.add_local(l);
    }

    /** \brief Parse introduction rules in the scope of m_params.

        Introduction rules with the annotation '{}' are marked for relaxed (aka non-strict) implicit parameter inference.
        Introduction rules with the annotation '()' are marked for no implicit parameter inference.
    */
    list<intro_rule> parse_intro_rules(name const & ind_name) {
        buffer<intro_rule> intros;
        while (true) {
            name intro_name = parse_intro_decl_name(ind_name);
            if (m_p.curr_is_token(get_lcurly_tk())) {
                m_p.next();
                m_p.check_token_next(get_rcurly_tk(), "invalid introduction rule, '}' expected");
                m_implicit_infer_map.insert(intro_name, implicit_infer_kind::RelaxedImplicit);
            }
            if (m_p.curr_is_token(get_lparen_tk())) {
                m_p.next();
                m_p.check_token_next(get_rparen_tk(), "invalid introduction rule, ')' expected");
                m_implicit_infer_map.insert(intro_name, implicit_infer_kind::None);
            }
            m_p.check_token_next(get_colon_tk(), "invalid introduction rule, ':' expected");
            expr intro_type = m_p.parse_expr();
            intros.push_back(intro_rule(intro_name, intro_type));
            if (!m_p.curr_is_token(get_comma_tk()))
                break;
            m_p.next();
        }
        return to_list(intros.begin(), intros.end());
    }

    void parse_inductive_decls(buffer<inductive_decl> & decls) {
        while (true) {
            parser::local_scope l_scope(m_p);
            pair<name, name> d_names = parse_inductive_decl_name();
            name d_short_name = d_names.first;
            name d_name       = d_names.second;
            parse_inductive_univ_params();
            if (!m_first) {
                add_params_to_local_scope();
                for (name const & lvl_name : m_explicit_levels)
                    m_p.add_local_level(lvl_name, mk_param_univ(lvl_name));
            }
            modifiers mods;
            mods.parse(m_p);
            m_modifiers.insert(d_name, mods);
            expr d_type = parse_datatype_type();
            bool empty_type = true;
            if (m_p.curr_is_token(get_assign_tk())) {
                empty_type = false;
                m_p.next();
            }
            if (m_first) {
                m_levels.append(m_explicit_levels);
                set_params(d_type);
            }
            if (empty_type) {
                decls.push_back(inductive_decl(d_name, d_type, list<intro_rule>()));
            } else {
                if (m_first)
                    add_params_to_local_scope();
                expr d_const = mk_constant(d_name, param_names_to_levels(to_list(m_explicit_levels.begin(),
                                                                                 m_explicit_levels.end())));
                m_p.add_local_expr(d_short_name, d_const);
                auto d_intro_rules = parse_intro_rules(d_name);
                decls.push_back(inductive_decl(d_name, d_type, d_intro_rules));
            }
            if (!m_p.curr_is_token(get_with_tk())) {
                break;
            }
            m_p.next();
            m_first = false;
        }
    }
    /** \brief Include in m_levels any local level referenced by decls. */
    void include_local_levels(buffer<inductive_decl> const & decls, buffer<expr> const & locals) {
        if (!m_p.has_locals())
            return;
        name_set all_lvl_params;
        for (auto const & local : locals) {
            all_lvl_params = collect_univ_params(mlocal_type(local), all_lvl_params);
        }
        for (auto const & d : decls) {
            all_lvl_params = collect_univ_params(inductive_decl_type(d), all_lvl_params);
            for (auto const & ir : inductive_decl_intros(d)) {
                all_lvl_params = collect_univ_params(intro_rule_type(ir), all_lvl_params);
            }
        }
        buffer<name> local_lvls;
        all_lvl_params.for_each([&](name const & l) {
                if (std::find(m_levels.begin(), m_levels.end(), l) == m_levels.end())
                    local_lvls.push_back(l);
            });
        std::sort(local_lvls.begin(), local_lvls.end(), [&](name const & n1, name const & n2) {
                return m_p.get_local_level_index(n1) < m_p.get_local_level_index(n2);
            });
        buffer<name> new_levels;
        new_levels.append(local_lvls);
        new_levels.append(m_levels);
        m_levels.clear();
        m_levels.append(new_levels);
    }

    /** \brief Collect local constants used in the inductive decls. */
    void collect_locals_core(buffer<inductive_decl> const & decls, expr_struct_set & ls) {
        buffer<expr> include_vars;
        m_p.get_include_variables(include_vars);
        for (expr const & param : include_vars) {
            ::lean::collect_locals(mlocal_type(param), ls);
            ls.insert(param);
        }
        for (auto const & d : decls) {
            ::lean::collect_locals(inductive_decl_type(d), ls);
            for (auto const & ir : inductive_decl_intros(d)) {
                expr ir_type = intro_rule_type(ir);
                ir_type = Pi(m_params, ir_type);
                ::lean::collect_locals(ir_type, ls);
            }
        }
    }

    /** \brief Collect local constants used in the declaration as extra parameters, and
        update inductive datatype types with them. */
    void collect_locals(buffer<inductive_decl> & decls, buffer<expr> & locals) {
        if (!m_p.has_locals())
            return;
        expr_struct_set local_set;
        collect_locals_core(decls, local_set);
        if (local_set.empty())
            return;
        sort_locals(local_set, m_p, locals);
        m_num_params += locals.size();
    }

    /** \brief Update the result sort of the given type */
    expr update_result_sort(expr t, level const & l) {
        t = m_tc->whnf(t).first;
        if (is_pi(t)) {
            return update_binding(t, binding_domain(t), update_result_sort(binding_body(t), l));
        } else if (is_sort(t)) {
            return update_sort(t, l);
        } else {
            lean_unreachable();
        }
    }

    /** \brief Convert inductive datatype declarations into local constants, and store them into \c r and \c map.
        \c map is a mapping from inductive datatype name into local constant. */
    void inductive_types_to_locals(buffer<inductive_decl> const & decls, buffer<expr> & r, name_map<expr> & map) {
        for (inductive_decl const & decl : decls) {
            name const & n = inductive_decl_name(decl);
            expr type      = inductive_decl_type(decl);
            for (unsigned i = 0; i < m_params.size(); i++) {
                lean_assert(is_pi(type));
                type = binding_body(type);
            }
            type = instantiate_rev(type, m_params.size(), m_params.data());
            level l = get_datatype_result_level(type);
            if (is_placeholder(l)) {
                if (m_using_explicit_levels)
                    throw_error("resultant universe must be provided, when using explicit universe levels");
                type = update_result_sort(type, m_u);
                m_infer_result_universe = true;
            }
            expr local = mk_local(m_p.mk_fresh_name(), n, type, binder_info());
            r.push_back(local);
            map.insert(n, local);
        }
    }

    // TODO(Leo): move to different file
    static bool is_explicit(binder_info const & bi) {
        return !bi.is_implicit() && !bi.is_strict_implicit() && !bi.is_inst_implicit();
    }

    /** \brief Replace every occurrences of the inductive datatypes (in decls) in \c type with a local constant */
    expr fix_intro_rule_type(expr const & type, name_map<expr> const & ind_to_local) {
        unsigned nparams = 0; // number of explicit parameters
        for (expr const & param : m_params) {
            if (is_explicit(local_info(param)))
                nparams++;
        }
        return replace(type, [&](expr const & e) {
                expr const & fn = get_app_fn(e);
                if (!is_constant(fn))
                    return none_expr();
                if (auto it = ind_to_local.find(const_name(fn))) {
                    buffer<expr> args;
                    get_app_args(e, args);
                    if (args.size() < nparams)
                        throw parser_error(sstream() << "invalide datatype declaration, "
                                           << "incorrect number of arguments to datatype '"
                                           << const_name(fn) << "'", m_p.pos_of(e));
                    pos_info pos = m_p.pos_of(e);
                    expr r       = m_p.save_pos(copy(*it), pos);
                    for (unsigned i = nparams; i < args.size(); i++)
                        r = m_p.mk_app(r, args[i], pos);
                    return some_expr(r);
                } else {
                    return none_expr();
                }
            });
    }

    void intro_rules_to_locals(buffer<inductive_decl> const & decls, name_map<expr> const & ind_to_local, buffer<expr> & r) {
        for (inductive_decl const & decl : decls) {
            for (intro_rule const & rule : inductive_decl_intros(decl)) {
                expr type  = fix_intro_rule_type(intro_rule_type(rule), ind_to_local);
                expr local = mk_local(m_p.mk_fresh_name(), intro_rule_name(rule), type, binder_info());
                r.push_back(local);
            }
        }
    }

    /** \brief Traverse the introduction rule type and collect the universes where arguments reside in \c r_lvls.
        This information is used to compute the resultant universe level for the inductive datatype declaration.
    */
    void accumulate_levels(expr intro_type, buffer<level> & r_lvls) {
        while (is_pi(intro_type)) {
            expr s  = m_tc->ensure_type(binding_domain(intro_type)).first;
            level l = sort_level(s);
            if (is_equal(l, m_u)) {
                // ignore, this is the auxiliary level
            } else if (occurs(m_u, l)) {
                throw exception("failed to infer inductive datatype resultant universe, "
                                "provide the universe levels explicitly");
            } else if (std::find_if(r_lvls.begin(), r_lvls.end(),
                                    [&](level const & l2) { return is_equal(l, l2); }) == r_lvls.end()) {
                r_lvls.push_back(l);
            }
            intro_type = instantiate(binding_body(intro_type), mk_local_for(intro_type));
        }
    }

    /** \brief Given a sequence of introduction rules (encoded as local constants), compute the resultant
        universe for the inductive datatype declaration.
    */
    level infer_resultant_universe(unsigned num_intro_rules, expr const * intro_rules) {
        lean_assert(m_infer_result_universe);
        buffer<level> r_lvls;
        for (unsigned i = 0; i < num_intro_rules; i++) {
            accumulate_levels(mlocal_type(intro_rules[i]), r_lvls);
        }
        return mk_result_level(m_env, r_lvls);
    }

    /** \brief Create a mapping from inductive datatype temporary name (used in local constants) to an
        application <tt>C.{ls} locals params</tt>, where \c C is the real name of the inductive datatype,
        and \c ls are the universe level parameters in \c m_levels.
    */
    name_map<expr> locals_to_inductive_types(buffer<expr> const & locals, unsigned nparams, expr const * params,
                                             unsigned num_decls, expr const * decls) {
        buffer<level> buffer_ls;
        for (name const & l : m_levels) {
            buffer_ls.push_back(mk_param_univ(l));
        }
        levels ls = to_list(buffer_ls.begin(), buffer_ls.end());
        name_map<expr> r;
        for (unsigned i = 0; i < num_decls; i++) {
            expr c = mk_constant(local_pp_name(decls[i]), ls);
            c = mk_app(c, locals);
            c = mk_app(c, nparams, params);
            r.insert(mlocal_name(decls[i]), c);
        }
        return r;
    }

    /** \brief Create the "final" introduction rule type. It will apply the mapping \c local_to_ind built using
        locals_to_inductive_types, and abstract locals and parameters.
    */
    expr mk_intro_rule_type(name const & ir_name,
                            buffer<expr> const & locals, unsigned nparams, expr const * params,
                            name_map<expr> const & local_to_ind, expr type) {
        type = replace(type, [&](expr const & e) {
                if (!is_local(e)) {
                    return none_expr();
                } else if (auto it = local_to_ind.find(mlocal_name(e))) {
                    return some_expr(*it);
                } else {
                    return none_expr();
                }
            });
        type = Pi(nparams, params, type);
        type = Pi(locals, type);
        implicit_infer_kind k = get_implicit_infer_kind(ir_name);
        switch (k) {
        case implicit_infer_kind::Implicit: {
            bool strict = true;
            return infer_implicit(type, locals.size() + nparams, strict);
        }
        case implicit_infer_kind::RelaxedImplicit: {
            bool strict = false;
            return infer_implicit(type, locals.size() + nparams, strict);
        }
        case implicit_infer_kind::None:
            return type;
        }
        lean_unreachable();
    }

    /** \brief Elaborate inductive datatypes and their introduction rules. */
    void elaborate_decls(buffer<inductive_decl> & decls, buffer<expr> const & locals) {
        // We create an elaboration problem of the form
        // Pi (params) (inductive_types) (intro_rules), Type
        buffer<expr> to_elab;
        to_elab.append(m_params);
        name_map<expr> ind_to_local;
        inductive_types_to_locals(decls, to_elab, ind_to_local);
        intro_rules_to_locals(decls, ind_to_local, to_elab);
        expr aux_type = Pi(to_elab, mk_Type(), m_p);
        list<expr> locals_ctx;
        for (expr const & local : locals)
            locals_ctx = cons(local, locals_ctx);
        level_param_names new_ls;
        std::tie(aux_type, new_ls) = m_p.elaborate_type(aux_type, locals_ctx);
        // save new levels
        for (auto l : new_ls)
            m_levels.push_back(l);
        // update to_elab
        for (expr & l : to_elab) {
            l = update_mlocal(l, binding_domain(aux_type));
            aux_type = instantiate(binding_body(aux_type), l);
        }
        unsigned nparams         = m_params.size();
        unsigned num_decls       = decls.size();
        unsigned first_intro_idx = nparams + num_decls;
        lean_assert(first_intro_idx <= to_elab.size());
        // compute resultant level
        level resultant_level;
        if (m_infer_result_universe) {
            unsigned num_intros = to_elab.size() - first_intro_idx;
            resultant_level     = infer_resultant_universe(num_intros, to_elab.data() + first_intro_idx);
        }
        // update decls
        unsigned i       = nparams;
        for (inductive_decl & decl : decls) {
            expr type = mlocal_type(to_elab[i]);
            if (m_infer_result_universe)
                type = update_result_sort(type, resultant_level);
            type = Pi(nparams, to_elab.data(), type);
            type = Pi(locals, type);
            decl = update_inductive_decl(decl, type);
            i++;
        }
        // Create mapping for converting occurrences of inductive types (as local constants)
        // into the real ones.
        name_map<expr> local_to_ind = locals_to_inductive_types(locals,
                                                                nparams, to_elab.data(),
                                                                num_decls, to_elab.data() + nparams);
        i = nparams + num_decls;
        for (inductive_decl & decl : decls) {
            buffer<intro_rule> new_irs;
            for (intro_rule const & ir : inductive_decl_intros(decl)) {
                expr type = mlocal_type(to_elab[i]);
                type = mk_intro_rule_type(intro_rule_name(ir), locals, nparams, to_elab.data(), local_to_ind, type);
                new_irs.push_back(update_intro_rule(ir, type));
                i++;
            }
            decl = update_inductive_decl(decl, new_irs);
        }
    }

    /** \brief Create an alias for the fully qualified name \c full_id. */
    environment create_alias(environment env, bool composite, name const & full_id,
                             levels const & ctx_levels, buffer<expr> const & ctx_params) {
        name id;
        if (composite)
            id = name(name(full_id.get_prefix().get_string()), full_id.get_string());
        else
            id = name(full_id.get_string());
        if (!empty(ctx_levels) || !ctx_params.empty()) {
            expr r = mk_local_ref(full_id, ctx_levels, ctx_params);
            m_p.add_local_expr(id, r);
        }
        if (full_id != id)
            env = add_expr_alias_rec(env, id, full_id);
        return env;
    }

    /** \brief Add aliases for the inductive datatype, introduction and elimination rules */
    environment add_aliases(environment env, level_param_names const & ls, buffer<expr> const & locals,
                            buffer<inductive_decl> const & decls) {
        buffer<expr> params_only(locals);
        remove_local_vars(m_p, params_only);
        // Create aliases/local refs
        levels ctx_levels = collect_local_nonvar_levels(m_p, ls);
        for (auto & d : decls) {
            name d_name = inductive_decl_name(d);
            name d_short_name(d_name.get_string());
            env = create_alias(env, false, d_name, ctx_levels, params_only);
            name rec_name = mk_rec_name(d_name);
            env = create_alias(env, true, rec_name, ctx_levels, params_only);
            env = add_protected(env, rec_name);
            for (intro_rule const & ir : inductive_decl_intros(d)) {
                name ir_name = intro_rule_name(ir);
                env = create_alias(env, true, ir_name, ctx_levels, params_only);
            }
        }
        return env;
    }

    void update_declaration_index(environment const & env) {
        name n, k; pos_info p;
        for (auto const & info : m_decl_info) {
            std::tie(n, k, p) = info;
            expr type = env.get(n).get_type();
            m_p.add_decl_index(n, p, k, type);
        }
    }

    environment apply_modifiers(environment env) {
        m_modifiers.for_each([&](name const & n, modifiers const & m) {
                if (m.m_is_class)
                    env = add_class(env, n);
            });
        return env;
    }

    /** \brief Auxiliary method used for debugging */
    void display(std::ostream & out, buffer<inductive_decl> const & decls) {
        if (!m_levels.empty()) {
            out << "inductive level params:";
            for (auto l : m_levels) out << " " << l;
            out << "\n";
        }
        for (auto const & d : decls) {
            name d_name; expr d_type; list<intro_rule> d_irules;
            std::tie(d_name, d_type, d_irules) = d;
            out << "inductive " << d_name << " : " << d_type << "\n";
            for (auto const & ir : d_irules) {
                name ir_name; expr ir_type;
                std::tie(ir_name, ir_type) = ir;
                out << "  | " << ir_name << " : " << ir_type << "\n";
            }
        }
        out << "\n";
    }

    environment operator()() {
        parser::no_undef_id_error_scope err_scope(m_p);
        buffer<inductive_decl> decls;
        {
            parser::local_scope scope(m_p);
            parse_inductive_decls(decls);
        }
        buffer<expr> locals;
        collect_locals(decls, locals);
        include_local_levels(decls, locals);
        elaborate_decls(decls, locals);
        level_param_names ls = to_list(m_levels.begin(), m_levels.end());
        environment env = module::add_inductive(m_p.env(), ls, m_num_params, to_list(decls.begin(), decls.end()));
        update_declaration_index(env);
        env = add_aliases(env, ls, locals, decls);
        return apply_modifiers(env);
    }
};

environment inductive_cmd(parser & p) {
    return inductive_cmd_fn(p)();
}

void register_inductive_cmd(cmd_table & r) {
    add_cmd(r, cmd_info("inductive",   "declare an inductive datatype", inductive_cmd));
}
}
