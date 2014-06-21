abbreviation Bool : Type.{1} := Type.{0}
variable and : Bool → Bool → Bool
infixl `∧` 25 := and
variable and_intro : forall (a b : Bool), a → b → a ∧ b
variables a b c d : Bool
axiom Ha : a
axiom Hb : b
axiom Hc : c
check
  have a ∧ b, from and_intro a b Ha Hb,
  have [fact] b ∧ a, from and_intro b a Hb Ha,
  have H : a ∧ b, from and_intro a b Ha Hb,
  have H [fact] : a ∧ b, from and_intro a b Ha Hb,
  then have a ∧ b, from and_intro a b Ha Hb,
  then have [fact] b ∧ a, from and_intro b a Hb Ha,
  then have H : a ∧ b, from and_intro a b Ha Hb,
  then have H [fact] : a ∧ b, from and_intro a b Ha Hb,
    Ha