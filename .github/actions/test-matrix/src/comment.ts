// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import * as core from '@actions/core';
import * as github from '@actions/github';
import { emptyState, parseState, renderComment, runMarker, type MatrixState } from './render.js';

type Octokit = ReturnType<typeof github.getOctokit>;

const sleep = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

function runMeta(): Pick<MatrixState, 'runNumber' | 'runUrl' | 'commit'> {
  const { GITHUB_SERVER_URL, GITHUB_REPOSITORY, GITHUB_RUN_ID, GITHUB_RUN_NUMBER, GITHUB_SHA } =
    process.env;
  return {
    runNumber: GITHUB_RUN_NUMBER ?? '?',
    runUrl: `${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}`,
    commit: (GITHUB_SHA ?? '').slice(0, 7),
  };
}

async function findComment(octokit: Octokit, prNumber: number, runId: string) {
  const { owner, repo } = github.context.repo;
  const marker = runMarker(runId);
  const it = octokit.paginate.iterator(octokit.rest.issues.listComments, {
    owner,
    repo,
    issue_number: prNumber,
    per_page: 100,
  });
  for await (const { data } of it) {
    const hit = data.find((c) => c.body?.includes(marker));
    if (hit) return hit;
  }
  return undefined;
}

/**
 * Find-or-create the run's comment and apply `merge` to its state, then write it back.
 * Parallel jobs race on one comment, so retry: re-read, re-merge, re-write until our
 * write sticks (last writer with the freshest state wins).
 */
export async function upsert(opts: {
  token: string;
  runId: string;
  prNumber: number;
  merge: (state: MatrixState) => void;
}): Promise<void> {
  const octokit = github.getOctokit(opts.token);
  const { owner, repo } = github.context.repo;

  for (let attempt = 0; attempt < 8; attempt++) {
    const existing = await findComment(octokit, opts.prNumber, opts.runId);
    const state = (existing?.body && parseState(existing.body)) || emptyState(runMeta());
    opts.merge(state);
    const body = renderComment(opts.runId, state);

    if (!existing) {
      await octokit.rest.issues.createComment({ owner, repo, issue_number: opts.prNumber, body });
      return;
    }
    await octokit.rest.issues.updateComment({ owner, repo, comment_id: existing.id, body });

    const after = await findComment(octokit, opts.prNumber, opts.runId);
    if (after?.body === body) return; // our write stuck
    await sleep(200 + Math.random() * 400 * (attempt + 1)); // a concurrent job clobbered us
  }
  core.warning('test-matrix: comment update did not converge after retries.');
}
