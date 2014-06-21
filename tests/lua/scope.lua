local env    = environment()
local l      = param_univ("l")
local nat    = Const("nat")
local real   = Const("real")
local one    = Const("one")
local Ul     = mk_sort(l)
local lst_l  = Const("lst", {l})
local vec_l  = Const("vec", {l})
local mat_l  = Const("mat", {l})
local A      = Local("A", Ul)
local n      = Local("n", nat)
local ll     = Local("ll", lst_l(A))
local len_l  = Const("len", {l})
local lst_1  = Const("lst", {1})
local l1     = param_univ("l1")
local l2     = param_univ("l2")
local m      = Local("m", nat)
env = add_decl(env, mk_var_decl("nat", Type))
env = add_decl(env, mk_var_decl("real", Type))
env = add_decl(env, mk_var_decl("one", nat))
env = add_decl(env, mk_var_decl("lst", {l}, mk_arrow(Ul, Ul)))
env = add_decl(env, mk_var_decl("len", {l}, Pi(A, mk_arrow(lst_l(A), nat))))
env = add_decl(env, mk_var_decl("vec", {l}, mk_arrow(Ul, nat, Ul)))
env = add_decl(env, mk_var_decl("mat", {l}, mk_arrow(Ul, nat, nat, Ul)))
env = add_decl(env, mk_var_decl("dlst", {l1, l2}, mk_arrow(mk_sort(l1), mk_sort(l2), mk_sort(max_univ(l1, l2)))))
env = add_decl(env, mk_var_decl("vec2lst", {l}, Pi({A, n}, mk_arrow(vec_l(A, n), lst_l(A)))))
env = add_decl(env, mk_var_decl("lst2vec", {l}, Pi({A, ll}, vec_l(A, len_l(A, ll)))))
env = add_decl(env, mk_var_decl("vec2mat", {l}, Pi({A, n}, mk_arrow(vec_l(A, n), mat_l(A, n, one)))))
env = add_decl(env, mk_var_decl("mat2dlst", {l}, Pi({A, n, m}, mk_arrow(mat_l(A, n, m), Const("dlst", {l, 1})(A, nat)))))
env = add_decl(env, mk_var_decl("nat2lst", mk_arrow(nat, lst_1(nat))))
env = add_coercion(env, "lst2vec")
env = push_scope(env, "tst")
local lst_nat = lst_1(nat)
print(get_coercion(env, lst_nat, "vec"))
env = add_coercion(env, "vec2mat")
print(get_coercion(env, lst_nat, "mat"))
assert(env:type_check(get_coercion(env, lst_nat, "mat")))
function get_num_coercions(env)
   local r = 0
   for_each_coercion_user(env, function(C, D, f) r = r + 1 end)
   return r
end
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 3)
env = pop_scope(env)
print("---------")
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 1)
print("---------")
env = push_scope(env)
env = using_namespace_objects(env, "tst")
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 3)
env = pop_scope(env)
print("---------")
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 1)
print("---------")
env = push_scope(env, "tst")
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 3)
print("---------")
env = push_scope(env)
env = add_coercion(env, "nat2lst")
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 6)
print("---------")
env = pop_scope(env)
assert(get_num_coercions(env) == 3)
env = pop_scope(env)
for_each_coercion_user(env, function(C, D, f) print(tostring(C) .. " >-> " .. tostring(D) .. " : " .. tostring(f)) end)
assert(get_num_coercions(env) == 1)
env = push_scope(env, "tst")
assert(get_num_coercions(env) == 3)