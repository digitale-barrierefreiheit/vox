// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Single source of truth for the UIA test app's control tree (#40).
///
/// Both the test app (which builds the Win32 controls) and the provider integration test
/// (which asserts the read AccessibleNode + the German utterance) consume this one list, so
/// the documented tree cannot drift from what is built or asserted. `name`/`value` are UTF-8
/// (ASCII here); the app widens them for Win32, the test compares them directly.
#ifndef VOX_TESTS_UIA_TEST_APP_CONTROL_TREE_HPP
#define VOX_TESTS_UIA_TEST_APP_CONTROL_TREE_HPP

#include <array>
#include <optional>
#include <string_view>

#include <vox/model/role.hpp>
#include <vox/model/state.hpp>

namespace vox::testapp {

/// What Win32 control the app creates for a row (drives className/style + initial state).
enum class Kind {
  Button,
  CheckedCheckbox,
  TriStateCheckbox,
  Radio,
  Edit,
  ReadOnlyEdit,
  Combobox,
  ListBox,
  Link,
};

/// One control: how the app builds it (`kind`, `name`, `value`) and what the integration
/// test expects the provider to read (`role`, `state`, `value`) and announce (`utterance`).
struct ControlSpec {
  Kind kind;
  std::string_view
      name; ///< Accessible name (button/checkbox text, edit/combo label, list/link text).
  std::string_view value; ///< Value text (edit content, combo selection); empty if none.
  vox::model::Role role;  ///< Expected mapped role.
  std::optional<vox::model::State> state; ///< Expected headline state, if any.
  std::string_view utterance;             ///< Expected German announce() text.
};

using vox::model::Role;
using enum vox::model::State;

/// The known control tree. Order is the focus/tab order the app lays out top to bottom.
inline constexpr std::array<ControlSpec, 10> ControlTree{{
    {Kind::Button, "Speichern", "", Role::Button, std::nullopt, "Schaltfläche, Speichern"},
    {Kind::CheckedCheckbox, "Kapitel anzeigen", "", Role::Checkbox, Checked,
     "Kontrollkästchen, Kapitel anzeigen, aktiviert"},
    {Kind::TriStateCheckbox, "Teilauswahl", "", Role::Checkbox, Mixed,
     "Kontrollkästchen, Teilauswahl, teilweise aktiviert"},
    {Kind::Radio, "Deutsch", "", Role::RadioButton, Checked, "Optionsfeld, Deutsch, aktiviert"},
    {Kind::Edit, "Name", "Hallo", Role::Edit, std::nullopt, "Eingabefeld, Name, Hallo"},
    {Kind::Edit, "Suche", "", Role::Edit, std::nullopt, "Eingabefeld, Suche, leer"},
    {Kind::ReadOnlyEdit, "Pfad", "system32", Role::Edit, ReadOnly,
     "Eingabefeld, Pfad, schreibgeschützt, system32"},
    {Kind::Combobox, "Stimme", "Anna", Role::Combobox, Expandable,
     "Kombinationsfeld, Stimme, reduziert, Anna"},
    {Kind::ListBox, "Eintrag 1", "", Role::ListItem, Selected,
     "Listenelement, Eintrag 1, ausgewählt"},
    {Kind::Link, "Hilfe", "", Role::Link, std::nullopt, "Link, Hilfe"},
}};

/// How the app creates a non-focusable control (a plain static label / a menu-bar item).
enum class NonFocusableKind { StaticLabel, MenuBar };

/// A non-focusable control: focus cycling cannot reach it, so the integration test reads it
/// by name through UiaProvider::nodeByName instead. These cover the remaining mapped roles.
struct NonFocusableControl {
  NonFocusableKind kind;
  std::string_view name;      ///< Accessible name (the static's text / the menu's title).
  vox::model::Role role;      ///< Expected mapped role.
  std::string_view utterance; ///< Expected German announce() text.
};

/// The non-focusable controls: a static text and a menu-bar item (StaticText + MenuItem roles).
inline constexpr std::array<NonFocusableControl, 2> NonFocusableTree{{
    {NonFocusableKind::StaticLabel, "Hinweis", Role::StaticText, "Text, Hinweis"},
    {NonFocusableKind::MenuBar, "Datei", Role::MenuItem, "Menüpunkt, Datei"},
}};

/// The test app's window class + title — shared so the integration test can find the window
/// (FindWindow) to pass its handle to nodeByName for the non-focusable reads.
inline constexpr const wchar_t* WindowClassName = L"VoxUiaTestAppWindow";
inline constexpr const wchar_t* WindowTitle = L"Vox UIA Test App";

} // namespace vox::testapp

#endif // VOX_TESTS_UIA_TEST_APP_CONTROL_TREE_HPP
