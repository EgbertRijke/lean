/-
Copyright (c) 2014 Jeremy Avigad. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Module: data.nat.div
Authors: Jeremy Avigad, Leonardo de Moura

Definitions of div, mod, and gcd on the natural numbers.
-/

import data.nat.sub data.nat.comm_semiring tools.fake_simplifier
open eq.ops well_founded decidable fake_simplifier prod

namespace nat

/- div and mod -/

-- auxiliary lemma used to justify div
private definition div_rec_lemma {x y : nat} (H : 0 < y ∧ y ≤ x) : x - y < x :=
and.rec_on H (λ ypos ylex, sub_lt (lt_of_lt_of_le ypos ylex) ypos)

private definition div.F (x : nat) (f : Π x₁, x₁ < x → nat → nat) (y : nat) : nat :=
if H : 0 < y ∧ y ≤ x then f (x - y) (div_rec_lemma H) y + 1 else zero

definition divide (x y : nat) := fix div.F x y

theorem divide_def (x y : nat) : divide x y = if 0 < y ∧ y ≤ x then divide (x - y) y + 1 else 0 :=
congr_fun (fix_eq div.F x) y

notation a div b := divide a b

theorem div_zero (a : ℕ) : a div 0 = 0 :=
divide_def a 0 ⬝ if_neg (!not_and_of_not_left (lt.irrefl 0))

theorem div_eq_zero_of_lt {a b : ℕ} (h : a < b) : a div b = 0 :=
divide_def a b ⬝ if_neg (!not_and_of_not_right (not_le_of_lt h))

theorem zero_div (b : ℕ) : 0 div b = 0 :=
divide_def 0 b ⬝ if_neg (λ h, and.rec_on h (λ l r, absurd (lt_of_lt_of_le l r) (lt.irrefl 0)))

theorem div_eq_succ_sub_div {a b : ℕ} (h₁ : b > 0) (h₂ : a ≥ b) : a div b = succ ((a - b) div b) :=
divide_def a b ⬝ if_pos (and.intro h₁ h₂)

theorem add_div_left {x z : ℕ} (H : z > 0) : (x + z) div z = succ (x div z) :=
calc
  (x + z) div z = if 0 < z ∧ z ≤ x + z then (x + z - z) div z + 1 else 0 : !divide_def
            ... = (x + z - z) div z + 1 : if_pos (and.intro H (le_add_left z x))
            ... = succ (x div z)        : {!add_sub_cancel}

theorem add_div_right {x z : ℕ} (H : x > 0) : (x + z) div x = succ (z div x) :=
!add.comm ▸ add_div_left H

theorem add_mul_div_left {x y z : ℕ} (H : z > 0) : (x + y * z) div z = x div z + y :=
induction_on y
  (calc (x + zero * z) div z = (x + zero) div z : zero_mul
                       ...   = x div z          : add_zero
                       ...   = x div z + zero   : add_zero)
  (take y,
    assume IH : (x + y * z) div z = x div z + y, calc
      (x + succ y * z) div z = (x + y * z + z) div z    : by simp
                         ... = succ ((x + y * z) div z) : add_div_left H
                         ... = x div z + succ y         : by simp)

theorem add_mul_div_right {x y z : ℕ} (H : y > 0) : (x + y * z) div y = x div y + z :=
!mul.comm ▸ add_mul_div_left H

private definition mod.F (x : nat) (f : Π x₁, x₁ < x → nat → nat) (y : nat) : nat :=
if H : 0 < y ∧ y ≤ x then f (x - y) (div_rec_lemma H) y else x

definition modulo (x y : nat) := fix mod.F x y

notation a mod b := modulo a b

theorem modulo_def (x y : nat) : modulo x y = if 0 < y ∧ y ≤ x then modulo (x - y) y else x :=
congr_fun (fix_eq mod.F x) y

theorem mod_zero (a : ℕ) : a mod 0 = a :=
modulo_def a 0 ⬝ if_neg (!not_and_of_not_left (lt.irrefl 0))

theorem mod_eq_of_lt {a b : ℕ} (h : a < b) : a mod b = a :=
modulo_def a b ⬝ if_neg (!not_and_of_not_right (not_le_of_lt h))

theorem zero_mod (b : ℕ) : 0 mod b = 0 :=
modulo_def 0 b ⬝ if_neg (λ h, and.rec_on h (λ l r, absurd (lt_of_lt_of_le l r) (lt.irrefl 0)))

theorem mod_eq_sub_mod {a b : ℕ} (h₁ : b > 0) (h₂ : a ≥ b) : a mod b = (a - b) mod b :=
modulo_def a b ⬝ if_pos (and.intro h₁ h₂)

theorem add_mod_left {x z : ℕ} (H : z > 0) : (x + z) mod z = x mod z :=
calc
  (x + z) mod z = if 0 < z ∧ z ≤ x + z then (x + z - z) mod z else _ : modulo_def
            ... = (x + z - z) mod z   : if_pos (and.intro H (le_add_left z x))
            ... = x mod z             : add_sub_cancel

theorem add_mod_right {x z : ℕ} (H : x > 0) : (x + z) mod x = z mod x :=
!add.comm ▸ add_mod_left H

theorem add_mul_mod_left {x y z : ℕ} (H : z > 0) : (x + y * z) mod z = x mod z :=
induction_on y
  (calc (x + zero * z) mod z = (x + zero) mod z : zero_mul
                         ... = x mod z          : add_zero)
  (take y,
    assume IH : (x + y * z) mod z = x mod z,
    calc
      (x + succ y * z) mod z = (x + (y * z + z)) mod z : succ_mul
                         ... = (x + y * z + z) mod z   : add.assoc
                         ... = (x + y * z) mod z       : add_mod_left H
                         ... = x mod z                 : IH)

theorem add_mul_mod_right {x y z : ℕ} (H : y > 0) : (x + y * z) mod y = x mod y :=
!mul.comm ▸ add_mul_mod_left H

theorem mul_mod_left {m n : ℕ} : (m * n) mod n = 0 :=
by_cases_zero_pos n (by simp)
  (take n,
    assume npos : n > 0,
    (by simp) ▸ (@add_mul_mod_left 0 m _ npos))

theorem mul_mod_right {m n : ℕ} : (m * n) mod m = 0 :=
!mul.comm ▸ !mul_mod_left

theorem mod_lt {x y : ℕ} (H : y > 0) : x mod y < y :=
case_strong_induction_on x
  (show 0 mod y < y, from !zero_mod⁻¹ ▸ H)
  (take x,
    assume IH : ∀x', x' ≤ x → x' mod y < y,
    show succ x mod y < y, from
      by_cases -- (succ x < y)
        (assume H1 : succ x < y,
          have H2 : succ x mod y = succ x, from mod_eq_of_lt H1,
          show succ x mod y < y, from H2⁻¹ ▸ H1)
        (assume H1 : ¬ succ x < y,
          have H2 : y ≤ succ x, from le_of_not_lt H1,
          have H3 : succ x mod y = (succ x - y) mod y, from mod_eq_sub_mod H H2,
          have H4 : succ x - y < succ x, from sub_lt !succ_pos H,
          have H5 : succ x - y ≤ x, from le_of_lt_succ H4,
          show succ x mod y < y, from H3⁻¹ ▸ IH _ H5))

/- properties of div and mod together -/

-- the quotient / remainder theorem
theorem eq_div_mul_add_mod {x y : ℕ} : x = x div y * y + x mod y :=
by_cases_zero_pos y
  (show x = x div 0 * 0 + x mod 0, from
    (calc
      x div 0 * 0 + x mod 0 = 0 + x mod 0 : mul_zero
        ... = x mod 0                     : zero_add
        ... = x                           : mod_zero)⁻¹)
  (take y,
    assume H : y > 0,
    show x = x div y * y + x mod y, from
      case_strong_induction_on x
        (show 0 = (0 div y) * y + 0 mod y, by simp)
        (take x,
          assume IH : ∀x', x' ≤ x → x' = x' div y * y + x' mod y,
          show succ x = succ x div y * y + succ x mod y, from
            by_cases -- (succ x < y)
              (assume H1 : succ x < y,
                have H2 : succ x div y = 0, from div_eq_zero_of_lt H1,
                have H3 : succ x mod y = succ x, from mod_eq_of_lt H1,
                by simp)
              (assume H1 : ¬ succ x < y,
                have H2 : y ≤ succ x, from le_of_not_lt H1,
                have H3 : succ x div y = succ ((succ x - y) div y), from div_eq_succ_sub_div H H2,
                have H4 : succ x mod y = (succ x - y) mod y, from mod_eq_sub_mod H H2,
                have H5 : succ x - y < succ x, from sub_lt !succ_pos H,
                have H6 : succ x - y ≤ x, from le_of_lt_succ H5,
                (calc
                  succ x div y * y + succ x mod y =
                          succ ((succ x - y) div y) * y + succ x mod y      : H3
                    ... = ((succ x - y) div y) * y + y + succ x mod y       : succ_mul
                    ... = ((succ x - y) div y) * y + y + (succ x - y) mod y : H4
                    ... = ((succ x - y) div y) * y + (succ x - y) mod y + y : add.right_comm
                    ... = succ x - y + y                                    : {!(IH _ H6)⁻¹}
                    ... = succ x                                         : sub_add_cancel H2)⁻¹)))

theorem mod_le {x y : ℕ} : x mod y ≤ x :=
eq_div_mul_add_mod⁻¹ ▸ !le_add_left

theorem eq_remainder {y : ℕ} (H : y > 0) {q1 r1 q2 r2 : ℕ} (H1 : r1 < y) (H2 : r2 < y)
  (H3 : q1 * y + r1 = q2 * y + r2) : r1 = r2 :=
calc
  r1 = r1 mod y : by simp
    ... = (r1 + q1 * y) mod y : (add_mul_mod_left H)⁻¹
    ... = (q1 * y + r1) mod y : add.comm
    ... = (r2 + q2 * y) mod y : by simp
    ... = r2 mod y            : add_mul_mod_left H
    ... = r2                  : by simp

theorem eq_quotient {y : ℕ} (H : y > 0) {q1 r1 q2 r2 : ℕ} (H1 : r1 < y) (H2 : r2 < y)
  (H3 : q1 * y + r1 = q2 * y + r2) : q1 = q2 :=
have H4 : q1 * y + r2 = q2 * y + r2, from (eq_remainder H H1 H2 H3) ▸ H3,
have H5 : q1 * y = q2 * y, from add.cancel_right H4,
have H6 : y > 0, from lt_of_le_of_lt !zero_le H1,
show q1 = q2, from eq_of_mul_eq_mul_right H6 H5

theorem mul_div_mul_left {z x y : ℕ} (zpos : z > 0) : (z * x) div (z * y) = x div y :=
by_cases -- (y = 0)
  (assume H : y = 0, by simp)
  (assume H : y ≠ 0,
    have ypos : y > 0, from pos_of_ne_zero H,
    have zypos : z * y > 0, from mul_pos zpos ypos,
    have H1 : (z * x) mod (z * y) < z * y, from mod_lt zypos,
    have H2 : z * (x mod y) < z * y, from mul_lt_mul_of_pos_left (mod_lt ypos) zpos,
    eq_quotient zypos H1 H2
      (calc
        ((z * x) div (z * y)) * (z * y) + (z * x) mod (z * y) = z * x : eq_div_mul_add_mod
          ... = z * (x div y * y + x mod y)                           : eq_div_mul_add_mod
          ... = z * (x div y * y) + z * (x mod y)                     : mul.left_distrib
          ... = (x div y) * (z * y) + z * (x mod y)                   : mul.left_comm))

theorem mul_div_mul_right {x z y : ℕ} (zpos : z > 0) : (x * z) div (y * z) = x div y :=
!mul.comm ▸ !mul.comm ▸ mul_div_mul_left zpos

theorem mul_mod_mul_left {z x y : ℕ} (zpos : z > 0) : (z * x) mod (z * y) = z * (x mod y) :=
by_cases -- (y = 0)
  (assume H : y = 0, by simp)
  (assume H : y ≠ 0,
    have ypos : y > 0, from pos_of_ne_zero H,
    have zypos : z * y > 0, from mul_pos zpos ypos,
    have H1 : (z * x) mod (z * y) < z * y, from mod_lt zypos,
    have H2 : z * (x mod y) < z * y, from mul_lt_mul_of_pos_left (mod_lt ypos) zpos,
    eq_remainder zypos H1 H2
      (calc
        ((z * x) div (z * y)) * (z * y) + (z * x) mod (z * y) = z * x : eq_div_mul_add_mod
          ... = z * (x div y * y + x mod y)                           : eq_div_mul_add_mod
          ... = z * (x div y * y) + z * (x mod y)                     : mul.left_distrib
          ... = (x div y) * (z * y) + z * (x mod y)                   : mul.left_comm))

theorem mul_mod_mul_right {x z y : ℕ} (zpos : z > 0) : (x * z) mod (y * z) = (x mod y) * z :=
mul.comm z x ▸ mul.comm z y ▸ !mul.comm ▸ mul_mod_mul_left zpos

theorem mod_one (x : ℕ) : x mod 1 = 0 :=
have H1 : x mod 1 < 1, from mod_lt !succ_pos,
eq_zero_of_le_zero (le_of_lt_succ H1)

theorem mod_self (n : ℕ) : n mod n = 0 :=
cases_on n (by simp)
  (take n,
    have H : (succ n * 1) mod (succ n * 1) = succ n * (1 mod 1),
      from mul_mod_mul_left !succ_pos,
    (by simp) ▸ H)

theorem div_one (n : ℕ) : n div 1 = n :=
have H : n div 1 * 1 + n mod 1 = n, from eq_div_mul_add_mod⁻¹,
(by simp) ▸ H

theorem div_self {n : ℕ} (H : n > 0) : n div n = 1 :=
have H1 : (n * 1) div (n * 1) = 1 div 1, from mul_div_mul_left H,
(by simp) ▸ H1

theorem div_mul_eq_of_mod_eq_zero {x y : ℕ} (H : x mod y = 0) : x div y * y = x :=
(calc
  x = x div y * y + x mod y : eq_div_mul_add_mod
    ... = x div y * y + 0 : H
    ... = x div y * y : !add_zero)⁻¹

/- divides -/

theorem dvd_of_mod_eq_zero {x y : ℕ} (H : y mod x = 0) : x | y :=
dvd.intro (!mul.comm ▸ div_mul_eq_of_mod_eq_zero H)

theorem mod_eq_zero_of_dvd {x y : ℕ} (H : x | y) : y mod x = 0 :=
dvd.elim H
  (take z,
    assume H1 : x * z = y,
    H1 ▸ !mul_mod_right)

theorem dvd_iff_mod_eq_zero (x y : ℕ) : x | y ↔ y mod x = 0 :=
iff.intro mod_eq_zero_of_dvd dvd_of_mod_eq_zero

definition dvd.decidable_rel [instance] : decidable_rel dvd :=
take m n, decidable_of_decidable_of_iff _ (iff.symm !dvd_iff_mod_eq_zero)

theorem div_mul_eq_of_dvd {x y : ℕ} (H : y | x) : x div y * y = x :=
div_mul_eq_of_mod_eq_zero (mod_eq_zero_of_dvd H)

theorem dvd_of_dvd_add_left {m n1 n2 : ℕ} : m | (n1 + n2) → m | n1 → m | n2 :=
by_cases_zero_pos m
  (assume (H1 : 0 | n1 + n2) (H2 : 0 | n1),
    have H3 : n1 + n2 = 0, from eq_zero_of_zero_dvd H1,
    have H4 : n1 = 0, from eq_zero_of_zero_dvd H2,
    have H5 : n2 = 0, from calc
      n2   = 0  + n2 : zero_add
       ... = n1 + n2 : H4
       ... = 0       : H3,
    show 0 | n2, from H5 ▸ dvd.refl n2)
  (take m,
    assume mpos : m > 0,
    assume H1 : m | (n1 + n2),
    assume H2 : m | n1,
    have H3 : n1 + n2 = n1 + n2 div m * m, from calc
      n1 + n2 = (n1 + n2) div m * m         : div_mul_eq_of_dvd H1
          ... = (n1 div m * m + n2) div m * m : div_mul_eq_of_dvd H2
          ... = (n2 + n1 div m * m) div m * m : add.comm
          ... = (n2 div m + n1 div m) * m     : add_mul_div_left mpos
          ... = n2 div m * m + n1 div m * m   : mul.right_distrib
          ... = n1 div m * m + n2 div m * m   : add.comm
          ... = n1 + n2 div m * m             : div_mul_eq_of_dvd H2,
    have H4 : n2 = n2 div m * m, from add.cancel_left H3,
    have H5 : m * (n2 div m) = n2, from !mul.comm ▸ H4⁻¹,
    dvd.intro H5)

theorem dvd_of_dvd_add_right {m n1 n2 : ℕ} (H : m | (n1 + n2)) : m | n2 → m | n1 :=
dvd_of_dvd_add_left (!add.comm ▸ H)

theorem dvd_sub {m n1 n2 : ℕ} (H1 : m | n1) (H2 : m | n2) : m | (n1 - n2) :=
by_cases
  (assume H3 : n1 ≥ n2,
    have H4 : n1 = n1 - n2 + n2, from (sub_add_cancel H3)⁻¹,
    show m | n1 - n2, from dvd_of_dvd_add_right (H4 ▸ H1) H2)
  (assume H3 : ¬ (n1 ≥ n2),
    have H4 : n1 - n2 = 0, from sub_eq_zero_of_le (le_of_lt (lt_of_not_le H3)),
    show m | n1 - n2, from H4⁻¹ ▸ dvd_zero _)

theorem dvd.antisymm {m n : ℕ} : m | n → n | m → m = n :=
by_cases_zero_pos n
  (assume H1, assume H2 : 0 | m, eq_zero_of_zero_dvd H2)
  (take n,
    assume Hpos : n > 0,
    assume H1 : m | n,
    assume H2 : n | m,
    obtain k (Hk : m * k = n), from dvd.ex H1,
    obtain l (Hl : n * l = m), from dvd.ex H2,
    have H3 : n * (l * k) = n, from !mul.assoc ▸ Hl⁻¹ ▸ Hk,
    have H4 : l * k = 1, from eq_one_of_mul_eq_self_right Hpos H3,
    have H5 : k = 1, from eq_one_of_mul_eq_one_left H4,
    show m = n, from (mul_one m)⁻¹ ⬝ (H5 ▸ Hk))

/- gcd and lcm -/

private definition pair_nat.lt : nat × nat → nat × nat → Prop := measure pr₂
private definition pair_nat.lt.wf : well_founded pair_nat.lt :=
intro_k (measure.wf pr₂) 20  -- we use intro_k to be able to execute gcd efficiently in the kernel

attribute pair_nat.lt.wf [instance]      -- instance will not be saved in .olean
infixl [local] `≺`:50 := pair_nat.lt

private definition gcd.lt.dec (x y₁ : nat) : (succ y₁, x mod succ y₁) ≺ (x, succ y₁) :=
mod_lt (succ_pos y₁)

definition gcd.F (p₁ : nat × nat) : (Π p₂ : nat × nat, p₂ ≺ p₁ → nat) → nat :=
prod.cases_on p₁ (λx y, cases_on y
  (λ f, x)
  (λ y₁ (f : Πp₂, p₂ ≺ (x, succ y₁) → nat), f (succ y₁, x mod succ y₁) !gcd.lt.dec))

definition gcd (x y : nat) := fix gcd.F (pair x y)

theorem gcd_zero (x : nat) : gcd x 0 = x :=
well_founded.fix_eq gcd.F (x, 0)

theorem gcd_succ (x y : nat) : gcd x (succ y) = gcd (succ y) (x mod succ y) :=
well_founded.fix_eq gcd.F (x, succ y)

theorem gcd_one (n : ℕ) : gcd n 1 = 1 :=
calc gcd n 1 = gcd 1 (n mod 1) : gcd_succ n zero
         ... = gcd 1 0         : mod_one
         ... = 1               : gcd_zero

theorem gcd_def (x y : ℕ) : gcd x y = if y = 0 then x else gcd y (x mod y) :=
cases_on y
  (calc gcd x 0 = x                                          : gcd_zero x
           ...  = if 0 = 0 then x else gcd zero (x mod zero) : (if_pos rfl)⁻¹)
  (λy₁, calc
    gcd x (succ y₁) = gcd (succ y₁) (x mod succ y₁)                  : gcd_succ x y₁
      ... = if succ y₁ = 0 then x else gcd (succ y₁) (x mod succ y₁) : (if_neg (succ_ne_zero y₁))⁻¹)

theorem gcd_rec (m : ℕ) {n : ℕ} (H : n > 0) : gcd m n = gcd n (m mod n) :=
gcd_def m n ⬝ if_neg (ne_zero_of_pos H)

theorem gcd_self (n : ℕ) : gcd n n = n :=
cases_on n
  rfl
  (λn₁, calc
    gcd (succ n₁) (succ n₁) = gcd (succ n₁) (succ n₁ mod succ n₁) : gcd_succ (succ n₁) n₁
                      ...   = gcd (succ n₁) 0                     : mod_self (succ n₁)
                      ...   = succ n₁                             : gcd_zero)

theorem gcd_zero_left (n : nat) : gcd 0 n = n :=
cases_on n
  rfl
  (λ n₁, calc
    gcd 0 (succ n₁) = gcd (succ n₁) (0 mod succ n₁) : gcd_succ
                ... = gcd (succ n₁) 0               : zero_mod
                ... = (succ n₁)                     : gcd_zero)

theorem gcd.induction {P : ℕ → ℕ → Prop}
                   (m n : ℕ)
                   (H0 : ∀m, P m 0)
                   (H1 : ∀m n, 0 < n → P n (m mod n) → P m n) :
                 P m n :=
let Q : nat × nat → Prop := λ p : nat × nat, P (pr₁ p) (pr₂ p) in
have aux : Q (m, n), from
  well_founded.induction (m, n) (λp, prod.cases_on p
    (λm n, cases_on n
      (λ ih, show P (pr₁ (m, 0)) (pr₂ (m, 0)), from H0 m)
      (λ n₁ (ih : ∀p₂, p₂ ≺ (m, succ n₁) → P (pr₁ p₂) (pr₂ p₂)),
        have hlt₁ : 0 < succ n₁, from succ_pos n₁,
        have hlt₂ : (succ n₁, m mod succ n₁) ≺ (m, succ n₁), from gcd.lt.dec _ _,
        have hp   : P (succ n₁) (m mod succ n₁), from ih _ hlt₂,
        show P m (succ n₁), from
          H1 m (succ n₁) hlt₁ hp))),
aux

theorem gcd_dvd (m n : ℕ) : (gcd m n | m) ∧ (gcd m n | n) :=
gcd.induction m n
  (take m,
    show (gcd m 0 | m) ∧ (gcd m 0 | 0), by simp)
  (take m n,
    assume npos : 0 < n,
    assume IH : (gcd n (m mod n) | n) ∧ (gcd n (m mod n) | (m mod n)),
    have H : gcd n (m mod n) | (m div n * n + m mod n), from
      dvd_add (dvd.trans (and.elim_left IH) !dvd_mul_left) (and.elim_right IH),
    have H1 : gcd n (m mod n) | m, from eq_div_mul_add_mod⁻¹ ▸ H,
    have gcd_eq : gcd n (m mod n) = gcd m n, from (gcd_rec _ npos)⁻¹,
    show (gcd m n | m) ∧ (gcd m n | n), from gcd_eq ▸ (and.intro H1 (and.elim_left IH)))

theorem gcd_dvd_left (m n : ℕ) : (gcd m n | m) := and.elim_left !gcd_dvd

theorem gcd_dvd_right (m n : ℕ) : (gcd m n | n) := and.elim_right !gcd_dvd

theorem dvd_gcd {m n k : ℕ} : k | m → k | n → k | (gcd m n) :=
gcd.induction m n
  (take m, assume (h₁ : k | m) (h₂ : k | 0),
   show k | gcd m 0, from !gcd_zero⁻¹ ▸ h₁)
  (take m n,
    assume npos : n > 0,
    assume IH : k | n → k | (m mod n) → k | gcd n (m mod n),
    assume H1 : k | m,
    assume H2 : k | n,
    have H3 : k | m div n * n + m mod n, from eq_div_mul_add_mod ▸ H1,
    have H4 : k | m mod n, from nat.dvd_of_dvd_add_left H3 (dvd.trans H2 (by simp)),
    have gcd_eq : gcd n (m mod n) = gcd m n, from (gcd_rec _ npos)⁻¹,
    show k | gcd m n, from gcd_eq ▸ IH H2 H4)

theorem gcd.comm (m n : ℕ) : gcd m n = gcd n m :=
dvd.antisymm
  (dvd_gcd !gcd_dvd_right !gcd_dvd_left)
  (dvd_gcd !gcd_dvd_right !gcd_dvd_left)

theorem gcd.assoc (m n k : ℕ) : gcd (gcd m n) k = gcd m (gcd n k) :=
dvd.antisymm
  (dvd_gcd
    (dvd.trans !gcd_dvd_left !gcd_dvd_left)
    (dvd_gcd (dvd.trans !gcd_dvd_left !gcd_dvd_right) !gcd_dvd_right))
  (dvd_gcd
    (dvd_gcd !gcd_dvd_left (dvd.trans !gcd_dvd_right !gcd_dvd_left))
    (dvd.trans !gcd_dvd_right !gcd_dvd_right))

end nat
