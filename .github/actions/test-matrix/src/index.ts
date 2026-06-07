// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { readFileSync } from 'node:fs';
import * as core from '@actions/core';
import * as github from '@actions/github';
import { parseJunit, type ParsedJob } from './junit.js';
import { mergeJob } from './render.js';
import { upsert } from './comment.js';

function writeJobSummary(job: string, r: ParsedJob): Promise<unknown> {
  const failed = Object.entries(r.tests)
    .filter(([, s]) => s === 'failed')
    .map(([n]) => n);
  let s = core.summary
    .addHeading(`🧪 Tests — ${job}`, 3)
    .addRaw(`✅ ${r.passed} passed · ❌ ${r.failed} failed · ⏭️ ${r.skipped} skipped · ${r.total} total`, true);
  if (failed.length > 0) {
    s = s.addHeading('Failed', 4).addList(failed);
  }
  return s.write();
}

async function run(): Promise<void> {
  const mode = core.getInput('mode', { required: true });
  const token = core.getInput('token') || process.env.GITHUB_TOKEN || '';
  const runId = process.env.GITHUB_RUN_ID ?? '';
  const prNumber = github.context.payload.pull_request?.number;

  if (!prNumber) {
    core.info('Not a pull_request event — skipping the test-matrix comment.');
    // Still surface this job's results in the run Summary on push runs.
    if (mode === 'report') {
      const job = core.getInput('job', { required: true });
      await writeJobSummary(job, parseJunit(readFileSync(core.getInput('report-paths') || 'test-results.xml', 'utf8')));
    }
    return;
  }

  if (mode === 'init') {
    await upsert({ token, runId, prNumber, merge: () => {} });
    return;
  }

  if (mode === 'report') {
    const job = core.getInput('job', { required: true });
    const result = parseJunit(readFileSync(core.getInput('report-paths') || 'test-results.xml', 'utf8'));
    await writeJobSummary(job, result);
    await upsert({ token, runId, prNumber, merge: (state) => mergeJob(state, job, result) });
    return;
  }

  core.setFailed(`Unknown mode '${mode}' (expected 'init' or 'report').`);
}

run().catch((err) => core.setFailed(err instanceof Error ? err.message : String(err)));
