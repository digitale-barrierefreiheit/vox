// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A small Win32 app with a known control tree, for the UIA provider's
///        integration tests (#40).
///
/// It exposes one control per provider-mappable, *focusable* role with known names
/// and initial states, and cycles keyboard focus through them on a timer (via the
/// Win32 tab order) so a UIA client (the integration test) sees a focus-changed
/// event for each. Non-focusable roles (static text, a disabled control) and popup
/// menu items can't be reached through the focus path and stay unit-tested. It
/// signals a named "ready" event (name in argv[1]) once the first control is
/// focused, then pumps messages until terminated. Standard common controls expose
/// UI Automation through the system's default provider, so this app writes no UIA code.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>

namespace {

constexpr const wchar_t* WindowClassName = L"VoxUiaTestAppWindow";
constexpr const wchar_t* WindowTitle = L"Vox UIA Test App";
constexpr int EventNameBufferChars = 256;
constexpr UINT_PTR FocusCycleTimerId = 1;
constexpr UINT FocusCycleMs = 200;

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_TIMER) {
    // Advance focus to the next tab-stop control, wrapping at the end.
    if (HWND next = ::GetNextDlgTabItem(window, ::GetFocus(), FALSE)) {
      ::SetForegroundWindow(window);
      ::SetFocus(next);
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

// Builds the known focusable control tree and returns the first control (the initial
// focus). Each control's window text is its accessible name; checkboxes/radio get an
// explicit checked/indeterminate state; the edit's text is its value (it has no name).
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

  addControl(parent, {.className = L"EDIT",
                      .text = L"Hallo",
                      .style = WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                      .top = 146});

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
  const int written =
      ::MultiByteToWideChar(CP_ACP, 0, eventName, -1, wide.data(), EventNameBufferChars);
  if (written <= 0) {
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

  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = &windowProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = WindowClassName;
  if (::RegisterClassExW(&windowClass) == 0) {
    return 1;
  }

  HWND window =
      ::CreateWindowExW(0, WindowClassName, WindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 320, 260, nullptr, nullptr, instance, nullptr);
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
