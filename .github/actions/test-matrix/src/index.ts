// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { readFileSync } from 'node:fs';
import * as core from '@actions/core';
import * as github from '@actions/github';
import { parseJunit, type ParsedJob } from './junit.js';
import { mergeJob } from './render.js';
import { upsert } from './comment.js';

/** Read + parse this job's JUnit file; on any error (e.g. the build failed before it was
 *  produced) warn and return null rather than throwing and masking the real failure. */
function loadResult(): ParsedJob | null {
  const path = core.getInput('report-path') || 'test-results.xml';
  try {
    return parseJunit(readFileSync(path, 'utf8'));
  } catch (err) {
    core.warning(`test-matrix: could not read ${path} (${err instanceof Error ? err.message : err}); skipping report.`);
    return null;
  }
}

// addList writes raw <li> markup, so HTML-escape the JUnit-sourced names and flatten any
// newlines before listing them.
const escapeListItem = (s: string): string =>
  s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll(/[\r\n]+/g, ' ');

function writeJobSummary(job: string, r: ParsedJob): Promise<unknown> {
  const failed = Object.entries(r.tests)
    .filter(([, s]) => s === 'failed')
    .map(([n]) => escapeListItem(n));
  core.summary
    .addHeading(`🧪 Tests — ${job}`, 3)
    .addRaw(`✅ ${r.passed} passed · ❌ ${r.failed} failed · ⏭️ ${r.skipped} skipped · ${r.total} total`, true);
  if (failed.length > 0) core.summary.addHeading('Failed', 4).addList(failed);
  return core.summary.write();
}

/** Upsert the run's comment. PR-comment writes 403 on restricted contexts (fork PRs get a
 *  read-only token) — treat that as non-fatal so the test job's own status stays the signal. */
async function postComment(prNumber: number, job: string, result: ParsedJob | null, create: boolean): Promise<void> {
  const token = core.getInput('token') || process.env.GITHUB_TOKEN || '';
  const runId = process.env.GITHUB_RUN_ID ?? '';
  try {
    await upsert({
      token,
      runId,
      prNumber,
      create,
      merge: (state) => {
        if (result) mergeJob(state, job, result);
      },
    });
  } catch (err) {
    core.warning(`test-matrix: could not update the PR comment (${err instanceof Error ? err.message : err}).`);
  }
}

async function run(): Promise<void> {
  const mode = core.getInput('mode', { required: true });
  if (mode !== 'init' && mode !== 'report') {
    core.setFailed(`Unknown mode '${mode}' (expected 'init' or 'report').`);
    return;
  }

  const job = mode === 'report' ? core.getInput('job', { required: true }) : '';
  const result = mode === 'report' ? loadResult() : null;
  if (result) await writeJobSummary(job, result);

  const prNumber = github.context.payload.pull_request?.number;
  if (prNumber) {
    await postComment(prNumber, job, result, mode === 'init');
  } else {
    core.info('Not a pull_request event — wrote the run Summary only.');
  }
}

try {
  await run();
} catch (err) {
  core.setFailed(err instanceof Error ? err.message : String(err));
}
