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

/** Parse a CTest (`--output-junit`) JUnit XML string into one job's results. */
export function parseJunit(xml: string): ParsedJob {
  const doc = parser.parse(xml) as Record<string, any>;
  const suites: any[] = doc.testsuite ?? doc.testsuites?.testsuite ?? [];
  const tests: Record<string, Status> = {};
  let passed = 0;
  let failed = 0;
  let skipped = 0;

  for (const suite of suites) {
    for (const tc of (suite.testcase ?? []) as any[]) {
      const name = String(tc['@_name'] ?? 'unknown');
      let status: Status;
      if (tc.failure !== undefined || tc.error !== undefined) {
        status = 'failed';
        failed++;
      } else if (tc.skipped !== undefined || tc['@_status'] === 'disabled') {
        status = 'skipped';
        skipped++;
      } else {
        status = 'passed';
        passed++;
      }
      tests[name] = status;
    }
  }
  return { passed, failed, skipped, total: passed + failed + skipped, tests };
}
