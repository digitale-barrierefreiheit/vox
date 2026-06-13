// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

// Tiny entry point: wire the real GitHub Actions IO to `run` (the tested logic in
// report.ts) and surface any uncaught error as a failed step.

import { readFileSync } from 'node:fs';
import * as core from '@actions/core';
import * as github from '@actions/github';
import { parseJunit } from './junit.js';
import { applyReport, parseExpectedJobs } from './render.js';
import { upsert } from './comment.js';
import { run, type Io } from './report.js';

const io: Io = {
  readResults: (path) => {
    try {
      return parseJunit(readFileSync(path, 'utf8'));
    } catch (err) {
      core.warning(`test-matrix: could not read ${path} (${err instanceof Error ? err.message : err}); skipping report.`);
      return null;
    }
  },
  writeSummary: async (markdown) => {
    await core.summary.addRaw(markdown, true).write();
  },
  upsertComment: async (create, job, prNumber, result) => {
    // init seeds the expected-job set so the verdict knows when the run is complete; report
    // steps pass nothing here and inherit it from the comment's embedded state.
    const expectedJobs = parseExpectedJobs(core.getInput('jobs'));
    try {
      await upsert({
        token: core.getInput('token') || process.env.GITHUB_TOKEN || '',
        runId: process.env.GITHUB_RUN_ID ?? '',
        prNumber,
        create,
        merge: (state) => applyReport(state, { create, job, result, expectedJobs }),
      });
    } catch (err) {
      core.warning(`test-matrix: could not update the PR comment (${err instanceof Error ? err.message : err}).`);
    }
  },
  warn: (message) => core.warning(message),
  info: (message) => core.info(message),
  fail: (message) => core.setFailed(message),
};

const inputs = {
  mode: core.getInput('mode', { required: true }),
  job: core.getInput('job'),
  reportPath: core.getInput('report-path') || 'test-results.xml',
};

try {
  await run(inputs, github.context.payload.pull_request?.number, io);
} catch (err) {
  core.setFailed(err instanceof Error ? err.message : String(err));
}
