// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { emptyState, mergeJob, parseState, renderComment } from './render.js';

const meta = { runNumber: '42', runUrl: 'https://x/42', commit: 'abc1234' };

test('summary, failure matrix, full matrix, and embedded state round-trip', () => {
  const s = emptyState(meta);
  mergeJob(s, 'x64', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'passed' } });
  mergeJob(s, 'tsan', { passed: 1, failed: 1, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'failed' } });

  const body = renderComment('run1', s);
  assert.match(body, /run=run1/);
  assert.match(body, /❌ \*\*1 failed\*\*/);
  assert.match(body, /Failed tests/);
  assert.match(body, /Full test matrix \(2 tests × 2 jobs\)/);

  const back = parseState(body);
  assert.ok(back);
  assert.deepEqual(back?.jobOrder, ['x64', 'tsan']);
  // Re-rendering from the recovered state is byte-stable (so the retry loop converges).
  assert.equal(renderComment('run1', back!), body);
});

test('a later job adding new tests pads earlier columns', () => {
  const s = emptyState(meta);
  mergeJob(s, 'x64', { passed: 1, failed: 0, skipped: 0, total: 1, tests: { 'A.a': 'passed' } });
  mergeJob(s, 'de-DE', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'German.x': 'passed' } });

  assert.equal(s.jobs['x64'].status.length, s.testNames.length);
  const body = renderComment('r', s);
  assert.match(body, /German\.x/);
  assert.match(body, /—/); // x64 did not run German.x
});

test('all-pass shows a green summary and no failure section', () => {
  const s = emptyState(meta);
  mergeJob(s, 'x64', { passed: 2, failed: 0, skipped: 1, total: 3, tests: { 'A.a': 'passed', 'A.b': 'passed', 'A.c': 'skipped' } });
  const body = renderComment('r', s);
  assert.match(body, /✅ \*\*All 2 passed\*\*/);
  assert.doesNotMatch(body, /Failed tests/);
});

test('re-reporting a job replaces its column rather than duplicating it', () => {
  const s = emptyState(meta);
  mergeJob(s, 'x64', { passed: 1, failed: 1, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'failed' } });
  mergeJob(s, 'x64', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'passed' } });
  assert.deepEqual(s.jobOrder, ['x64']);
  assert.equal(s.jobs['x64'].f, 0);
});
