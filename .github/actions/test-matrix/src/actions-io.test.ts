// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { liveDeps, main, makeIo, readInputs, type IoDeps } from './actions-io.js';
import { emptyState, type MatrixState } from './render.js';
import type { ParsedJob } from './junit.js';

const meta = { runNumber: '1', runUrl: 'u', commit: 'c' };
const okXml = '<testsuite><testcase name="A.a"/></testsuite>';

/** A fully-faked dep bundle; override the one or two calls a test cares about. */
const fakeDeps = (over: Partial<IoDeps> = {}): IoDeps => ({
  readFile: () => okXml,
  writeSummary: async () => {},
  upsert: async () => {},
  getInput: () => '',
  token: 'tok',
  runId: 'r1',
  warn: () => {},
  info: () => {},
  fail: () => {},
  ...over,
});

test('makeIo.readResults parses JUnit on success', () => {
  assert.equal(makeIo(fakeDeps()).readResults('x.xml')?.passed, 1);
});

test('makeIo.readResults warns and returns null when the read/parse fails', () => {
  const warnings: string[] = [];
  const io = makeIo(
    fakeDeps({
      readFile: () => {
        throw new Error('nope');
      },
      warn: (m) => warnings.push(m),
    }),
  );
  assert.equal(io.readResults('missing.xml'), null);
  assert.match(warnings[0], /could not read missing\.xml.*nope/);
});

test('makeIo.upsertComment seeds the expected set on init (via applyReport)', async () => {
  let merge: ((s: MatrixState) => void) | undefined;
  const io = makeIo(
    fakeDeps({
      getInput: (n) => (n === 'jobs' ? 'x64, tsan' : ''),
      upsert: async (o) => {
        assert.equal(o.create, true);
        assert.equal(o.token, 'tok');
        assert.equal(o.runId, 'r1');
        assert.equal(o.prNumber, 7);
        merge = o.merge;
      },
    }),
  );
  await io.upsertComment(true, '', 7, null);
  const s = emptyState(meta);
  merge!(s);
  assert.deepEqual(s.expectedJobs, ['x64', 'tsan']);
});

test('makeIo.upsertComment folds a job column on report', async () => {
  let merge: ((s: MatrixState) => void) | undefined;
  const io = makeIo(
    fakeDeps({
      upsert: async (o) => {
        merge = o.merge;
      },
    }),
  );
  const result: ParsedJob = { passed: 1, failed: 0, skipped: 0, total: 1, tests: { 'A.a': 'passed' } };
  await io.upsertComment(false, 'x64', 7, result);
  const s = emptyState(meta);
  merge!(s);
  assert.deepEqual(s.jobOrder, ['x64']);
});

test('makeIo.upsertComment warns instead of rejecting when the upsert fails', async () => {
  const warnings: string[] = [];
  const io = makeIo(
    fakeDeps({
      upsert: async () => {
        throw new Error('rate limited');
      },
      warn: (m) => warnings.push(m),
    }),
  );
  await io.upsertComment(false, 'x64', 7, null); // must not reject
  assert.match(warnings[0], /could not update the PR comment.*rate limited/);
});

test('makeIo delegates writeSummary / warn / info / fail to the injected calls', async () => {
  const calls: string[] = [];
  const io = makeIo(
    fakeDeps({
      writeSummary: async (md) => {
        calls.push(`sum:${md}`);
      },
      warn: (m) => calls.push(`warn:${m}`),
      info: (m) => calls.push(`info:${m}`),
      fail: (m) => calls.push(`fail:${m}`),
    }),
  );
  await io.writeSummary('S');
  io.warn('W');
  io.info('I');
  io.fail('F');
  assert.deepEqual(calls, ['sum:S', 'warn:W', 'info:I', 'fail:F']);
});

test('readInputs reads mode/job/report-path from the environment', () => {
  const saved = { ...process.env };
  try {
    process.env['INPUT_MODE'] = 'report';
    process.env['INPUT_JOB'] = 'x64';
    process.env['INPUT_REPORT-PATH'] = 'out.xml';
    assert.deepEqual(readInputs(), { mode: 'report', job: 'x64', reportPath: 'out.xml' });
    delete process.env['INPUT_REPORT-PATH'];
    assert.equal(readInputs().reportPath, 'test-results.xml'); // falls back to the default
  } finally {
    process.env = saved;
  }
});

test('liveDeps binds real fs, getInput, and the log/fail calls', async () => {
  const saved = { ...process.env };
  const savedExit = process.exitCode;
  const dir = mkdtempSync(join(tmpdir(), 'vox-io-'));
  try {
    process.env['INPUT_TOKEN'] = 'live-tok';
    process.env['GITHUB_RUN_ID'] = '999';
    const deps = liveDeps();

    assert.equal(deps.token, 'live-tok');
    assert.equal(deps.runId, '999');
    assert.equal(deps.getInput('token'), 'live-tok');

    const xml = join(dir, 'r.xml');
    writeFileSync(xml, okXml);
    assert.match(deps.readFile(xml), /testcase/);
    // deps.writeSummary (the live core.summary path) is exercised by the main() test below,
    // which is the sole core.summary user — @actions/core caches the summary file path, so two
    // tests writing to different files would collide.

    // warn/info/fail emit ::workflow:: commands to stdout; silence them for the three sync
    // calls (so they don't post annotations or corrupt the test reporter's output) while still
    // exercising the bindings, then restore stdout and the exit code setFailed flipped.
    const realWrite = process.stdout.write.bind(process.stdout);
    (process.stdout as unknown as { write: () => boolean }).write = () => true;
    try {
      deps.warn('w');
      deps.info('i');
      deps.fail('boom');
    } finally {
      process.stdout.write = realWrite;
    }
  } finally {
    process.env = saved;
    process.exitCode = savedExit; // undo setFailed so the suite still reports success
  }
});

test('main runs the report end-to-end on the no-PR path and writes the summary', async () => {
  const saved = { ...process.env };
  const dir = mkdtempSync(join(tmpdir(), 'vox-main-'));
  try {
    delete process.env['GITHUB_EVENT_PATH']; // no PR -> context.payload has no pull_request
    process.env['INPUT_MODE'] = 'report';
    process.env['INPUT_JOB'] = 'smoke';
    const xml = join(dir, 'r.xml');
    writeFileSync(xml, okXml);
    process.env['INPUT_REPORT-PATH'] = xml;
    const summaryFile = join(dir, 'summary.md');
    writeFileSync(summaryFile, '');
    process.env['GITHUB_STEP_SUMMARY'] = summaryFile;

    // Pass prNumber explicitly so the live github.context (a real PR event under CI) is never
    // consulted — that keeps the test off the octokit path; it writes the summary and stops.
    await main(undefined);

    assert.match(readFileSync(summaryFile, 'utf8'), /🧪 Tests — smoke/); // wrote the per-job summary
  } finally {
    process.env = saved;
  }
});

test('liveDeps falls back to GITHUB_TOKEN then empty, and to an empty run id', () => {
  const saved = { ...process.env };
  try {
    delete process.env['INPUT_TOKEN'];
    delete process.env['GITHUB_RUN_ID'];
    process.env['GITHUB_TOKEN'] = 'gh-tok';
    let deps = liveDeps();
    assert.equal(deps.token, 'gh-tok'); // no token input -> GITHUB_TOKEN
    assert.equal(deps.runId, ''); // no run id -> empty
    delete process.env['GITHUB_TOKEN'];
    deps = liveDeps();
    assert.equal(deps.token, ''); // neither set -> empty
  } finally {
    process.env = saved;
  }
});
