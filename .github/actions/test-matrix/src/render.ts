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
  /** The job ran but published no JUnit (e.g. its build failed first) — counts as not
   *  passing, so a missing report can never leave the run reading as all-green. */
  u?: boolean;
}

/** The full run state, embedded (as JSON) in the comment so every job can rebuild it. */
export interface MatrixState {
  runNumber: string;
  runUrl: string;
  commit: string;
  /** Every test job expected to report this run, seeded by the init step. The overall
   *  verdict is ✅ only once all of these have reported and none failed; until then the
   *  summary stays ⏳ so a partial run never reads as all-passed. */
  expectedJobs: string[];
  jobOrder: string[];
  testNames: string[];
  jobs: Record<string, JobColumn>;
}

export function emptyState(
  meta: Pick<MatrixState, 'runNumber' | 'runUrl' | 'commit'>,
  expectedJobs: string[] = [],
): MatrixState {
  return { ...meta, expectedJobs, jobOrder: [], testNames: [], jobs: {} };
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

/** Record a job that ran but produced no JUnit (its build failed, or the runner died before
 *  tests): an empty column flagged `u`, so the verdict treats it as not-passing rather than
 *  simply absent (which would otherwise read as "still waiting" forever). */
export function mergeUnavailable(state: MatrixState, job: string): void {
  const status = state.testNames.map(() => '-').join('');
  state.jobs[job] = { p: 0, f: 0, s: 0, status, u: true };
  if (!state.jobOrder.includes(job)) state.jobOrder.push(job);
}

/** Apply one action invocation to the state: `init` seeds the expected-job set; a `report`
 *  folds in this job's column, or marks it unavailable when it produced no JUnit. Pure, so
 *  index.ts's merge closure stays a one-liner and this decision is unit-tested directly. */
export function applyReport(
  state: MatrixState,
  opts: { create: boolean; job: string; result: ParsedJob | null; expectedJobs: string[] },
): void {
  if (opts.create) {
    state.expectedJobs = opts.expectedJobs;
  } else if (opts.result) {
    mergeJob(state, opts.job, opts.result);
  } else {
    mergeUnavailable(state, opts.job);
  }
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

const isStringArray = (v: unknown): boolean => Array.isArray(v) && v.every((x) => typeof x === 'string');

function isJobColumn(v: unknown): boolean {
  if (typeof v !== 'object' || v === null) return false;
  const c = v as Record<string, unknown>;
  return (
    typeof c.p === 'number' &&
    typeof c.f === 'number' &&
    typeof c.s === 'number' &&
    typeof c.status === 'string' &&
    (c.u === undefined || typeof c.u === 'boolean')
  );
}

function hasStringMeta(s: Record<string, unknown>): boolean {
  return typeof s.runNumber === 'string' && typeof s.runUrl === 'string' && typeof s.commit === 'string';
}

/** Every job column must be well-formed, carry one status code per test name, and use only
 *  the known codes (P/F/S/-) so codeAt/ICON never render an undefined cell. */
function columnsValid(jobs: unknown, testCount: number): boolean {
  if (typeof jobs !== 'object' || jobs === null) return false;
  const columns = Object.values(jobs);
  if (!columns.every(isJobColumn)) return false;
  return columns.every((c) => {
    const { status } = c as JobColumn;
    return status.length === testCount && /^[PFS-]*$/.test(status);
  });
}

/** Validate the decoded shape (meta strings, string arrays, well-formed and aligned job
 *  columns) so a hand-edited / malformed comment reliably falls back to a fresh state
 *  instead of throwing later in mergeJob/renderComment or rendering a confusing header. */
function isMatrixState(v: unknown): v is MatrixState {
  if (typeof v !== 'object' || v === null) return false;
  const s = v as Record<string, unknown>;
  return (
    hasStringMeta(s) &&
    isStringArray(s.expectedJobs) &&
    isStringArray(s.jobOrder) &&
    isStringArray(s.testNames) &&
    columnsValid(s.jobs, (s.testNames as string[]).length) &&
    (s.jobOrder as string[]).every((j) => Object.hasOwn(s.jobs as object, j))
  );
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
    // Back-fill expectedJobs for a comment written before the field existed, so a run that
    // is in flight across a deploy still parses (and merges) instead of resetting mid-run.
    if (decoded && typeof decoded === 'object' && (decoded as Record<string, unknown>).expectedJobs === undefined) {
      (decoded as Record<string, unknown>).expectedJobs = [];
    }
    return isMatrixState(decoded) ? decoded : null;
  } catch {
    return null;
  }
}

const codeAt = (col: JobColumn, i: number): Code => (col.status[i] as Code) ?? '-';

// Plain Markdown table cell (job labels): `|` breaks the column, newlines break the row.
const cell = (s: string): string => s.replaceAll('|', String.raw`\|`).replaceAll(/\r?\n/g, ' ');

// A faithful code span for a test name: escape the table-breaking chars, then fence with
// more backticks than any run inside it so the real name (backticks and all) displays as-is.
export const codeCell = (s: string): string => {
  const safe = cell(s);
  const longest = (safe.match(/`+/g) ?? []).reduce((max, run) => Math.max(max, run.length), 0);
  const fence = '`'.repeat(longest + 1);
  const pad = safe.startsWith('`') || safe.endsWith('`') ? ' ' : '';
  return `${fence}${pad}${safe}${pad}${fence}`;
};

/** The one-line headline. ✅ is reserved for a fully-reported, all-passed run: every expected
 *  job has reported and none failed. ❌ the moment any job fails or comes back without results.
 *  ⏳ while still waiting on expected jobs — so a partial run is never shown as all-green. */
function renderStatusLine(state: MatrixState, totals: { p: number; f: number; s: number }): string {
  const reported = state.jobOrder.length;
  const failed = state.jobOrder.filter((j) => state.jobs[j].f > 0 || state.jobs[j].u);
  // The gate is set-based: every *expected* job must have reported. A count alone would pass
  // if an unexpected job stood in for a missing expected one.
  const pending = state.expectedJobs.filter((j) => !state.jobOrder.includes(j));
  // Completeness is enforced only when the expected set was seeded; an empty set (e.g. a
  // legacy comment) falls back to "all reported jobs", so the denominator is the live count.
  const seeded = state.expectedJobs.length > 0;
  const total = seeded ? state.expectedJobs.length : reported;
  const done = total - pending.length;

  if (failed.length > 0) {
    const noResults = failed.filter((j) => state.jobs[j].u);
    const parts: string[] = [];
    if (totals.f > 0) parts.push(`**${totals.f} failed**`);
    if (noResults.length > 0) parts.push(`**${noResults.length} job(s) without results** (${noResults.map(cell).join(', ')})`);
    return `❌ ${parts.join(', ')}, ${totals.p} passed, ${totals.s} skipped — ${done}/${total} jobs reported`;
  }
  if (reported === 0) return '⏳ Tests running… results appear as each job finishes.';
  if (pending.length > 0)
    return `⏳ ${totals.p} passed, ${totals.s} skipped so far — ${done}/${total} jobs reported (waiting for ${pending.map(cell).join(', ')})`;
  return `✅ **All ${totals.p} passed** (${totals.s} skipped) — ${total} jobs reported`;
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
  const statusLine = renderStatusLine(state, totals);

  let summary = '| Job | ✅ | ❌ | ⏭️ | Total |\n|---|--:|--:|--:|--:|\n';
  for (const j of jobs) {
    const c = state.jobs[j];
    summary += c.u
      ? `| ${cell(j)} | — | ⚠️ | — | no results |\n`
      : `| ${cell(j)} | ${c.p} | ${c.f} | ${c.s} | ${c.p + c.f + c.s} |\n`;
  }

  const colHead = `| Test | ${jobs.map(cell).join(' | ')} |\n|---|${jobs.map(() => ':-:').join('|')}|`;
  const row = (i: number): string =>
    `| ${codeCell(state.testNames[i])} | ${jobs.map((j) => ICON[codeAt(state.jobs[j], i)]).join(' | ')} |`;

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
