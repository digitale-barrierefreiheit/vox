// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

// Tiny entry point. All logic lives in (unit-tested) main() — read inputs, build the live IO,
// and run, routing failures to setFailed. This file is the irreducible ESM shim ncc bundles.

import { main } from './actions-io.js';

await main();
