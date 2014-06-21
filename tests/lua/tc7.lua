local env = environment()
env = add_decl(env, mk_var_decl("N", Type))
local N = Const("N")
env = add_decl(env, mk_var_decl("f", mk_arrow(N, N)))
env = add_decl(env, mk_var_decl("g", mk_arrow(N, N)))
env = add_decl(env, mk_var_decl("a", N))
local f = Const("f")
local g = Const("g")
local x = Local("x", N)
env = add_decl(env, mk_definition("h", mk_arrow(N, N), Fun(x, f(x)), {opaque=false}))
local h = Const("h")
local a = Const("a")
local m1   = mk_metavar("m1", N)
local cs   = {}
local ngen = name_generator("tst")
local tc   = type_checker(env, ngen, constraint_handler(function (c) print(c); cs[#cs+1] = c end))
assert(tc:is_def_eq(f(m1), g(a)))
assert(tc:is_def_eq(f(m1), a))
assert(not tc:is_def_eq(f(a), a))
assert(tc:is_def_eq(mk_lambda("x", N, Var(0)), h(m1)))
assert(tc:is_def_eq(h(a), f(a)))
assert(tc:is_def_eq(h(a), f(m1)))