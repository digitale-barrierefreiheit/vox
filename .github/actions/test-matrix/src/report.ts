// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

// The action's orchestration and pure rendering, kept here (and unit-tested) so index.ts
// stays a tiny entry point that only wires the real GitHub Actions IO to `run`.

import type { ParsedJob } from './junit.js';
import { codeCell } from './render.js';

export interface Inputs {
  mode: string;
  job: string;
  reportPath: string;
}

/** The side-effecting operations `run` needs, injected so it can be tested with fakes. */
export interface Io {
  /** Read + parse this job's JUnit file, or null on any error (e.g. the build failed first). */
  readResults(path: string): ParsedJob | null;
  writeSummary(markdown: string): Promise<void>;
  /** Create (init) or update (report) the run's PR comment with this job's column. */
  upsertComment(create: boolean, job: string, prNumber: number, result: ParsedJob | null): Promise<void>;
  warn(message: string): void;
  info(message: string): void;
  fail(message: string): void;
}

/** Pure: a job's run-Summary markdown (counts plus any failing test names). */
export function summaryMarkdown(job: string, r: ParsedJob): string {
  const failed = Object.entries(r.tests)
    .filter(([, status]) => status === 'failed')
    .map(([name]) => name);
  const head = `### 🧪 Tests — ${job}\n\n✅ ${r.passed} passed · ❌ ${r.failed} failed · ⏭️ ${r.skipped} skipped · ${r.total} total\n`;
  if (failed.length === 0) return head;
  const list = failed.map((name) => `- ${codeCell(name)}`).join('\n');
  return `${head}\n#### Failed\n${list}\n`;
}

/** Pure: the Summary shown when a job produced no JUnit (e.g. the build failed first). */
export function unavailableMarkdown(job: string): string {
  return `### 🧪 Tests — ${job}\n\n⚠️ Results unavailable — no JUnit report (the build may have failed before tests ran).\n`;
}

/** Reject an unknown mode or a report with no job (fails the step), else true. */
function validInputs(inputs: Inputs, io: Io): boolean {
  if (inputs.mode !== 'init' && inputs.mode !== 'report') {
    io.fail(`Unknown mode '${inputs.mode}' (expected 'init' or 'report').`);
    return false;
  }
  if (inputs.mode === 'report' && !inputs.job) {
    io.fail("report mode requires a non-empty 'job' input.");
    return false;
  }
  return true;
}

/** Validate inputs, write the per-job Summary, and (on PRs) fold this job into the comment. */
export async function run(inputs: Inputs, prNumber: number | undefined, io: Io): Promise<void> {
  if (!validInputs(inputs, io)) return;

  const result = inputs.mode === 'report' ? io.readResults(inputs.reportPath) : null;
  // Always write a per-job Summary in report mode — a "results unavailable" note when the
  // JUnit is missing is clearer than an empty Summary.
  if (inputs.mode === 'report') {
    await io.writeSummary(result ? summaryMarkdown(inputs.job, result) : unavailableMarkdown(inputs.job));
  }

  if (prNumber === undefined) {
    io.info('Not a pull_request event — skipping the PR comment.');
    return;
  }
  await io.upsertComment(inputs.mode === 'init', inputs.job, prNumber, result);
}
