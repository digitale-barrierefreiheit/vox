// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { run, summaryMarkdown, type Inputs, type Io } from './report.js';
import type { ParsedJob } from './junit.js';

const RESULT: ParsedJob = { passed: 2, failed: 1, skipped: 0, total: 3, tests: { 'A.a': 'passed', 'A.b': 'failed' } };

function fakeIo(overrides: Partial<Io> = {}) {
  const calls = {
    summary: [] as string[],
    comment: [] as { create: boolean; job: string; prNumber: number; res: ParsedJob | null }[],
    warn: [] as string[],
    info: [] as string[],
    fail: [] as string[],
  };
  const io: Io = {
    readResults: () => RESULT,
    writeSummary: async (md) => {
      calls.summary.push(md);
    },
    upsertComment: async (create, job, prNumber, res) => {
      calls.comment.push({ create, job, prNumber, res });
    },
    warn: (m) => {
      calls.warn.push(m);
    },
    info: (m) => {
      calls.info.push(m);
    },
    fail: (m) => {
      calls.fail.push(m);
    },
    ...overrides,
  };
  return { io, calls };
}

const inputs = (over: Partial<Inputs> = {}): Inputs => ({ mode: 'report', job: 'x64', reportPath: 'test-results.xml', ...over });

test('summaryMarkdown lists counts and only the failing tests', () => {
  const md = summaryMarkdown('x64', { passed: 1, failed: 1, skipped: 0, total: 2, tests: { 'A.ok': 'passed', 'A.bad': 'failed' } });
  assert.match(md, /🧪 Tests — x64/);
  assert.match(md, /❌ 1 failed/);
  assert.match(md, /A\.bad/);
  assert.doesNotMatch(md, /A\.ok/);
});

test('summaryMarkdown omits the Failed section when all pass', () => {
  const md = summaryMarkdown('x64', { passed: 2, failed: 0, skipped: 0, total: 2, tests: { a: 'passed', b: 'passed' } });
  assert.doesNotMatch(md, /Failed/);
});

test('run(report) writes the summary and updates the comment on a PR', async () => {
  const { io, calls } = fakeIo();
  await run(inputs(), 7, io);
  assert.equal(calls.summary.length, 1);
  assert.equal(calls.comment.length, 1);
  assert.equal(calls.comment[0].create, false);
  assert.equal(calls.comment[0].prNumber, 7);
  assert.equal(calls.comment[0].res, RESULT);
});

test('run(init) creates the comment and writes no summary', async () => {
  const { io, calls } = fakeIo();
  await run(inputs({ mode: 'init', job: '' }), 7, io);
  assert.equal(calls.summary.length, 0);
  assert.equal(calls.comment[0].create, true);
});

test('run off a PR writes the run Summary only', async () => {
  const { io, calls } = fakeIo();
  await run(inputs(), undefined, io);
  assert.equal(calls.summary.length, 1);
  assert.equal(calls.comment.length, 0);
  assert.equal(calls.info.length, 1);
});

test('run rejects an unknown mode without side effects', async () => {
  const { io, calls } = fakeIo();
  await run(inputs({ mode: 'bogus' }), 7, io);
  assert.equal(calls.fail.length, 1);
  assert.equal(calls.summary.length, 0);
  assert.equal(calls.comment.length, 0);
});

test('run(report) requires a non-empty job', async () => {
  const { io, calls } = fakeIo();
  await run(inputs({ job: '' }), 7, io);
  assert.equal(calls.fail.length, 1);
  assert.equal(calls.comment.length, 0);
});

test('run(report) writes an "unavailable" summary and still upserts when results are missing', async () => {
  const { io, calls } = fakeIo({ readResults: () => null });
  await run(inputs(), 7, io);
  assert.equal(calls.summary.length, 1);
  assert.match(calls.summary[0], /Results unavailable/);
  assert.equal(calls.comment[0].res, null);
});
