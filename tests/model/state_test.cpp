// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::model::State / StateSet.
#include <gtest/gtest.h>

#include <vox/model/state.hpp>

namespace {

using vox::model::State;
using vox::model::StateSet;
using vox::model::toString;
using enum State;

// The flag helpers are usable in constant expressions.
static_assert(StateSet{}.none());
static_assert(!StateSet{}.any());
static_assert(StateSet{Focused}.test(Focused));
static_assert(StateSet{}.set(Checked).test(Checked));
static_assert(StateSet{Checked} == StateSet{Checked});
static_assert(StateSet{Checked} != StateSet{Mixed});

TEST(StateSet, DefaultIsEmpty) {
  const StateSet empty;
  EXPECT_TRUE(empty.none());
  EXPECT_FALSE(empty.any());
  EXPECT_FALSE(empty.test(Focused));
  EXPECT_EQ(empty.bits(), 0U);
}

TEST(StateSet, SingleStateConstructor) {
  const StateSet focused{Focused};
  EXPECT_TRUE(focused.any());
  EXPECT_FALSE(focused.none());
  EXPECT_TRUE(focused.test(Focused));
  EXPECT_FALSE(focused.test(Focusable));
}

TEST(StateSet, SetIsChainableAndAdditive) {
  StateSet states;
  states.set(Focusable).set(Focused).set(Selected);
  EXPECT_TRUE(states.test(Focusable));
  EXPECT_TRUE(states.test(Focused));
  EXPECT_TRUE(states.test(Selected));
  EXPECT_FALSE(states.test(Disabled));
}

TEST(StateSet, SetIsIdempotent) {
  StateSet a;
  a.set(Checked);
  StateSet b{a};
  b.set(Checked);
  EXPECT_EQ(a, b);
}

TEST(StateSet, ClearRemovesOnlyTheGivenState) {
  StateSet states;
  states.set(Focusable).set(Focused);
  states.clear(Focused);
  EXPECT_TRUE(states.test(Focusable));
  EXPECT_FALSE(states.test(Focused));
}

TEST(StateSet, ClearAbsentStateIsNoOp) {
  StateSet states{Focusable};
  states.clear(Disabled);
  EXPECT_EQ(states, StateSet{Focusable});
}

TEST(StateSet, Equality) {
  StateSet a;
  a.set(Focusable).set(Focused);
  StateSet b;
  b.set(Focused).set(Focusable); // order must not matter
  EXPECT_EQ(a, b);
  b.set(Disabled);
  EXPECT_NE(a, b);
}

// Tri-state checkbox: off, on, and indeterminate are all distinguishable.
TEST(StateSet, TriStateCheckbox) {
  const StateSet off;
  const StateSet on{Checked};
  const StateSet mixed{Mixed};

  EXPECT_FALSE(off.test(Checked));
  EXPECT_FALSE(off.test(Mixed));

  EXPECT_TRUE(on.test(Checked));
  EXPECT_FALSE(on.test(Mixed));

  EXPECT_FALSE(mixed.test(Checked));
  EXPECT_TRUE(mixed.test(Mixed));

  EXPECT_NE(off, on);
  EXPECT_NE(on, mixed);
  EXPECT_NE(off, mixed);
}

// Expandable + Expanded: not-expandable, collapsed, and expanded differ.
TEST(StateSet, CollapsedVersusNotExpandable) {
  const StateSet notExpandable;
  StateSet collapsed;
  collapsed.set(Expandable);
  StateSet expanded;
  expanded.set(Expandable).set(Expanded);

  EXPECT_FALSE(notExpandable.test(Expandable));

  EXPECT_TRUE(collapsed.test(Expandable));
  EXPECT_FALSE(collapsed.test(Expanded));

  EXPECT_TRUE(expanded.test(Expandable));
  EXPECT_TRUE(expanded.test(Expanded));

  EXPECT_NE(notExpandable, collapsed);
  EXPECT_NE(collapsed, expanded);
}

TEST(StateSet, ToStringEmptyIsNone) {
  EXPECT_EQ(toString(StateSet{}), "None");
}

TEST(StateSet, ToStringSingle) {
  EXPECT_EQ(toString(StateSet{Focused}), "Focused");
}

TEST(StateSet, ToStringJoinsInAscendingBitOrder) {
  StateSet states;
  // Set out of order; rendering is by ascending bit value, not insertion order.
  states.set(ReadOnly).set(Focusable).set(Checked);
  EXPECT_EQ(toString(states), "Focusable|Checked|ReadOnly");
}

} // namespace
