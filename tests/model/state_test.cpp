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

// Tri-state checkbox: off, on, and indeterminate are all distinguishable. The
// three configurations are shared by the membership and distinctness tests.
class TriStateCheckbox : public ::testing::Test {
protected:
  const StateSet off_;
  const StateSet on_{Checked};
  const StateSet mixed_{Mixed};
};

TEST_F(TriStateCheckbox, OffHasNeitherCheckedNorMixed) {
  EXPECT_FALSE(off_.test(Checked));
  EXPECT_FALSE(off_.test(Mixed));
}

TEST_F(TriStateCheckbox, OnHasCheckedButNotMixed) {
  EXPECT_TRUE(on_.test(Checked));
  EXPECT_FALSE(on_.test(Mixed));
}

TEST_F(TriStateCheckbox, IndeterminateHasMixedButNotChecked) {
  EXPECT_FALSE(mixed_.test(Checked));
  EXPECT_TRUE(mixed_.test(Mixed));
}

TEST_F(TriStateCheckbox, AllThreeConfigurationsDiffer) {
  EXPECT_NE(off_, on_);
  EXPECT_NE(on_, mixed_);
  EXPECT_NE(off_, mixed_);
}

// Expandable + Expanded: not-expandable, collapsed, and expanded differ. The
// three configurations are shared by the membership and distinctness tests.
class ExpandableStates : public ::testing::Test {
protected:
  static StateSet collapsed() {
    StateSet states;
    states.set(Expandable);
    return states;
  }

  static StateSet expanded() {
    StateSet states;
    states.set(Expandable).set(Expanded);
    return states;
  }

  const StateSet notExpandable_;
  const StateSet collapsed_{collapsed()};
  const StateSet expanded_{expanded()};
};

TEST_F(ExpandableStates, NotExpandableHasNoExpandable) {
  EXPECT_FALSE(notExpandable_.test(Expandable));
}

TEST_F(ExpandableStates, CollapsedIsExpandableButNotExpanded) {
  EXPECT_TRUE(collapsed_.test(Expandable));
  EXPECT_FALSE(collapsed_.test(Expanded));
}

TEST_F(ExpandableStates, ExpandedIsBothExpandableAndExpanded) {
  EXPECT_TRUE(expanded_.test(Expandable));
  EXPECT_TRUE(expanded_.test(Expanded));
}

TEST_F(ExpandableStates, AdjacentConfigurationsDiffer) {
  EXPECT_NE(notExpandable_, collapsed_);
  EXPECT_NE(collapsed_, expanded_);
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
