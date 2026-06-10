// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A small Win32 app with a known control tree, for the UIA provider's
///        integration tests (#40).
///
/// It exposes focusable controls for several mapped roles (button, checkboxes, radio,
/// labelled edits, combobox, list box) with known names and initial states, and cycles focus
/// among them on a timer (via the Win32 tab order) so a UIA client (the integration test) sees a
/// focus-changed event for each. Roles that cannot take keyboard focus (static text,
/// disabled controls, popup menu items) fall outside this focus path and stay unit-tested. It
/// signals a named "ready" event (name in argv[1]) once startup is done — foreground and
/// focus requested for the first control — then pumps messages until terminated. Standard
/// common controls expose UI Automation through the system's default provider, so this
/// app writes no UIA code.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <CommCtrl.h>

namespace {

constexpr const wchar_t* WindowClassName = L"VoxUiaTestAppWindow";
constexpr const wchar_t* WindowTitle = L"Vox UIA Test App";
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

// Creates a STATIC label and a drop-down COMBOBOX on one row (label first, for the name),
// with two items and an initial selection. Returns the combobox (the focusable control).
HWND addLabeledCombo(HWND parent, int top, const wchar_t* label) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  ::CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE, 10, top + 4, 70, 20, parent,
                    nullptr, instance, nullptr);
  HWND combo = ::CreateWindowExW(0, L"COMBOBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 85, top,
                                 220, 160, parent, nullptr, instance, nullptr);
  ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Anna"));
  ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stefan"));
  ::SendMessageW(combo, CB_SETCURSEL, 0, 0); // select "Anna"
  return combo;
}

// Creates a single-select LISTBOX with two items and the first selected. When the listbox
// has keyboard focus the system's focused element is the focused item, so the provider reads
// a ListItem. Returns the listbox.
HWND addListBox(HWND parent, int top) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  HWND list = ::CreateWindowExW(0, L"LISTBOX", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY, 10,
                                top, 295, 50, parent, nullptr, instance, nullptr);
  ::SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Eintrag 1"));
  ::SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Eintrag 2"));
  ::SendMessageW(list, LB_SETCURSEL, 0, 0); // select "Eintrag 1"
  return list;
}

// Builds the known control tree and returns the first control (the initial focus). Each
// button/checkbox/radio's window text is its accessible name; checkboxes/radio get an
// explicit checked/indeterminate state. The edits are labelled (a preceding STATIC), so the
// accessible name is the label and the window text is the value (empty / read-only cases too).
HWND buildControlTree(HWND parent) {
  HWND firstButton = addControl(parent, {.className = L"BUTTON",
                                         .text = L"Speichern",
                                         .style = WS_TABSTOP | BS_PUSHBUTTON,
                                         .top = 10});

  HWND checkedBox = addControl(parent, {.className = L"BUTTON",
                                        .text = L"Kapitel anzeigen",
                                        .style = WS_TABSTOP | BS_AUTOCHECKBOX,
                                        .top = 44});
  ::SendMessageW(checkedBox, BM_SETCHECK, BST_CHECKED, 0);

  HWND triStateBox = addControl(parent, {.className = L"BUTTON",
                                         .text = L"Teilauswahl",
                                         .style = WS_TABSTOP | BS_AUTO3STATE,
                                         .top = 78});
  ::SendMessageW(triStateBox, BM_SETCHECK, BST_INDETERMINATE, 0);

  HWND radio = addControl(parent, {.className = L"BUTTON",
                                   .text = L"Deutsch",
                                   .style = WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON,
                                   .top = 112});
  ::SendMessageW(radio, BM_SETCHECK, BST_CHECKED, 0);

  addLabeledEdit(parent, {.top = 146, .label = L"Name", .text = L"Hallo", .extraStyle = 0});
  addLabeledEdit(parent, {.top = 180, .label = L"Suche", .text = L"", .extraStyle = 0});
  addLabeledEdit(parent,
                 {.top = 214, .label = L"Pfad", .text = L"system32", .extraStyle = ES_READONLY});
  addLabeledCombo(parent, 248, L"Stimme");
  addListBox(parent, 282);
  // A SysLink (comctl32 v6, via the embedded manifest); the link text is its accessible name.
  ::CreateWindowExW(0, L"SysLink", L"<a>Hilfe</a>", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 340,
                    295, 24, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);

  return firstButton;
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
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 360, 440,
                                  nullptr, nullptr, instance, nullptr);
  if (window == nullptr) {
    return 1;
  }

  HWND firstControl = buildControlTree(window);
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
