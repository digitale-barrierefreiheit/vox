// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import * as github from '@actions/github';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

// Exercises the entry shim end-to-end: clear the PR from the context so it takes the no-PR
// path (main(undefined) -> no octokit), point the inputs at a temp JUnit + summary file, then
// dynamic-import index.ts so its top-level `await main(...)` runs. Restores the mutated context
// and env afterwards.
test('the index entry reads the (no-PR) context and runs main, writing the per-job summary', async () => {
  const savedEnv = { ...process.env };
  const savedPayload = github.context.payload;
  const dir = mkdtempSync(join(tmpdir(), 'vox-entry-'));
  try {
    github.context.payload = {}; // no pull_request -> prNumber undefined -> never reaches octokit
    process.env['INPUT_MODE'] = 'report';
    process.env['INPUT_JOB'] = 'smoke';
    const xml = join(dir, 'r.xml');
    writeFileSync(xml, '<testsuite><testcase name="A.a"/></testsuite>');
    process.env['INPUT_REPORT-PATH'] = xml;
    const summary = join(dir, 'summary.md');
    writeFileSync(summary, '');
    process.env['GITHUB_STEP_SUMMARY'] = summary;

    await import('./index.js'); // runs `await main(github.context.payload.pull_request?.number)`

    assert.match(readFileSync(summary, 'utf8'), /🧪 Tests — smoke/);
  } finally {
    process.env = savedEnv;
    github.context.payload = savedPayload;
  }
});
