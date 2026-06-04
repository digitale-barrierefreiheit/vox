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

// The flag helpers are usable in constant expressions.
static_assert(StateSet{}.none());
static_assert(!StateSet{}.any());
static_assert(StateSet{State::Focused}.test(State::Focused));
static_assert(StateSet{}.set(State::Checked).test(State::Checked));
static_assert(StateSet{State::Checked} == StateSet{State::Checked});
static_assert(StateSet{State::Checked} != StateSet{State::Mixed});

TEST(StateSet, DefaultIsEmpty) {
  const StateSet empty;
  EXPECT_TRUE(empty.none());
  EXPECT_FALSE(empty.any());
  EXPECT_FALSE(empty.test(State::Focused));
  EXPECT_EQ(empty.bits(), 0U);
}

TEST(StateSet, SingleStateConstructor) {
  const StateSet focused{State::Focused};
  EXPECT_TRUE(focused.any());
  EXPECT_FALSE(focused.none());
  EXPECT_TRUE(focused.test(State::Focused));
  EXPECT_FALSE(focused.test(State::Focusable));
}

TEST(StateSet, SetIsChainableAndAdditive) {
  StateSet states;
  states.set(State::Focusable).set(State::Focused).set(State::Selected);
  EXPECT_TRUE(states.test(State::Focusable));
  EXPECT_TRUE(states.test(State::Focused));
  EXPECT_TRUE(states.test(State::Selected));
  EXPECT_FALSE(states.test(State::Disabled));
}

TEST(StateSet, SetIsIdempotent) {
  StateSet a;
  a.set(State::Checked);
  StateSet b{a};
  b.set(State::Checked);
  EXPECT_EQ(a, b);
}

TEST(StateSet, ClearRemovesOnlyTheGivenState) {
  StateSet states;
  states.set(State::Focusable).set(State::Focused);
  states.clear(State::Focused);
  EXPECT_TRUE(states.test(State::Focusable));
  EXPECT_FALSE(states.test(State::Focused));
}

TEST(StateSet, ClearAbsentStateIsNoOp) {
  StateSet states{State::Focusable};
  states.clear(State::Disabled);
  EXPECT_EQ(states, StateSet{State::Focusable});
}

TEST(StateSet, Equality) {
  StateSet a;
  a.set(State::Focusable).set(State::Focused);
  StateSet b;
  b.set(State::Focused).set(State::Focusable); // order must not matter
  EXPECT_EQ(a, b);
  b.set(State::Disabled);
  EXPECT_NE(a, b);
}

// Tri-state checkbox: off, on, and indeterminate are all distinguishable.
TEST(StateSet, TriStateCheckbox) {
  const StateSet off;
  const StateSet on{State::Checked};
  const StateSet mixed{State::Mixed};

  EXPECT_FALSE(off.test(State::Checked));
  EXPECT_FALSE(off.test(State::Mixed));

  EXPECT_TRUE(on.test(State::Checked));
  EXPECT_FALSE(on.test(State::Mixed));

  EXPECT_FALSE(mixed.test(State::Checked));
  EXPECT_TRUE(mixed.test(State::Mixed));

  EXPECT_NE(off, on);
  EXPECT_NE(on, mixed);
  EXPECT_NE(off, mixed);
}

// Expandable + Expanded: not-expandable, collapsed, and expanded differ.
TEST(StateSet, CollapsedVersusNotExpandable) {
  const StateSet notExpandable;
  StateSet collapsed;
  collapsed.set(State::Expandable);
  StateSet expanded;
  expanded.set(State::Expandable).set(State::Expanded);

  EXPECT_FALSE(notExpandable.test(State::Expandable));

  EXPECT_TRUE(collapsed.test(State::Expandable));
  EXPECT_FALSE(collapsed.test(State::Expanded));

  EXPECT_TRUE(expanded.test(State::Expandable));
  EXPECT_TRUE(expanded.test(State::Expanded));

  EXPECT_NE(notExpandable, collapsed);
  EXPECT_NE(collapsed, expanded);
}

TEST(StateSet, ToStringEmptyIsNone) {
  EXPECT_EQ(toString(StateSet{}), "None");
}

TEST(StateSet, ToStringSingle) {
  EXPECT_EQ(toString(StateSet{State::Focused}), "Focused");
}

TEST(StateSet, ToStringJoinsInAscendingBitOrder) {
  StateSet states;
  // Set out of order; rendering is by ascending bit value, not insertion order.
  states.set(State::ReadOnly).set(State::Focusable).set(State::Checked);
  EXPECT_EQ(toString(states), "Focusable|Checked|ReadOnly");
}

} // namespace
