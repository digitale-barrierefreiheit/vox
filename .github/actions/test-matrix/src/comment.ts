// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import * as core from '@actions/core';
import * as github from '@actions/github';
import { emptyState, parseState, renderComment, runMarker, type MatrixState } from './render.js';

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

/** The minimal PR-comment operations `upsertWith` needs — so its logic can be unit-tested
 *  with an in-memory fake instead of a live octokit. */
export interface CommentClient {
  list(prNumber: number): Promise<{ id: number; body: string }[]>;
  create(prNumber: number, body: string): Promise<void>;
  update(commentId: number, body: string): Promise<void>;
}

/**
 * Find-or-create the run's comment and apply `merge` to its state, then write it back.
 * Parallel jobs race on one comment, so retry: re-read, re-merge, re-write until our
 * write sticks (last writer with the freshest state wins). Pure of octokit/env for tests.
 */
export async function upsertWith(
  client: CommentClient,
  opts: {
    runId: string;
    prNumber: number;
    merge: (state: MatrixState) => void;
    meta: Pick<MatrixState, 'runNumber' | 'runUrl' | 'commit'>;
    sleep?: (ms: number) => Promise<void>;
  },
): Promise<void> {
  const wait = opts.sleep ?? sleep;
  const marker = runMarker(opts.runId);
  const find = (cs: { id: number; body: string }[]) => cs.find((c) => c.body.includes(marker));

  for (let attempt = 0; attempt < 8; attempt++) {
    const existing = find(await client.list(opts.prNumber));
    const state = (existing?.body && parseState(existing.body)) || emptyState(opts.meta);
    opts.merge(state);
    const body = renderComment(opts.runId, state);

    if (!existing) {
      await client.create(opts.prNumber, body);
      return;
    }
    await client.update(existing.id, body);

    if (find(await client.list(opts.prNumber))?.body === body) return; // our write stuck
    await wait(200 + Math.random() * 400 * (attempt + 1)); // a concurrent job clobbered us
  }
  core.warning('test-matrix: comment update did not converge after retries.');
}

/** Wire `upsertWith` to the live GitHub PR-comment API. */
export async function upsert(opts: {
  token: string;
  runId: string;
  prNumber: number;
  merge: (state: MatrixState) => void;
}): Promise<void> {
  const octokit = github.getOctokit(opts.token);
  const { owner, repo } = github.context.repo;
  const client: CommentClient = {
    list: async (prNumber) => {
      const out: { id: number; body: string }[] = [];
      const it = octokit.paginate.iterator(octokit.rest.issues.listComments, {
        owner,
        repo,
        issue_number: prNumber,
        per_page: 100,
      });
      for await (const { data } of it) out.push(...data.map((c) => ({ id: c.id, body: c.body ?? '' })));
      return out;
    },
    create: async (prNumber, body) => {
      await octokit.rest.issues.createComment({ owner, repo, issue_number: prNumber, body });
    },
    update: async (commentId, body) => {
      await octokit.rest.issues.updateComment({ owner, repo, comment_id: commentId, body });
    },
  };
  await upsertWith(client, { runId: opts.runId, prNumber: opts.prNumber, merge: opts.merge, meta: runMeta() });
}
