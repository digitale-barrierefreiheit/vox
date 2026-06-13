// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

// Tiny entry point. All logic lives in (unit-tested) main() — read inputs, build the live IO,
// and run, routing failures to setFailed. This file is the irreducible ESM shim ncc bundles:
// it reads the live PR number from the event context and hands it to main().

import * as github from '@actions/github';
import { main } from './actions-io.js';

await main(github.context.payload.pull_request?.number);
