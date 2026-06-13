// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { errorMessage } from './util.js';

test('errorMessage returns an Error message, or the string form of any other thrown value', () => {
  assert.equal(errorMessage(new Error('boom')), 'boom');
  assert.equal(errorMessage('plain string'), 'plain string');
  assert.equal(errorMessage(42), '42');
  assert.equal(errorMessage({ code: 1 }), '[object Object]');
});
