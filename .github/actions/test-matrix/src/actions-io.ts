// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

// The IO `run` consumes, assembled from an injectable dependency bundle. makeIo holds the
// adapter logic (JUnit read + warn-on-error, expected-jobs parse + comment upsert + warn-on-
// failure, the summary/log delegations) and is unit-tested with fakes; liveDeps/readInputs are
// the thin bindings to the real fs / @actions/core that index.ts wires in.

import { readFileSync } from 'node:fs';
import * as core from '@actions/core';
import { parseJunit } from './junit.js';
import { applyReport, parseExpectedJobs, type MatrixState } from './render.js';
import { upsert } from './comment.js';
import { run, type Inputs, type Io } from './report.js';
import { errorMessage } from './util.js';

/** The platform side-effects makeIo wraps, injected so every adapter is testable with fakes. */
export interface IoDeps {
  readFile: (path: string) => string;
  writeSummary: (markdown: string) => Promise<void>;
  upsert: (opts: {
    token: string;
    runId: string;
    prNumber: number;
    create: boolean;
    merge: (state: MatrixState) => void;
  }) => Promise<void>;
  getInput: (name: string) => string;
  token: string;
  runId: string;
  warn: (message: string) => void;
  info: (message: string) => void;
  fail: (message: string) => void;
}

/** Assemble the Io `run` consumes from a dependency bundle (live or faked). */
export function makeIo(deps: IoDeps): Io {
  return {
    readResults: (path) => {
      try {
        return parseJunit(deps.readFile(path));
      } catch (err) {
        deps.warn(`test-matrix: could not read ${path} (${errorMessage(err)}); skipping report.`);
        return null;
      }
    },
    writeSummary: (markdown) => deps.writeSummary(markdown),
    upsertComment: async (create, job, prNumber, result) => {
      // init seeds the expected-job set so the verdict knows when the run is complete; report
      // steps pass nothing here and inherit it from the comment's embedded state.
      const expectedJobs = parseExpectedJobs(deps.getInput('jobs'));
      try {
        await deps.upsert({
          token: deps.token,
          runId: deps.runId,
          prNumber,
          create,
          merge: (state) => applyReport(state, { create, job, result, expectedJobs }),
        });
      } catch (err) {
        deps.warn(`test-matrix: could not update the PR comment (${errorMessage(err)}).`);
      }
    },
    warn: deps.warn,
    info: deps.info,
    fail: deps.fail,
  };
}

/** The live dependencies: real fs and the @actions/core toolkit. */
export function liveDeps(): IoDeps {
  return {
    readFile: (path) => readFileSync(path, 'utf8'),
    writeSummary: async (markdown) => {
      await core.summary.addRaw(markdown, true).write();
    },
    upsert,
    getInput: (name) => core.getInput(name),
    token: core.getInput('token') || process.env.GITHUB_TOKEN || '',
    runId: process.env.GITHUB_RUN_ID ?? '',
    warn: (message) => core.warning(message),
    info: (message) => core.info(message),
    fail: (message) => core.setFailed(message),
  };
}

/** Read the action's inputs from the environment (`mode` is required). */
export function readInputs(): Inputs {
  return {
    mode: core.getInput('mode', { required: true }),
    job: core.getInput('job'),
    reportPath: core.getInput('report-path') || 'test-results.xml',
  };
}

/** The action entry: read inputs, build the live IO, and run. @p prNumber is the PR the
 *  workflow runs against (undefined off a PR) — passed in by index.ts, not defaulted here, so a
 *  test can drive the no-PR path with an explicit `undefined` and never reach the live context.
 *  run() routes its own failures to io.fail. */
export async function main(prNumber: number | undefined): Promise<void> {
  await run(readInputs(), prNumber, makeIo(liveDeps()));
}
