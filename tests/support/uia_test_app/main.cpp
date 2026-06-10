// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A small Win32 app with a known control tree, for the UIA provider's
///        integration tests (#40).
///
/// It exposes focusable controls for several mapped roles (button, checkboxes, radio,
/// labelled edits, combobox, list box) with known names and initial states, and cycles focus
/// among them on a timer (via the Win32 tab order) so a UIA client (the integration test) sees a
/// focus-changed event for each. It also exposes the non-focusable roles — a static text and a
/// menu-bar item — which the integration test reads by name (UiaProvider::nodeByName) rather than
/// via focus. It signals a named "ready" event (name in argv[1]) once startup is done — foreground
/// and
/// focus requested for the first control — then pumps messages until terminated. Standard
/// common controls expose UI Automation through the system's default provider, so this
/// app writes no UIA code.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <CommCtrl.h>
#include <cstddef>
#include <string>
#include <string_view>
#include <uia_test_app/control_tree.hpp>

namespace {

using vox::testapp::WindowClassName;
using vox::testapp::WindowTitle;
constexpr int EventNameBufferChars = 256;
constexpr UINT_PTR FocusCycleTimerId = 1;
constexpr UINT FocusCycleMs = 200;

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_TIMER && wParam == FocusCycleTimerId) {
    // Advance focus to the next tab-stop child, wrapping at the end. GetNextDlgTabItem walks
    // any window's WS_TABSTOP children (not only real dialogs); the window carries
    // WS_EX_CONTROLPARENT to make that traversal explicit. Anchor on the control we last
    // focused (a static, seeded with the first child) rather than GetFocus(): GetFocus() is
    // per-thread and can be null when the window is not foreground, which would stall the
    // cycle. The window is brought foreground once at startup; we do not re-steal it per tick.
    static HWND anchor = nullptr;
    if (anchor == nullptr) {
      anchor = ::GetWindow(window, GW_CHILD);
    }
    if (HWND next = ::GetNextDlgTabItem(window, anchor, FALSE)) {
      ::SetFocus(next);
      anchor = next;
    }
    return 0;
  }
  if (message == WM_DESTROY) {
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(window, message, wParam, lParam);
}

struct ControlSpec {
  const wchar_t* className;
  const wchar_t* text;
  DWORD style;
  int top;
};

HWND addControl(HWND parent, const ControlSpec& spec) {
  return ::CreateWindowExW(0, spec.className, spec.text, WS_CHILD | WS_VISIBLE | spec.style, 10,
                           spec.top, 260, 28, parent, nullptr, ::GetModuleHandleW(nullptr),
                           nullptr);
}

// Adds a BUTTON-class control (checkbox/radio) and applies its initial check state.
HWND addCheckable(HWND parent, const ControlSpec& spec, WPARAM check) {
  HWND box = addControl(parent, spec);
  ::SendMessageW(box, BM_SETCHECK, check, 0);
  return box;
}

struct LabeledEditSpec {
  int top;
  const wchar_t* label;
  const wchar_t* text;
  DWORD extraStyle;
};

// Creates a STATIC label and an EDIT on one row, the label first so the MSAA/UIA bridge
// derives the edit's accessible name from the label text (a preceding-static label). The
// edit's window text is its value. Returns the edit (the focusable control the test reads).
HWND addLabeledEdit(HWND parent, const LabeledEditSpec& spec) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  ::CreateWindowExW(0, L"STATIC", spec.label, WS_CHILD | WS_VISIBLE, 10, spec.top + 4, 70, 20,
                    parent, nullptr, instance, nullptr);
  return ::CreateWindowExW(0, L"EDIT", spec.text,
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL |
                               spec.extraStyle,
                           85, spec.top, 220, 24, parent, nullptr, instance, nullptr);
}

struct LabeledComboSpec {
  int top;
  const wchar_t* label;
  const wchar_t* selection;
};

// Creates a STATIC label and a drop-down COMBOBOX on one row (label first, for the name),
// with the selection as its (selected) item. Returns the combobox (the focusable control).
HWND addLabeledCombo(HWND parent, const LabeledComboSpec& spec) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  ::CreateWindowExW(0, L"STATIC", spec.label, WS_CHILD | WS_VISIBLE, 10, spec.top + 4, 70, 20,
                    parent, nullptr, instance, nullptr);
  HWND combo = ::CreateWindowExW(0, L"COMBOBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 85,
                                 spec.top, 220, 160, parent, nullptr, instance, nullptr);
  ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(spec.selection));
  ::SendMessageW(combo, CB_SETCURSEL, 0, 0);
  return combo;
}

// Creates a single-select LISTBOX with @p selected as its selected item. When the listbox has
// keyboard focus the system's focused element is that item, so the provider reads a ListItem.
HWND addListBox(HWND parent, int top, const wchar_t* selected) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  HWND list = ::CreateWindowExW(0, L"LISTBOX", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY, 10,
                                top, 295, 50, parent, nullptr, instance, nullptr);
  ::SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(selected));
  ::SendMessageW(list, LB_SETCURSEL, 0, 0);
  return list;
}

// Creates a SysLink (comctl32 v6, via the manifest dependency); the link text is its name.
HWND addLink(HWND parent, int top, const std::wstring& name) {
  const std::wstring markup = L"<a>" + name + L"</a>";
  return ::CreateWindowExW(0, L"SysLink", markup.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10,
                           top, 295, 24, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
}

// Widens a UTF-8 string (the tree's names/values are ASCII) for the Win32 *W APIs.
std::wstring widen(std::string_view utf8) {
  const int count =
      ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), count);
  return wide;
}

// The button-family kinds: a BUTTON-class control with an optional initial check state.
// Returns nullptr for any other kind (so buildControl falls through to its own cases).
HWND buildButtonKind(HWND parent, vox::testapp::Kind kind, const wchar_t* name, int top) {
  using enum vox::testapp::Kind;
  switch (kind) {
  case Button:
    return addControl(
        parent,
        {.className = L"BUTTON", .text = name, .style = WS_TABSTOP | BS_PUSHBUTTON, .top = top});
  case CheckedCheckbox:
    return addCheckable(
        parent,
        {.className = L"BUTTON", .text = name, .style = WS_TABSTOP | BS_AUTOCHECKBOX, .top = top},
        BST_CHECKED);
  case TriStateCheckbox:
    return addCheckable(
        parent,
        {.className = L"BUTTON", .text = name, .style = WS_TABSTOP | BS_AUTO3STATE, .top = top},
        BST_INDETERMINATE);
  case Radio:
    return addCheckable(parent,
                        {.className = L"BUTTON",
                         .text = name,
                         .style = WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON,
                         .top = top},
                        BST_CHECKED);
  default:
    return nullptr;
  }
}

// Builds one control from a shared ControlSpec at @p top; returns the focusable control.
HWND buildControl(HWND parent, const vox::testapp::ControlSpec& spec, int top) {
  using enum vox::testapp::Kind;
  const std::wstring name = widen(spec.name);
  const std::wstring value = widen(spec.value);
  if (HWND button = buildButtonKind(parent, spec.kind, name.c_str(), top)) {
    return button;
  }
  switch (spec.kind) {
  case Edit:
    return addLabeledEdit(
        parent, {.top = top, .label = name.c_str(), .text = value.c_str(), .extraStyle = 0});
  case ReadOnlyEdit:
    return addLabeledEdit(
        parent,
        {.top = top, .label = name.c_str(), .text = value.c_str(), .extraStyle = ES_READONLY});
  case Combobox:
    return addLabeledCombo(parent, {.top = top, .label = name.c_str(), .selection = value.c_str()});
  case ListBox:
    return addListBox(parent, top, name.c_str());
  case Link:
    return addLink(parent, top, name);
  default:
    return nullptr; // button-family kinds are handled by buildButtonKind above
  }
}

constexpr int ControlTreeTop = 10; // y of the first control row

// Row height for a control kind (the list box is taller than the single-line controls).
int rowHeight(vox::testapp::Kind kind) {
  return (kind == vox::testapp::Kind::ListBox) ? 58 : 34;
}

// The y just below the focusable control tree — where the non-focusable static label sits.
// Derived from ControlTree's layout so it never drifts when the tree changes.
int controlTreeBottom() {
  int top = ControlTreeTop;
  for (const vox::testapp::ControlSpec& spec : vox::testapp::ControlTree) {
    top += rowHeight(spec.kind);
  }
  return top;
}

// Builds the known control tree from the single shared source and returns the first control
// (the initial focus).
HWND buildControlTree(HWND parent) {
  HWND first = nullptr;
  int top = ControlTreeTop;
  for (const vox::testapp::ControlSpec& spec : vox::testapp::ControlTree) {
    HWND control = buildControl(parent, spec, top);
    if (first == nullptr) {
      first = control;
    }
    top += rowHeight(spec.kind);
  }
  return first;
}

// Adds the non-focusable controls (a static label + a menu-bar item) from the shared source.
// Focus cycling cannot reach these, so the integration test reads them by name; they cover the
// StaticText and MenuItem roles.
void buildNonFocusable(HWND parent) {
  for (const vox::testapp::NonFocusableControl& spec : vox::testapp::NonFocusableTree) {
    const std::wstring name = widen(spec.name);
    switch (spec.kind) {
    case vox::testapp::NonFocusableKind::StaticLabel:
      ::CreateWindowExW(0, L"STATIC", name.c_str(), WS_CHILD | WS_VISIBLE, 10, controlTreeBottom(),
                        295, 20, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
      break;
    case vox::testapp::NonFocusableKind::MenuBar: {
      HMENU bar = ::CreateMenu();
      HMENU popup = ::CreatePopupMenu();
      ::AppendMenuW(popup, MF_STRING, 1, L"Neu");
      ::AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), name.c_str());
      ::SetMenu(parent, bar);
      ::DrawMenuBar(parent); // force a non-client repaint so UIA sees the menu item immediately
      break;
    }
    }
  }
}

// Signals the launcher's ready event once startup is done — the window is created and
// foreground + focus have been requested (whether focus actually lands is what the
// launcher polls for, since SetForegroundWindow/SetFocus can be refused). The event
// name arrives as a narrow argv (the CRT decoded it from the wide command line via the
// active code page), so decode it back with CP_ACP and open the Unicode-named event the
// launcher created with CreateEventW. An un-openable event is not fatal.
void signalReady(const char* eventName) {
  std::array<wchar_t, EventNameBufferChars> wide{};
  if (const int written =
          ::MultiByteToWideChar(CP_ACP, 0, eventName, -1, wide.data(), EventNameBufferChars);
      written <= 0) {
    return; // conversion failed or the name did not fit the buffer
  }
  if (HANDLE ready = ::OpenEventW(EVENT_MODIFY_STATE, FALSE, wide.data())) {
    ::SetEvent(ready);
    ::CloseHandle(ready);
  }
}

} // namespace

int main(int argc, char** argv) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);

  const INITCOMMONCONTROLSEX icc{.dwSize = sizeof(INITCOMMONCONTROLSEX), .dwICC = ICC_LINK_CLASS};
  ::InitCommonControlsEx(&icc); // register the SysLink (link) control class

  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = &windowProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = WindowClassName;
  if (::RegisterClassExW(&windowClass) == 0) {
    return 1;
  }

  HWND window = ::CreateWindowExW(WS_EX_CONTROLPARENT, WindowClassName, WindowTitle,
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 360, 480,
                                  nullptr, nullptr, instance, nullptr);
  if (window == nullptr) {
    return 1;
  }

  HWND firstControl = nullptr;
  try {
    firstControl = buildControlTree(window);
    buildNonFocusable(window);
  } catch (...) {
    return 1; // a control-name conversion (widen allocation) failed — exit rather than escape
  }
  if (firstControl == nullptr) {
    return 1;
  }

  ::ShowWindow(window, SW_SHOWNORMAL);
  ::SetForegroundWindow(window);
  ::SetFocus(firstControl);
  ::SetTimer(window, FocusCycleTimerId, FocusCycleMs, nullptr);

  if (argc > 1) {
    signalReady(argv[1]);
  }

  MSG message{};
  while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&message);
    ::DispatchMessageW(&message);
  }
  return 0;
}
