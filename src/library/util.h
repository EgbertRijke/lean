/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include "kernel/environment.h"
#include "kernel/type_checker.h"

namespace lean {
typedef std::unique_ptr<type_checker> type_checker_ptr;

/** \brief Return true iff \c e is of the form <tt>(f ...)</tt> where
    \c f is a non-opaque constant definition */
bool is_def_app(environment const & env, expr const & e);

/** \brief If \c e is of the form <tt>(f a_1 ... a_n)</tt>, where \c f is
    a non-opaque definition, then unfold \c f, and beta reduce.
    Otherwise, return none.
*/
optional<expr> unfold_app(environment const & env, expr const & e);

/** \brief Reduce (if possible) universe level by 1.
    \pre is_not_zero(l)
*/
optional<level> dec_level(level const & l);

bool has_unit_decls(environment const & env);
bool has_eq_decls(environment const & env);
bool has_heq_decls(environment const & env);
bool has_prod_decls(environment const & env);
bool has_lift_decls(environment const & env);
/** \brief Return true iff \c n is the name of a recursive datatype in \c env.
    That is, it must be an inductive datatype AND contain a recursive constructor.

    \remark Records are inductive datatypes, but they are not recursive.

    \remark For mutually indutive datatypes, \c n is considered recursive
    if there is a constructor taking \c n.
*/
bool is_recursive_datatype(environment const & env, name const & n);

/** \brief Return true if \c n is a recursive *and* reflexive datatype.

    We say an inductive type T is reflexive if it contains at least one constructor that
    takes as an argument a function returning T.
*/
bool is_reflexive_datatype(type_checker & tc, name const & n);

/** \brief Return true iff \c n is an inductive predicate, i.e., an inductive datatype that is in Prop.

    \remark If \c env does not have Prop (i.e., Type.{0} is not impredicative), then this method always return false.
*/
bool is_inductive_predicate(environment const & env, name const & n);

/** \brief Store in \c result the introduction rules of the given inductive datatype.
    \remark this procedure does nothing if \c n is not an inductive datatype.
*/
void get_intro_rule_names(environment const & env, name const & n, buffer<name> & result);

/** \brief "Consume" Pi-type \c type. This procedure creates local constants based on the domain of \c type
    and store them in telescope. If \c binfo is provided, then the local constants are annoted with the given
    binder_info, otherwise the procedure uses the one attached in the domain.
    The procedure returns the "body" of type.
*/
expr to_telescope(name_generator & ngen, expr const & type, buffer<expr> & telescope,
                  optional<binder_info> const & binfo = optional<binder_info>());
/** \brief Similar to previous procedure, but puts \c type in whnf */
expr to_telescope(type_checker & tc, expr type, buffer<expr> & telescope,
                  optional<binder_info> const & binfo = optional<binder_info>());
/** \brief Similar to previous procedure, but also accumulates constraints generated while
    normalizing type.

    \remark Constraints are generated only if \c type constains metavariables.
*/
expr to_telescope(type_checker & tc, expr type, buffer<expr> & telescope, optional<binder_info> const & binfo,
                  constraint_seq & cs);

/** \brief "Consume" fun/lambda. This procedure creates local constants based on the arguments of \c e
    and store them in telescope. If \c binfo is provided, then the local constants are annoted with the given
    binder_info, otherwise the procedure uses the one attached to the arguments.
    The procedure returns the "body" of function.
*/
expr fun_to_telescope(name_generator & ngen, expr const & e, buffer<expr> & telescope, optional<binder_info> const & binfo);

/** \brief Return the universe where inductive datatype resides
    \pre \c ind_type is of the form <tt>Pi (a_1 : A_1) (a_2 : A_2[a_1]) ..., Type.{lvl}</tt>
*/
level get_datatype_level(expr ind_type);

expr instantiate_univ_param (expr const & e, name const & p, level const & l);

expr mk_true();
expr mk_true_intro();
expr mk_and(expr const & a, expr const & b);
expr mk_and_intro(type_checker & tc, expr const & Ha, expr const & Hb);
expr mk_and_elim_left(type_checker & tc, expr const & H);
expr mk_and_elim_right(type_checker & tc, expr const & H);

expr mk_unit(level const & l);
expr mk_unit_mk(level const & l);
expr mk_prod(type_checker & tc, expr const & A, expr const & B);
expr mk_pair(type_checker & tc, expr const & a, expr const & b);
expr mk_pr1(type_checker & tc, expr const & p);
expr mk_pr2(type_checker & tc, expr const & p);

expr mk_unit(level const & l, bool prop);
expr mk_unit_mk(level const & l, bool prop);
expr mk_prod(type_checker & tc, expr const & a, expr const & b, bool prop);
expr mk_pair(type_checker & tc, expr const & a, expr const & b, bool prop);
expr mk_pr1(type_checker & tc, expr const & p, bool prop);
expr mk_pr2(type_checker & tc, expr const & p, bool prop);

expr mk_eq(type_checker & tc, expr const & lhs, expr const & rhs);
expr mk_refl(type_checker & tc, expr const & a);
bool is_eq_rec(expr const & e);
bool is_eq(expr const & e);
/** \brief Return true iff \c e is of the form (eq A a a) */
bool is_eq_a_a(expr const & e);
/** \brief Return true iff \c e is of the form (eq A a a') where \c a and \c a' are definitionally equal */
bool is_eq_a_a(type_checker & tc, expr const & e);

/** \brief Create a telescope equality for HoTT library.
    This procedure assumes eq supports dependent elimination.
    For HoTT, we can't use heterogeneous equality.
*/
void mk_telescopic_eq(type_checker & tc, buffer<expr> const & t, buffer<expr> const & s, buffer<expr> & eqs);
void mk_telescopic_eq(type_checker & tc, buffer<expr> const & t, buffer<expr> & eqs);

level mk_max(levels const & ls);

expr mk_sigma_mk(type_checker & tc, buffer<expr> const & ts, buffer<expr> const & as, constraint_seq & cs);

enum class implicit_infer_kind { Implicit, RelaxedImplicit, None };

/** \brief Infer implicit parameter annotations for the first \c nparams using mode
    specified by \c k.
*/
expr infer_implicit_params(expr const & type, unsigned nparams, implicit_infer_kind k);

void initialize_library_util();
void finalize_library_util();
}
