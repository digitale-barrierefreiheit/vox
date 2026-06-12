// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for the per-language lexicon loader (#61): resolution order,
///        validation (declared language + completeness), and the always-speaks
///        fallback to the embedded German default. Uses real files in a
///        per-test temp directory — the loader is the app's filesystem seam.
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vox/app/lexicon_loader.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/model/role.hpp>

namespace {

using vox::app::isLanguageTag;
using vox::app::LexiconOrigin;
using vox::app::LoadedLexicon;
using vox::app::loadLexicon;
using vox::model::Role;

using ::testing::HasSubstr;

/// A complete table (every required key) declaring @p language. Tests that need
/// a distinctive word append an override line — later keys win in the parser.
std::string completeTable(std::string_view language) {
  std::string text = "language = " + std::string(language) + "\n";
  for (const std::string_view key :
       {"role.button", "role.checkbox", "role.radiobutton", "role.edit", "role.combobox",
        "role.listitem", "role.menuitem", "role.link", "role.statictext", "state.checked",
        "state.unchecked", "state.mixed", "state.expanded", "state.collapsed", "state.selected",
        "state.disabled", "state.readonly", "state.emptyvalue"}) {
    text += std::string(key) + " = wort\n";
  }
  return text;
}

std::string completeTableWithButton(std::string_view language, std::string_view buttonWord) {
  return completeTable(language) + "role.button = " + std::string(buttonWord) + "\n";
}

class LexiconLoaderTest : public ::testing::Test {
protected:
  [[nodiscard]] const std::filesystem::path& dir() const {
    return dir_;
  }

  std::filesystem::path writeFile(const std::filesystem::path& name,
                                  std::string_view content) const {
    const std::filesystem::path file = dir_ / name;
    std::ofstream stream(file, std::ios::binary);
    stream << content;
    return file;
  }

private:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = std::filesystem::path(::testing::TempDir()) /
           (std::string("vox-lexicon-loader-") + info->name());
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(dir_);
  }

  std::filesystem::path dir_;
};

TEST_F(LexiconLoaderTest, LoadsTheGermanFileWhenNoLanguageIsRequested) {
  writeFile("de.lex", completeTableWithButton("de", "Knopf-aus-Datei"));

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir()});

  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_EQ(loaded.path, dir() / "de.lex");
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "Knopf-aus-Datei");
  EXPECT_TRUE(loaded.diagnostics.empty());
}

TEST_F(LexiconLoaderTest, LoadsTheRequestedLanguageFile) {
  writeFile("de.lex", completeTableWithButton("de", "Schaltfläche"));
  writeFile("en.lex", completeTableWithButton("en", "button-from-file"));

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir(), .requestedTag = "en"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "button-from-file");
  EXPECT_TRUE(loaded.diagnostics.empty());
}

TEST_F(LexiconLoaderTest, TheLanguageMatchIsAsciiCaseInsensitive) {
  writeFile("en.lex", completeTable("EN"));

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir(), .requestedTag = "en"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_TRUE(loaded.diagnostics.empty());
}

TEST_F(LexiconLoaderTest, AnAbsentFileFallsBackToTheEmbeddedGermanDefault) {
  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir(), .requestedTag = "en"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("could not be read"));
  // The fallback always speaks: the embedded default is complete German.
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "Schaltfläche");
  EXPECT_TRUE(loaded.lexicon.missingRequiredKeys().empty());
}

TEST_F(LexiconLoaderTest, AFileWithoutALanguageDeclarationIsRejected) {
  writeFile("de.lex", "role.button = Knopf\n");

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir()});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("declares no language"));
}

TEST_F(LexiconLoaderTest, AMismatchedLanguageDeclarationIsRejected) {
  // en content placed in de.lex (e.g. a copy-rename mistake) must not be
  // spoken as German.
  writeFile("de.lex", completeTable("en"));

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir(), .requestedTag = "de"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("\"de\" was expected"));
}

TEST_F(LexiconLoaderTest, AnIncompleteTableIsRejected) {
  writeFile("de.lex", "language = de\nrole.button = Knopf\n");

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir()});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("missing required keys"));
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("state.checked"));
}

TEST_F(LexiconLoaderTest, AnExplicitFileWinsAndItsDeclaredLanguageStands) {
  // VOX_LEXICON without VOX_LANGUAGE: the file says what it stands for.
  writeFile("de.lex", completeTable("de"));
  const std::filesystem::path custom =
      writeFile("custom.lex", completeTableWithButton("en", "my-button"));

  const LoadedLexicon loaded = loadLexicon({.explicitFile = custom, .lexiconDir = dir()});

  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_EQ(loaded.path, custom);
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "my-button");
  EXPECT_TRUE(loaded.diagnostics.empty());
}

TEST_F(LexiconLoaderTest, AnExplicitFileMustMatchAnExplicitlyRequestedLanguage) {
  const std::filesystem::path custom = writeFile("custom.lex", completeTable("en"));

  const LoadedLexicon loaded =
      loadLexicon({.explicitFile = custom, .lexiconDir = dir(), .requestedTag = "de"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("VOX_LEXICON"));
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("\"de\" was expected"));
}

TEST_F(LexiconLoaderTest, ABrokenExplicitFileFallsBackToTheDefaultNotTheDirectory) {
  // The user replaced the lookup with VOX_LEXICON; a broken value must not
  // silently re-enable what they replaced.
  writeFile("de.lex", completeTableWithButton("de", "Knopf-aus-Datei"));

  const LoadedLexicon loaded =
      loadLexicon({.explicitFile = dir() / "missing.lex", .lexiconDir = dir()});

  EXPECT_EQ(loaded.origin, LexiconOrigin::EmbeddedDefault);
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "Schaltfläche");
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("VOX_LEXICON"));
}

TEST_F(LexiconLoaderTest, AnInvalidRequestedTagIsReportedAndIgnored) {
  // "../evil" would otherwise become a path; the tag grammar forbids it.
  writeFile("de.lex", completeTableWithButton("de", "Knopf-aus-Datei"));

  const LoadedLexicon loaded = loadLexicon({.lexiconDir = dir(), .requestedTag = "../evil"});

  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "Knopf-aus-Datei");
  ASSERT_EQ(loaded.diagnostics.size(), 1U);
  EXPECT_THAT(loaded.diagnostics.front(), HasSubstr("not a language tag"));
}

TEST(IsLanguageTag, AcceptsBcp47ShapedTags) {
  EXPECT_TRUE(isLanguageTag("de"));
  EXPECT_TRUE(isLanguageTag("en-US"));
  EXPECT_TRUE(isLanguageTag("de-AT-1996"));
}

TEST(IsLanguageTag, RejectsEverythingThatCouldEscapeAFileName) {
  EXPECT_FALSE(isLanguageTag(""));
  EXPECT_FALSE(isLanguageTag(".."));
  EXPECT_FALSE(isLanguageTag("de/at"));
  EXPECT_FALSE(isLanguageTag("de\\at"));
  EXPECT_FALSE(isLanguageTag("de.at"));
  EXPECT_FALSE(isLanguageTag("de at"));
  EXPECT_FALSE(isLanguageTag("dé"));
}

// The files that actually ship next to the executable must pass the loader's
// own validation — for both supported languages (#61).
#ifdef VOX_LEXICON_DATA_DIR
TEST(ShippedLexicons, GermanFileLoadsThroughTheLoader) {
  const LoadedLexicon loaded =
      loadLexicon({.lexiconDir = VOX_LEXICON_DATA_DIR, .requestedTag = "de"});
  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_TRUE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "Schaltfläche");
}

TEST(ShippedLexicons, EnglishFileLoadsThroughTheLoader) {
  const LoadedLexicon loaded =
      loadLexicon({.lexiconDir = VOX_LEXICON_DATA_DIR, .requestedTag = "en"});
  EXPECT_EQ(loaded.origin, LexiconOrigin::File);
  EXPECT_TRUE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.lexicon.role(Role::Button), "button");
  EXPECT_TRUE(loaded.lexicon.missingRequiredKeys().empty());
}
#endif // VOX_LEXICON_DATA_DIR

} // namespace
