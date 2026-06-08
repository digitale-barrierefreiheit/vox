// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { XMLParser } from 'fast-xml-parser';

export type Status = 'passed' | 'failed' | 'skipped';

/** A single job's parsed results: per-test status plus counts. */
export interface ParsedJob {
  passed: number;
  failed: number;
  skipped: number;
  total: number;
  /** test name -> status */
  tests: Record<string, Status>;
}

const parser = new XMLParser({
  ignoreAttributes: false,
  attributeNamePrefix: '@_',
  // CTest emits a single <testsuite>; some tools wrap in <testsuites>. Force arrays so a
  // single suite/case parses the same as many.
  isArray: (name) => name === 'testsuite' || name === 'testcase',
});

/** Classify one `<testcase>`: a <failure>/<error> child is failed, <skipped>/disabled is
 *  skipped, otherwise passed. */
function classify(tc: Record<string, unknown>): Status {
  if (tc.failure !== undefined || tc.error !== undefined) return 'failed';
  if (tc.skipped !== undefined || tc['@_status'] === 'disabled') return 'skipped';
  return 'passed';
}

/** The testcase name attribute, defended against a non-string parse (avoids stringifying
 *  an object to "[object Object]"). */
function nameOf(tc: Record<string, unknown>): string {
  const raw = tc['@_name'];
  if (typeof raw === 'string') return raw;
  if (typeof raw === 'number') return String(raw);
  return 'unknown';
}

/** Parse a CTest (`--output-junit`) JUnit XML string into one job's results. */
export function parseJunit(xml: string): ParsedJob {
  const doc = parser.parse(xml) as Record<string, any>;
  const suites: any[] = doc.testsuite ?? doc.testsuites?.testsuite ?? [];
  const tests: Record<string, Status> = {};
  const counts: Record<Status, number> = { passed: 0, failed: 0, skipped: 0 };

  for (const suite of suites) {
    for (const tc of (suite.testcase ?? []) as Record<string, unknown>[]) {
      const status = classify(tc);
      tests[nameOf(tc)] = status;
      counts[status]++;
    }
  }
  return { ...counts, total: counts.passed + counts.failed + counts.skipped, tests };
}
