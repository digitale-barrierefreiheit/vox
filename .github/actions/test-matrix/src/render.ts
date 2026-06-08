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

// The embedded state is base64 so a value containing `-->` can't terminate the HTML
// comment early and corrupt it. encode/decode are symmetric.
const encodeState = (state: MatrixState): string =>
  Buffer.from(JSON.stringify(state), 'utf8').toString('base64');
const decodeState = (s: string): unknown => JSON.parse(Buffer.from(s, 'base64').toString('utf8'));

/** Validate the decoded shape so a hand-edited / malformed comment falls back to a fresh
 *  state instead of throwing later in mergeJob/renderComment. */
function isMatrixState(v: unknown): v is MatrixState {
  if (typeof v !== 'object' || v === null) return false;
  const s = v as Record<string, unknown>;
  return Array.isArray(s.jobOrder) && Array.isArray(s.testNames) && typeof s.jobs === 'object' && s.jobs !== null;
}

/** Recover the embedded state from a comment body, or null if absent/corrupt/malformed. */
export function parseState(body: string): MatrixState | null {
  const open = body.indexOf(STATE_OPEN);
  if (open < 0) return null;
  const start = open + STATE_OPEN.length;
  const end = body.indexOf(STATE_CLOSE, start);
  if (end < 0) return null;
  try {
    const decoded = decodeState(body.slice(start, end).trim());
    return isMatrixState(decoded) ? decoded : null;
  } catch {
    return null;
  }
}

const codeAt = (col: JobColumn, i: number): Code => (col.status[i] as Code) ?? '-';

// Escape a value for a Markdown table cell: `|` breaks the column, a backtick breaks the
// inline-code span, and a newline breaks the row.
const cell = (s: string): string =>
  s.replaceAll('|', String.raw`\|`).replaceAll('`', 'ˋ').replaceAll(/\r?\n/g, ' ');

/** The one-line headline: running, some-failed, or all-passed. */
function renderStatusLine(jobCount: number, totals: { p: number; f: number; s: number }): string {
  if (jobCount === 0) return '⏳ Tests running… results appear as each job finishes.';
  if (totals.f > 0)
    return `❌ **${totals.f} failed**, ${totals.p} passed, ${totals.s} skipped — ${jobCount} job(s) reported`;
  return `✅ **All ${totals.p} passed** (${totals.s} skipped) — ${jobCount} job(s) reported`;
}

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
  const statusLine = renderStatusLine(jobs.length, totals);

  let summary = '| Job | ✅ | ❌ | ⏭️ | Total |\n|---|--:|--:|--:|--:|\n';
  for (const j of jobs) {
    const c = state.jobs[j];
    summary += `| ${cell(j)} | ${c.p} | ${c.f} | ${c.s} | ${c.p + c.f + c.s} |\n`;
  }

  const colHead = `| Test | ${jobs.map(cell).join(' | ')} |\n|---|${jobs.map(() => ':-:').join('|')}|`;
  const row = (i: number): string =>
    `| \`${cell(state.testNames[i])}\` | ${jobs.map((j) => ICON[codeAt(state.jobs[j], i)]).join(' | ')} |`;

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
    `${STATE_OPEN} ${encodeState(state)} ${STATE_CLOSE}`,
  ].join('\n');
}
