// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/** A safe message string for any thrown value — an Error's `message`, otherwise its string
 *  form. Centralised so the catch sites don't each carry an `instanceof Error` branch. */
export const errorMessage = (err: unknown): string => (err instanceof Error ? err.message : String(err));
