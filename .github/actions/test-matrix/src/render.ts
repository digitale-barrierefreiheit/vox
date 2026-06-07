// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import type { ParsedJob, Status } from './junit.js';

// One char per test keeps the comment's embedded state small enough for the 65 KB limit
// even with a few hundred tests across several jobs.
type Code = 'P' | 'F' | 'S' | '-';
const CODE: Record<Status, Code> = { passed: 'P', failed: 'F', skipped: 'S' };
const ICON: Record<Code, string> = { P: '✅', F: '❌', S: '⏭️', '-': '—' };

export interface JobColumn {
  p: number;
  f: number;
  s: number;
  /** status code per testNames index */
  status: string;
}

/** The full run state, embedded (as JSON) in the comment so every job can rebuild it. */
export interface MatrixState {
  runNumber: string;
  runUrl: string;
  commit: string;
  jobOrder: string[];
  testNames: string[];
  jobs: Record<string, JobColumn>;
}

export function emptyState(meta: Pick<MatrixState, 'runNumber' | 'runUrl' | 'commit'>): MatrixState {
  return { ...meta, jobOrder: [], testNames: [], jobs: {} };
}

/** Fold one job's results into the state (idempotent — re-reporting replaces the column). */
export function mergeJob(state: MatrixState, job: string, parsed: ParsedJob): void {
  // Extend the shared test-name list; pad existing columns for any new names.
  for (const name of Object.keys(parsed.tests)) {
    if (!state.testNames.includes(name)) {
      state.testNames.push(name);
      for (const col of Object.values(state.jobs)) col.status += '-';
    }
  }
  const status = state.testNames
    .map((name) => (parsed.tests[name] ? CODE[parsed.tests[name]] : '-'))
    .join('');
  state.jobs[job] = { p: parsed.passed, f: parsed.failed, s: parsed.skipped, status };
  if (!state.jobOrder.includes(job)) state.jobOrder.push(job);
}

const runMarker = (runId: string): string => `<!-- vox-test-matrix run=${runId} -->`;
const STATE_OPEN = '<!-- vox-test-matrix-state:';
const STATE_CLOSE = '-->';

export { runMarker };

/** Recover the embedded state from a comment body, or null if absent/corrupt. */
export function parseState(body: string): MatrixState | null {
  const open = body.indexOf(STATE_OPEN);
  if (open < 0) return null;
  const start = open + STATE_OPEN.length;
  const end = body.indexOf(STATE_CLOSE, start);
  if (end < 0) return null;
  try {
    return JSON.parse(body.slice(start, end).trim()) as MatrixState;
  } catch {
    return null;
  }
}

const codeAt = (col: JobColumn, i: number): Code => (col.status[i] as Code) ?? '-';

/** Render the whole comment body (marker + tables + embedded state). */
export function renderComment(runId: string, state: MatrixState): string {
  const jobs = state.jobOrder;
  const totals = jobs.reduce(
    (a, j) => {
      const c = state.jobs[j];
      return { p: a.p + c.p, f: a.f + c.f, s: a.s + c.s };
    },
    { p: 0, f: 0, s: 0 },
  );

  const header = `### 🧪 Test results — run [#${state.runNumber}](${state.runUrl}) \`${state.commit}\``;
  const statusLine =
    jobs.length === 0
      ? '⏳ Tests running… results appear as each job finishes.'
      : totals.f > 0
        ? `❌ **${totals.f} failed**, ${totals.p} passed, ${totals.s} skipped — ${jobs.length} job(s) reported`
        : `✅ **All ${totals.p} passed** (${totals.s} skipped) — ${jobs.length} job(s) reported`;

  let summary = '| Job | ✅ | ❌ | ⏭️ | Total |\n|---|--:|--:|--:|--:|\n';
  for (const j of jobs) {
    const c = state.jobs[j];
    summary += `| ${j} | ${c.p} | ${c.f} | ${c.s} | ${c.p + c.f + c.s} |\n`;
  }

  const colHead = `| Test | ${jobs.join(' | ')} |\n|---|${jobs.map(() => ':-:').join('|')}|`;
  const row = (i: number): string =>
    `| \`${state.testNames[i]}\` | ${jobs.map((j) => ICON[codeAt(state.jobs[j], i)]).join(' | ')} |`;

  const failedIdx = state.testNames
    .map((_, i) => i)
    .filter((i) => jobs.some((j) => codeAt(state.jobs[j], i) === 'F'));
  const failures =
    failedIdx.length > 0 ? `\n#### ❌ Failed tests\n${colHead}\n${failedIdx.map(row).join('\n')}\n` : '';

  const full =
    state.testNames.length > 0
      ? `\n<details><summary>Full test matrix (${state.testNames.length} tests × ${jobs.length} jobs)</summary>\n\n${colHead}\n${state.testNames
          .map((_, i) => row(i))
          .join('\n')}\n\n</details>\n`
      : '';

  return [
    runMarker(runId),
    header,
    '',
    statusLine,
    '',
    summary + failures + full,
    `${STATE_OPEN} ${JSON.stringify(state)} ${STATE_CLOSE}`,
  ].join('\n');
}
