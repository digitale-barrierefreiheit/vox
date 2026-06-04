// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Golden tests for vox::output::OutputManager against the shipped German.
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/output/utterance.hpp>

namespace {

using vox::german::DefaultGermanLexiconData;
using vox::german::Lexicon;
using vox::model::AccessibleNode;
using vox::model::Role;
using vox::model::State;
using vox::model::StateSet;
using vox::output::OutputManager;
using vox::output::Priority;
using vox::output::Source;

// A manager backed by the real shipped German table, so these are end-to-end
// golden assertions on actual announcements.
const OutputManager& shippedManager() {
  static const OutputManager manager(Lexicon::parse(DefaultGermanLexiconData));
  return manager;
}

// Builds a node with all fields supplied (keeps the aggregate init warning-clean
// without leaning on partial designated initializers).
AccessibleNode makeNode(Role role, std::string_view name, StateSet states = {},
                        std::optional<std::string> value = {}) {
  return {role, std::string(name), states, std::move(value)};
}

TEST(OutputManager, FocusedButton) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Button, "OK")).text, "Schaltfläche, OK");
}

TEST(OutputManager, CheckedCheckbox) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::Checkbox, "Newsletter", StateSet{State::Checked}))
                .text,
            "Kontrollkästchen, Newsletter, aktiviert");
}

TEST(OutputManager, UncheckedCheckboxAnnouncesNegativeState) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Checkbox, "Newsletter")).text,
            "Kontrollkästchen, Newsletter, nicht aktiviert");
}

TEST(OutputManager, IndeterminateCheckbox) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::Checkbox, "Newsletter", StateSet{State::Mixed}))
                .text,
            "Kontrollkästchen, Newsletter, teilweise aktiviert");
}

TEST(OutputManager, RadioButtonUsesToggleVocabulary) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::RadioButton, "Sofort", StateSet{State::Checked}))
                .text,
            "Optionsfeld, Sofort, aktiviert");
}

TEST(OutputManager, ExpandedVersusCollapsed) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::Combobox, "Land",
                                   StateSet{}.set(State::Expandable).set(State::Expanded)))
                .text,
            "Kombinationsfeld, Land, erweitert");
  EXPECT_EQ(
      shippedManager().announce(makeNode(Role::Combobox, "Land", StateSet{State::Expandable})).text,
      "Kombinationsfeld, Land, reduziert");
}

TEST(OutputManager, EmptyEditSpeaksLeer) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Edit, "Suche", {}, "")).text,
            "Eingabefeld, Suche, leer");
}

TEST(OutputManager, EditWithContentSpeaksTheValue) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Edit, "Benutzername", {}, "alice")).text,
            "Eingabefeld, Benutzername, alice");
}

// Order is role, name, states, value: read-only (state) precedes the content.
TEST(OutputManager, ReadOnlyEditOrdersStateBeforeValue) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::Edit, "Benutzername", StateSet{State::ReadOnly}, "geheim"))
                .text,
            "Eingabefeld, Benutzername, schreibgeschützt, geheim");
}

TEST(OutputManager, ButtonHasNoValueSpoken) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Button, "OK")).text, "Schaltfläche, OK");
}

TEST(OutputManager, SelectedAndDisabledWordsAppearInOrder) {
  EXPECT_EQ(shippedManager()
                .announce(makeNode(Role::ListItem, "Datei",
                                   StateSet{}.set(State::Selected).set(State::Disabled)))
                .text,
            "Listenelement, Datei, ausgewählt, nicht verfügbar");
}

TEST(OutputManager, NameIsNormalized) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Button, "  OK   Los ")).text,
            "Schaltfläche, OK Los");
}

TEST(OutputManager, UnknownRoleWordIsSkipped) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Unknown, "Etwas")).text, "Etwas");
}

TEST(OutputManager, WhitespaceOnlyValueIsTreatedAsEmpty) {
  EXPECT_EQ(shippedManager().announce(makeNode(Role::Edit, "Suche", {}, "   ")).text,
            "Eingabefeld, Suche, leer");
}

TEST(OutputManager, UtteranceMetadataDefaults) {
  const auto utterance = shippedManager().announce(makeNode(Role::Button, "OK"));
  EXPECT_EQ(utterance.priority, Priority::Normal);
  EXPECT_EQ(utterance.source, Source::FocusChange);
}

} // namespace
