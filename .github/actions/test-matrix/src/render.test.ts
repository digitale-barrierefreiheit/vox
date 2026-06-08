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

test('hostile test names are escaped and cannot corrupt the embedded state', () => {
  const s = emptyState(meta);
  const evil = 'Weird.name|with`backtick-->and pipe';
  mergeJob(s, 'x64', { passed: 1, failed: 0, skipped: 0, total: 1, tests: { [evil]: 'passed' } });
  const body = renderComment('r', s);
  // The pipe is escaped in the rendered cell (so the table column survives).
  assert.ok(body.includes(String.raw`\|`));
  assert.ok(!body.includes('name|with'));
  // The embedded state (base64) round-trips the exact name despite the `-->`/pipe/backtick,
  // proving it wasn't corrupted; and re-rendering is byte-stable (so the retry converges).
  const back = parseState(body);
  assert.equal(back?.testNames[0], evil);
  assert.equal(renderComment('r', back!), body);
});

test('parseState rejects a malformed embedded state (falls back to a fresh one)', () => {
  const bad = Buffer.from(JSON.stringify({ not: 'a state' })).toString('base64');
  const body = `<!-- vox-test-matrix run=r -->\n<!-- vox-test-matrix-state: ${bad} -->`;
  assert.equal(parseState(body), null);
});

test('parseState rejects state with a malformed job column or missing meta', () => {
  const encode = (o: unknown): string =>
    `<!-- vox-test-matrix-state: ${Buffer.from(JSON.stringify(o)).toString('base64')} -->`;
  const base = { runNumber: '1', runUrl: 'x', commit: 'c', jobOrder: ['x64'], testNames: ['A.a'] };
  // job column missing `status`
  assert.equal(parseState(encode({ ...base, jobs: { x64: { p: 1, f: 0, s: 0 } } })), null);
  // missing meta string
  assert.equal(parseState(encode({ ...base, runUrl: undefined, jobs: {} })), null);
  // non-string entry in testNames
  assert.equal(parseState(encode({ ...base, testNames: [1], jobs: {} })), null);
});
