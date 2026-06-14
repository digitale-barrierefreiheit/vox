// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { applyReport, codeCell, emptyState, mergeJob, mergeUnavailable, parseExpectedJobs, parseState, renderComment } from './render.js';
import type { ParsedJob } from './junit.js';

const meta = { runNumber: '42', runUrl: 'https://x/42', commit: 'abc1234' };
const pass = (name: string): ParsedJob => ({ passed: 1, failed: 0, skipped: 0, total: 1, tests: { [name]: 'passed' } });

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
  assert.deepEqual(back.jobOrder, ['x64', 'tsan']);
  // Re-rendering from the recovered state is byte-stable (so the retry loop converges).
  assert.equal(renderComment('run1', back), body);
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
  assert.ok(back);
  assert.equal(back.testNames[0], evil);
  assert.equal(renderComment('r', back), body);
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
  // status length not aligned to testNames (1 test, 2 codes)
  assert.equal(parseState(encode({ ...base, jobs: { x64: { p: 1, f: 0, s: 0, status: 'PP' } } })), null);
  // jobOrder references a job with no column
  assert.equal(parseState(encode({ ...base, jobOrder: ['x64', 'ghost'], jobs: { x64: { p: 1, f: 0, s: 0, status: 'P' } } })), null);
  // status contains an unknown code (would render undefined cells)
  assert.equal(parseState(encode({ ...base, jobs: { x64: { p: 1, f: 0, s: 0, status: 'X' } } })), null);
  // missing meta string
  assert.equal(parseState(encode({ ...base, runUrl: undefined, jobs: {} })), null);
  // non-string entry in testNames
  assert.equal(parseState(encode({ ...base, testNames: [1], jobs: {} })), null);
});

test('with an expected set, a partial run stays ⏳ and never shows a premature ✅', () => {
  const s = emptyState(meta, ['x64', 'x86', 'de-DE', 'asan', 'tsan']);
  mergeJob(s, 'x64', { passed: 3, failed: 0, skipped: 0, total: 3, tests: { 'A.a': 'passed', 'A.b': 'passed', 'A.c': 'passed' } });
  const body = renderComment('r', s);
  assert.match(body, /⏳ 3 passed, 0 skipped so far — 1\/5 expected jobs reported/);
  assert.match(body, /waiting for x86, de-DE, asan, tsan/);
  assert.doesNotMatch(body, /✅ \*\*All/); // only 1 of 5 has reported
});

test('✅ appears only once every expected job has reported and passed', () => {
  const s = emptyState(meta, ['x64', 'tsan']);
  mergeJob(s, 'x64', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'passed' } });
  assert.match(renderComment('r', s), /⏳/); // 1 of 2 — still waiting
  mergeJob(s, 'tsan', { passed: 1, failed: 0, skipped: 0, total: 1, tests: { 'A.a': 'passed' } });
  assert.match(renderComment('r', s), /✅ \*\*All 3 passed\*\* \(0 skipped\) — 2 expected jobs reported/);
});

test('a job that published no results forces ❌, never a stuck ⏳ or a green ✅', () => {
  const s = emptyState(meta, ['x64', 'asan']);
  mergeJob(s, 'x64', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { 'A.a': 'passed', 'A.b': 'passed' } });
  mergeUnavailable(s, 'asan'); // asan ran but produced no JUnit (e.g. its build failed)
  const body = renderComment('r', s);
  assert.match(body, /❌ \*\*1 job\(s\) without results\*\* \(asan\)/);
  assert.doesNotMatch(body, /✅ \*\*All/);
  assert.match(body, /\| asan \| — \| ⚠️ \| — \| no results \|/); // summary row marks it
});

test('applyReport: init seeds the expected set; report folds a column or marks it unavailable', () => {
  const s = emptyState(meta);
  applyReport(s, { create: true, job: '', result: null, expectedJobs: ['x64', 'asan'] });
  assert.deepEqual(s.expectedJobs, ['x64', 'asan']);
  assert.deepEqual(s.jobOrder, []); // init adds no column of its own

  applyReport(s, { create: false, job: 'x64', result: pass('A.a'), expectedJobs: [] });
  assert.deepEqual(s.jobOrder, ['x64']);

  applyReport(s, { create: false, job: 'asan', result: null, expectedJobs: [] });
  assert.deepEqual(s.jobOrder, ['x64', 'asan']);
  assert.equal(s.jobs['asan'].u, true);
});

test('parseState back-fills expectedJobs for a legacy comment, and round-trips a seeded one', () => {
  const legacy = { runNumber: '1', runUrl: 'x', commit: 'c', jobOrder: ['x64'], testNames: ['A.a'], jobs: { x64: { p: 1, f: 0, s: 0, status: 'P' } } };
  const legacyBody = `<!-- vox-test-matrix-state: ${Buffer.from(JSON.stringify(legacy)).toString('base64')} -->`;
  assert.deepEqual(parseState(legacyBody)?.expectedJobs, []); // parses, with no completeness gate

  const s = emptyState(meta, ['x64', 'tsan']);
  mergeJob(s, 'x64', pass('A.a'));
  assert.deepEqual(parseState(renderComment('r', s))?.expectedJobs, ['x64', 'tsan']); // survives round-trip
});

test('parseState rejects a truncated comment, a null column, and a non-object jobs map', () => {
  const encode = (o: unknown): string => `<!-- vox-test-matrix-state: ${Buffer.from(JSON.stringify(o)).toString('base64')} -->`;
  const base = { runNumber: '1', runUrl: 'x', commit: 'c', expectedJobs: [], jobOrder: ['x64'], testNames: ['A.a'] };
  assert.equal(parseState('<!-- vox-test-matrix-state: not-terminated'), null); // no closing -->
  assert.equal(parseState(encode({ ...base, jobs: { x64: null } })), null); // a null job column
  assert.equal(parseState(encode({ ...base, jobs: null })), null); // jobs is not an object
});

test('codeCell pads a name that starts or ends with a backtick so the span still renders', () => {
  assert.equal(codeCell('`b`'), '`` `b` ``'); // fence widened by one, spaces padding the edges
});

test('parseExpectedJobs trims, drops empties, and de-duplicates the init input', () => {
  assert.deepEqual(parseExpectedJobs('x64, x86 ,de-DE,asan,tsan'), ['x64', 'x86', 'de-DE', 'asan', 'tsan']);
  assert.deepEqual(parseExpectedJobs(''), []); // unseeded init (e.g. a non-PR run)
  assert.deepEqual(parseExpectedJobs(' , ,'), []); // stray commas/spaces seed no blank labels
  assert.deepEqual(parseExpectedJobs('x64,x64, x86 ,x86'), ['x64', 'x86']); // a repeat can't inflate the denominator
});

test('parseState returns null when the decoded payload is not JSON', () => {
  // Well-formed markers, but the base64 decodes to plain text -> JSON.parse throws -> caught.
  const notJson = Buffer.from('hello world', 'utf8').toString('base64');
  assert.equal(parseState(`<!-- vox-test-matrix-state: ${notJson} -->`), null);
});

test('parseState returns null for a payload that is valid JSON but not a state object', () => {
  const enc = (v: string): string => `<!-- vox-test-matrix-state: ${Buffer.from(v, 'utf8').toString('base64')} -->`;
  assert.equal(parseState(enc('null')), null); // decoded is null — the back-fill guard skips it
  assert.equal(parseState(enc('42')), null); // decoded is a primitive, not an object
});

test('parseState validates and round-trips an unavailable (u:true) job column', () => {
  const s = emptyState(meta, ['x64', 'asan']);
  mergeJob(s, 'x64', pass('A.a'));
  mergeUnavailable(s, 'asan');
  const back = parseState(renderComment('r', s));
  assert.equal(back?.jobs['asan'].u, true); // the boolean u flag passes isJobColumn and survives
});

test('renderComment on a fresh, not-yet-reported state shows the running headline and no matrix', () => {
  const body = renderComment('r', emptyState(meta, ['x64', 'tsan']));
  assert.match(body, /⏳ Tests running…/);
  assert.doesNotMatch(body, /Full test matrix/); // no test names yet, so no matrix block
});
