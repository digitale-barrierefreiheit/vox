// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Normalized control states and a small flag-set value type.
///
/// `State` is the provider-independent "what is true about this control right
/// now" axis of an AccessibleNode (architecture §5.1, ADR-04/ADR-12). Several
/// states are deliberately split so the model can express things a single bool
/// cannot:
///   - `Checked` + `Mixed`: a checkbox is off (neither), on (`Checked`), or
///     indeterminate (`Mixed`).
///   - `Expandable` + `Expanded`: a control is not expandable (neither),
///     collapsed (`Expandable` only), or expanded (both).
/// Interpreting a state is keyed by the node's Role; the model only stores it.
#ifndef VOX_MODEL_STATE_HPP
#define VOX_MODEL_STATE_HPP

#include <cstdint>
#include <string>
#include <utility>

namespace vox::model {

/// A single normalized state, one bit each. The set grows over time; values are
/// fixed so cached/serialized data stays stable.
///
/// The base type is fixed at 32 bits to match StateSet's mask and to leave room
/// for well past 16 flags; performance-enum-size (which would shrink it to the
/// current value range) is therefore suppressed here by design.
// NOLINTNEXTLINE(performance-enum-size)
enum class State : std::uint32_t {
  Focusable = 1U << 0U,  ///< Can receive keyboard focus.
  Focused = 1U << 1U,    ///< Currently has keyboard focus.
  Disabled = 1U << 2U,   ///< Present but not currently operable.
  Checked = 1U << 3U,    ///< Toggle is on (checkbox/radio); see also Mixed.
  Mixed = 1U << 4U,      ///< Toggle is indeterminate (tri-state checkbox).
  Expandable = 1U << 5U, ///< Can be expanded/collapsed at all.
  Expanded = 1U << 6U,   ///< Is expanded (only meaningful with Expandable).
  Selected = 1U << 7U,   ///< Selected within its container.
  ReadOnly = 1U << 8U,   ///< Value is displayed but cannot be edited.
};

/// An unordered set of `State` flags. A tiny, allocation-free, `constexpr` /
/// `noexcept` value type. The query helpers (`test`/`any`/`none`/`bits`) are
/// `[[nodiscard]]`; the mutators (`set`/`clear`) return `*this` purely as a
/// chaining convenience and are intentionally NOT `[[nodiscard]]`, because
/// discarding the result (e.g. `states.set(State::Focused);`) is the common,
/// intended usage.
class StateSet {
public:
  /// Constructs an empty set (no states).
  constexpr StateSet() noexcept = default;

  /// Constructs a set holding the single state @p state.
  constexpr explicit StateSet(State state) noexcept : bits_{std::to_underlying(state)} {}

  /// Adds @p state to the set. Returns `*this` so calls can be chained; the
  /// result is fine to discard, hence intentionally not `[[nodiscard]]`.
  constexpr StateSet& set(State state) noexcept {
    bits_ |= std::to_underlying(state);
    return *this;
  }

  /// Removes @p state from the set. Returns `*this` so calls can be chained; the
  /// result is fine to discard, hence intentionally not `[[nodiscard]]`.
  constexpr StateSet& clear(State state) noexcept {
    bits_ &= ~std::to_underlying(state);
    return *this;
  }

  /// @brief Tests whether @p state is present.
  [[nodiscard]] constexpr bool test(State state) const noexcept {
    return (bits_ & std::to_underlying(state)) != 0U;
  }

  /// @brief Returns true if any state is set.
  [[nodiscard]] constexpr bool any() const noexcept {
    return bits_ != 0U;
  }

  /// @brief Returns true if no state is set.
  [[nodiscard]] constexpr bool none() const noexcept {
    return bits_ == 0U;
  }

  /// @brief Returns the raw bit pattern (for serialization/diagnostics).
  [[nodiscard]] constexpr std::uint32_t bits() const noexcept {
    return bits_;
  }

  /// Two sets are equal iff they hold the same states.
  [[nodiscard]] friend constexpr bool operator==(const StateSet&,
                                                 const StateSet&) noexcept = default;

private:
  std::uint32_t bits_{0};
};

/// @brief Returns a stable ASCII description of @p states, e.g.
///        "Focusable|Focused", or "None" when empty.
/// @return A diagnostic string for logging, the inspectable speech seam, and
///         test output — not a user-facing announcement (that is the lexicon's
///         job, #34). Order is by ascending bit value and is stable.
[[nodiscard]] std::string toString(const StateSet& states);

} // namespace vox::model

#endif // VOX_MODEL_STATE_HPP
