/-
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.
Authors: Floris van Doorn, Leonardo de Moura
-/
prelude
import init.wf init.tactic init.hedberg init.util init.types.sum

open eq.ops decidable sum

namespace nat
  open lift
  notation `ℕ` := nat

  inductive lt (a : nat) : nat → Type :=
  base : lt a (succ a),
  step : Π {b}, lt a b → lt a (succ b)

  notation a < b := lt a b

  definition le (a b : nat) : Type₀ := a < succ b

  notation a ≤ b := le a b

  definition pred (a : nat) : nat :=
  cases_on a zero (λ a₁, a₁)

  protected definition is_inhabited [instance] : inhabited nat :=
  inhabited.mk zero

  protected definition has_decidable_eq [instance] : decidable_eq nat :=
  λn m : nat,
  have general : ∀n, decidable (n = m), from
    rec_on m
      (λ n, cases_on n
        (inl rfl)
        (λ m, inr (λ (e : succ m = zero), down (no_confusion e))))
      (λ (m' : nat) (ih : ∀n, decidable (n = m')) (n : nat), cases_on n
        (inr (λ h, down (no_confusion h)))
        (λ (n' : nat),
          decidable.rec_on (ih n')
            (assume Heq : n' = m', inl (eq.rec_on Heq rfl))
            (assume Hne : n' ≠ m',
              have H1 : succ n' ≠ succ m', from
                assume Heq, down (no_confusion Heq (λ e : n' = m', Hne e)),
              inr H1))),
  general n

  -- less-than is well-founded
  definition lt.wf [instance] : well_founded lt :=
  well_founded.intro (λn, rec_on n
    (acc.intro zero (λ (y : nat) (hlt : y < zero),
      have aux : ∀ {n₁}, y < n₁ → zero = n₁ → acc lt y, from
        λ n₁ hlt, lt.cases_on hlt
          (λ heq, down (no_confusion heq))
          (λ b hlt heq, down (no_confusion heq)),
      aux hlt rfl))
    (λ (n : nat) (ih : acc lt n),
      acc.intro (succ n) (λ (m : nat) (hlt : m < succ n),
        have aux : ∀ {n₁} (hlt : m < n₁), succ n = n₁ → acc lt m, from
          λ n₁ hlt, lt.cases_on hlt
            (λ (heq : succ n = succ m),
              down (no_confusion heq (λ (e : n = m),
                eq.rec_on e ih)))
            (λ b (hlt : m < b) (heq : succ n = succ b),
              down (no_confusion heq (λ (e : n = b),
                acc.inv (eq.rec_on e ih) hlt))),
        aux hlt rfl)))

  definition measure {A : Type} (f : A → nat) : A → A → Type₀ :=
  inv_image lt f

  definition measure.wf {A : Type} (f : A → nat) : well_founded (measure f) :=
  inv_image.wf f lt.wf

  definition not_lt_zero (a : nat) : ¬ a < zero :=
  have aux : ∀ {b}, a < b → b = zero → empty, from
    λ b H, lt.cases_on H
      (λ heq, down (no_confusion heq))
      (λ b h₁ heq, down (no_confusion heq)),
  λ H, aux H rfl

  definition zero_lt_succ (a : nat) : zero < succ a :=
  rec_on a
    (lt.base zero)
    (λ a (hlt : zero < succ a), lt.step hlt)

  definition lt.trans {a b c : nat} (H₁ : a < b) (H₂ : b < c) : a < c :=
  have aux : ∀ {d}, d < c → b = d → a < b → a < c, from
    (λ d H, lt.rec_on H
      (λ h₁ h₂, lt.step (eq.rec_on h₁ h₂))
      (λ b hl ih h₁ h₂, lt.step (ih h₁ h₂))),
  aux H₂ rfl H₁

  definition lt.succ_of_lt {a b : nat} (H : a < b) : succ a < succ b :=
  lt.rec_on H
    (lt.base (succ a))
    (λ b hlt ih, lt.trans ih (lt.base (succ b)))

  definition lt.of_succ_lt {a b : nat} (H : succ a < b) : a < b :=
  have aux : ∀ {a₁}, a₁ < b → succ a = a₁ → a < b, from
    λ a₁ H, lt.rec_on H
      (λ e₁, eq.rec_on e₁ (lt.step (lt.base a)))
      (λ d hlt ih e₁, lt.step (ih e₁)),
  aux H rfl

  definition lt.of_succ_lt_succ {a b : nat} (H : succ a < succ b) : a < b :=
  have aux : pred (succ a) < pred (succ b), from
    lt.rec_on H
      (lt.base a)
      (λ (b : nat) (hlt : succ a < b) ih,
        show pred (succ a) < pred (succ b), from
        lt.of_succ_lt hlt),
  aux

  definition lt.is_decidable_rel [instance] : decidable_rel lt :=
  λ a b, rec_on b
    (λ (a : nat), inr (not_lt_zero a))
    (λ (b₁ : nat) (ih : ∀ a, decidable (a < b₁)) (a : nat), cases_on a
       (inl !zero_lt_succ)
       (λ a, decidable.rec_on (ih a)
         (λ h_pos : a < b₁, inl (lt.succ_of_lt h_pos))
         (λ h_neg : ¬ a < b₁,
           have aux : ¬ succ a < succ b₁, from
             λ h : succ a < succ b₁, h_neg (lt.of_succ_lt_succ h),
           inr aux)))
    a

  definition le.refl (a : nat) : a ≤ a :=
  lt.base a

  definition le.of_lt {a b : nat} (H : a < b) : a ≤ b :=
  lt.step H

  definition eq_or_lt_of_le {a b : nat} (H : a ≤ b) : sum (a = b) (a < b) :=
  have aux : Π (a₁ b₁ : nat) (hlt : a₁ < b₁), a₁ = a → b₁ = (succ b) → sum (a = b) (a < b), from
    λ a₁ b₁ hlt, lt.rec_on hlt
      (λ h₁, eq.rec_on h₁ (λ h₂, down (no_confusion h₂ (λ h₃, eq.rec_on h₃ (sum.inl rfl)))))
      (λ b₁ hlt ih h₁, eq.rec_on h₁ (λ h₂, down (no_confusion h₂ (λ h₃, eq.rec_on h₃ (sum.inr hlt))))),
  aux a (succ b) H rfl rfl

  definition le.of_eq_or_lt {a b : nat} (H : sum (a = b) (a < b)) : a ≤ b :=
  sum.rec_on H
    (λ hl, eq.rec_on hl !le.refl)
    (λ hr, le.of_lt hr)

  definition le.is_decidable_rel [instance] : decidable_rel le :=
  λ a b, decidable_iff_equiv _ (iff.intro le.of_eq_or_lt eq_or_lt_of_le)

  definition le.rec_on {a : nat} {P : nat → Type} {b : nat} (H : a ≤ b) (H₁ : P a) (H₂ : ∀ b, a < b → P b) : P b :=
  begin
    cases H with (b', hlt),
      apply H₁,
      apply (H₂ b hlt)
  end

  definition lt.irrefl (a : nat) : ¬ a < a :=
  rec_on a
    !not_lt_zero
    (λ (a : nat) (ih : ¬ a < a) (h : succ a < succ a),
      ih (lt.of_succ_lt_succ h))

  definition lt.asymm {a b : nat} (H : a < b) : ¬ b < a :=
  lt.rec_on H
    (λ h : succ a < a, !lt.irrefl (lt.of_succ_lt h))
    (λ b hlt (ih : ¬ b < a) (h : succ b < a), ih (lt.of_succ_lt h))

  definition lt.trichotomy (a b : nat) : a < b ⊎ a = b ⊎ b < a :=
  rec_on b
    (λa, cases_on a
       (sum.inr (sum.inl rfl))
       (λ a₁, sum.inr (sum.inr !zero_lt_succ)))
    (λ b₁ (ih : ∀a, a < b₁ ⊎ a = b₁ ⊎ b₁ < a) (a : nat), cases_on a
       (sum.inl !zero_lt_succ)
       (λ a, sum.rec_on (ih a)
           (λ h : a < b₁, sum.inl (lt.succ_of_lt h))
           (λ h, sum.rec_on h
              (λ h : a = b₁, sum.inr (sum.inl (eq.rec_on h rfl)))
              (λ h : b₁ < a, sum.inr (sum.inr (lt.succ_of_lt h))))))
    a

  definition eq_or_lt_of_not_lt {a b : nat} (hnlt : ¬ a < b) : a = b ⊎ b < a :=
  sum.rec_on (lt.trichotomy a b)
    (λ hlt, absurd hlt hnlt)
    (λ h, h)

  definition lt_succ_of_le {a b : nat} (h : a ≤ b) : a < succ b :=
  h

  definition lt_of_succ_le {a b : nat} (h : succ a ≤ b) : a < b :=
  lt.of_succ_lt_succ h

  definition le.step {a b : nat} (h : a ≤ b) : a ≤ succ b :=
  lt.step h

  definition succ_le_of_lt {a b : nat} (h : a < b) : succ a ≤ b :=
  lt.succ_of_lt h

  definition le.trans {a b c : nat} (h₁ : a ≤ b) (h₂ : b ≤ c) : a ≤ c :=
  begin
    cases h₁ with (b', hlt),
      apply h₂,
      apply (lt.trans hlt h₂)
  end

  definition lt.of_le_of_lt {a b c : nat} (h₁ : a ≤ b) (h₂ : b < c) : a < c :=
  begin
    cases h₁ with (b', hlt),
      apply h₂,
      apply (lt.trans hlt h₂)
  end

  definition lt.of_lt_of_le {a b c : nat} (h₁ : a < b) (h₂ : b ≤ c) : a < c :=
  begin
    cases h₁ with (b', hlt),
      apply (lt.of_succ_lt_succ h₂),
      apply (lt.trans hlt (lt.of_succ_lt_succ h₂))
  end

  definition lt.of_lt_of_eq {a b c : nat} (h₁ : a < b) (h₂ : b = c) : a < c :=
  eq.rec_on h₂ h₁

  definition le.of_le_of_eq {a b c : nat} (h₁ : a ≤ b) (h₂ : b = c) : a ≤ c :=
  eq.rec_on h₂ h₁

  definition lt.of_eq_of_lt {a b c : nat} (h₁ : a = b) (h₂ : b < c) : a < c :=
  eq.rec_on (eq.rec_on h₁ rfl) h₂

  definition le.of_eq_of_le {a b c : nat} (h₁ : a = b) (h₂ : b ≤ c) : a ≤ c :=
  eq.rec_on (eq.rec_on h₁ rfl) h₂

  calc_trans lt.trans
  calc_trans lt.of_le_of_lt
  calc_trans lt.of_lt_of_le
  calc_trans lt.of_lt_of_eq
  calc_trans lt.of_eq_of_lt
  calc_trans le.trans
  calc_trans le.of_le_of_eq
  calc_trans le.of_eq_of_le

  definition max (a b : nat) : nat :=
  if a < b then b else a

  definition min (a b : nat) : nat :=
  if a < b then a else b

  definition max_a_a (a : nat) : a = max a a :=
  eq.rec_on !if_t_t rfl

  definition max.eq_right {a b : nat} (H : a < b) : max a b = b :=
  if_pos H

  definition max.eq_left {a b : nat} (H : ¬ a < b) : max a b = a :=
  if_neg H

  definition max.right_eq {a b : nat} (H : a < b) : b = max a b :=
  eq.rec_on (max.eq_right H) rfl

  definition max.left_eq {a b : nat} (H : ¬ a < b) : a = max a b :=
  eq.rec_on (max.eq_left H) rfl

  definition max.left (a b : nat) : a ≤ max a b :=
  by_cases
    (λ h : a < b,   le.of_lt (eq.rec_on (max.right_eq h) h))
    (λ h : ¬ a < b, eq.rec_on (max.eq_left h) !le.refl)

  definition max.right (a b : nat) : b ≤ max a b :=
  by_cases
    (λ h : a < b,   eq.rec_on (max.eq_right h) !le.refl)
    (λ h : ¬ a < b, sum.rec_on (eq_or_lt_of_not_lt h)
      (λ heq, eq.rec_on heq (eq.rec_on (max_a_a a) !le.refl))
      (λ h : b < a,
        have aux : a = max a b, from max.left_eq (lt.asymm h),
        eq.rec_on aux (le.of_lt h)))

  definition gt a b := lt b a

  notation a > b := gt a b

  definition ge a b := le b a

  notation a ≥ b := ge a b

  definition add (a b : nat) : nat :=
  rec_on b a (λ b₁ r, succ r)

  notation a + b := add a b

  definition sub (a b : nat) : nat :=
  rec_on b a (λ b₁ r, pred r)

  notation a - b := sub a b

  definition mul (a b : nat) : nat :=
  rec_on b zero (λ b₁ r, r + a)

  notation a * b := mul a b

  attribute sub [reducible]
  definition succ_sub_succ_eq_sub (a b : nat) : succ a - succ b = a - b :=
  rec_on b
    rfl
    (λ b₁ (ih : succ a - succ b₁ = a - b₁),
      eq.rec_on ih (eq.refl (pred (succ a - succ b₁))))

  definition sub_eq_succ_sub_succ (a b : nat) : a - b = succ a - succ b :=
  eq.rec_on (succ_sub_succ_eq_sub a b) rfl

  definition zero_sub_eq_zero (a : nat) : zero - a = zero :=
  rec_on a
    rfl
    (λ a₁ (ih : zero - a₁ = zero), calc
      zero - succ a₁ = pred (zero - a₁) : rfl
                 ... = pred zero        : ih
                 ... = zero             : rfl)

  definition zero_eq_zero_sub (a : nat) : zero = zero - a :=
  eq.rec_on (zero_sub_eq_zero a) rfl

  definition sub.lt {a b : nat} : zero < a → zero < b → a - b < a :=
  have aux : Π {a}, zero < a → Π {b}, zero < b → a - b < a, from
    λa h₁, lt.rec_on h₁
      (λb h₂, lt.cases_on h₂
        (lt.base zero)
        (λ b₁ bpos,
          eq.rec_on (sub_eq_succ_sub_succ zero b₁)
           (eq.rec_on (zero_eq_zero_sub b₁) (lt.base zero))))
      (λa₁ apos ih b h₂, lt.cases_on h₂
        (lt.base a₁)
        (λ b₁ bpos,
          eq.rec_on (sub_eq_succ_sub_succ a₁ b₁)
            (lt.trans (@ih b₁ bpos) (lt.base a₁)))),
  λ h₁ h₂, aux h₁ h₂

  definition pred_le (a : nat) : pred a ≤ a :=
  cases_on a
    (le.refl zero)
    (λ a₁, le.of_lt (lt.base a₁))

  definition sub_le (a b : nat) : a - b ≤ a :=
  rec_on b
    (le.refl a)
    (λ b₁ ih, le.trans !pred_le ih)

  definition of_num [coercion] [reducible] (n : num) : ℕ :=
  num.rec zero
    (λ n, pos_num.rec (succ zero) (λ n r, r + r + (succ zero)) (λ n r, r + r) n) n
end nat
