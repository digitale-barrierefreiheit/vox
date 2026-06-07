// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only test seam over UiaProvider's COM object creation.
///
/// `UiaProvider` builds its UI Automation client with `CoCreateInstance`
/// (`CLSID_CUIAutomation` -> `IUIAutomation`) and walks automation -> cache
/// request / focused element / patterns from there. This seam lets a test
/// substitute that root creation with a factory returning a *mock*
/// `IUIAutomation` (or a failure `HRESULT`), so the provider's extraction and
/// focus-event paths are unit-tested with no real UI Automation tree (ADR-12,
/// issue #68). Production code never sets a factory and keeps using
/// `CoCreateInstance`.
#ifndef VOX_PROVIDER_UIA_TEST_SEAM_HPP
#define VOX_PROVIDER_UIA_TEST_SEAM_HPP

#if defined(_WIN32)

#  include <functional>

struct IUIAutomation; // forward-declared so this header needs no Windows SDK

namespace vox::provider::testing {

/// @brief Creates the UI Automation client. Mirrors `CoCreateInstance`: returns
///        an `HRESULT` (as `long`) and, on success, sets `*out` to an AddRef'd
///        interface. Return a failure code to exercise the degraded path.
using AutomationFactory = std::function<long(IUIAutomation** out)>;

/// @brief Installs @p factory for the next `UiaProvider` construction; an empty
///        factory restores the real `CoCreateInstance`. Test-only, not thread-safe.
void setAutomationFactory(AutomationFactory factory);

} // namespace vox::provider::testing

#endif // defined(_WIN32)

#endif // VOX_PROVIDER_UIA_TEST_SEAM_HPP
