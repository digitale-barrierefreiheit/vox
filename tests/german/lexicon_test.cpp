// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::german::Lexicon and the shipped de.lex table.
#include <gtest/gtest.h>

#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/model/role.hpp>

namespace {

using vox::german::DefaultGermanLexiconData;
using vox::german::Lexicon;
using vox::german::StateConcept;
using vox::model::Role;

TEST(Lexicon, ParsesSimpleEntries) {
  const Lexicon lex = Lexicon::parse("role.button = Schaltfläche\n");
  EXPECT_EQ(lex.role(Role::Button), "Schaltfläche");
}

TEST(Lexicon, IgnoresCommentsBlankLinesAndMalformedLines) {
  const Lexicon lex = Lexicon::parse("# a comment\n"
                                     "\n"
                                     "   # indented comment\n"
                                     "this line has no equals sign\n"
                                     "role.button = Schaltfläche\n");
  EXPECT_EQ(lex.role(Role::Button), "Schaltfläche");
  EXPECT_TRUE(lex.role(Role::Checkbox).empty());
}

TEST(Lexicon, IgnoresLinesWithAnEmptyKey) {
  // A line whose key is empty before the '=' (here just "= x") is skipped, not
  // stored under an empty key.
  const Lexicon lex = Lexicon::parse("= orphaned value\nrole.button = Schaltfläche\n");
  EXPECT_EQ(lex.role(Role::Button), "Schaltfläche");
}

TEST(Lexicon, TrimsWhitespaceAndStripsCarriageReturns) {
  const Lexicon lex = Lexicon::parse("  role.edit   =   Eingabefeld  \r\n");
  EXPECT_EQ(lex.role(Role::Edit), "Eingabefeld");
}

TEST(Lexicon, LaterKeysOverrideEarlier) {
  const Lexicon lex = Lexicon::parse("role.link = Erst\nrole.link = Zweit\n");
  EXPECT_EQ(lex.role(Role::Link), "Zweit");
}

TEST(Lexicon, UnknownRoleAndMissingLookupsAreEmpty) {
  const Lexicon lex = Lexicon::parse("role.button = Schaltfläche\n");
  EXPECT_TRUE(lex.role(Role::Unknown).empty());
  EXPECT_TRUE(lex.role(Role::Combobox).empty());
  EXPECT_TRUE(lex.state(StateConcept::Checked).empty());
}

// The out-of-table-range guards in Lexicon::role()/state() are defensive against
// an out-of-range Role/StateConcept (an upstream bug). They are unreachable with
// any valid enumerator (the tables span them), and forging an out-of-range value
// to exercise them trips clang-tidy's optin.core.EnumCastOutOfRange — so those two
// returns are excluded from coverage in lexicon.cpp rather than tested by
// violating the project's own lint.

// The contract that an unknown role is never spoken is enforced in code, not
// merely by de.lex omitting the key: a table that defines it changes nothing.
TEST(Lexicon, UnknownRoleStaysEmptyEvenIfTableDefinesIt) {
  const Lexicon lex = Lexicon::parse("role.unknown = Unbekannt\n");
  EXPECT_TRUE(lex.role(Role::Unknown).empty());
}

TEST(Lexicon, ExposesTheDeclaredLanguage) {
  const Lexicon lex = Lexicon::parse("language = de\nrole.button = Schaltfläche\n");
  EXPECT_EQ(lex.language(), "de");
}

TEST(Lexicon, LanguageIsEmptyWhenNotDeclared) {
  EXPECT_TRUE(Lexicon::parse("role.button = Schaltfläche\n").language().empty());
}

TEST(Lexicon, LanguageIsTrimmedAndLaterDeclarationsOverride) {
  const Lexicon lex = Lexicon::parse("language =  fr \nlanguage = de-AT\n");
  EXPECT_EQ(lex.language(), "de-AT");
}

// The language declaration is metadata, not a word: it must never count as a
// missing announcement key.
TEST(Lexicon, LanguageIsNotARequiredKey) {
  const Lexicon lex = Lexicon::parse("");
  EXPECT_TRUE(lex.language().empty());
  for (const auto& key : lex.missingRequiredKeys()) {
    EXPECT_NE(key, "language");
  }
}

TEST(Lexicon, MissingRequiredKeysCountsGaps) {
  // 9 roles (excluding the intentionally-empty Unknown) + 9 state concepts.
  EXPECT_EQ(Lexicon::parse("").missingRequiredKeys().size(), 18U);
  EXPECT_EQ(Lexicon::parse("role.button = Schaltfläche").missingRequiredKeys().size(), 17U);
}

// The table that actually ships must define every word the MVP announces, and
// must declare the language it stands for (#61).
TEST(Lexicon, ShippedGermanTableIsCompleteAndDeclaresGerman) {
  const Lexicon lex = Lexicon::parse(DefaultGermanLexiconData);
  EXPECT_TRUE(lex.missingRequiredKeys().empty());
  EXPECT_EQ(lex.language(), "de");
}

TEST(Lexicon, ShippedGermanWords) {
  const Lexicon lex = Lexicon::parse(DefaultGermanLexiconData);
  EXPECT_EQ(lex.role(Role::Button), "Schaltfläche");
  EXPECT_EQ(lex.role(Role::Checkbox), "Kontrollkästchen");
  EXPECT_EQ(lex.role(Role::RadioButton), "Optionsfeld");
  EXPECT_TRUE(lex.role(Role::Unknown).empty());
  EXPECT_EQ(lex.state(StateConcept::Checked), "aktiviert");
  EXPECT_EQ(lex.state(StateConcept::Unchecked), "nicht aktiviert");
  EXPECT_EQ(lex.state(StateConcept::Mixed), "teilweise aktiviert");
  EXPECT_EQ(lex.state(StateConcept::Collapsed), "reduziert");
  EXPECT_EQ(lex.state(StateConcept::EmptyValue), "leer");
}

} // namespace
