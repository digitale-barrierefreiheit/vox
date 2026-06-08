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

type Comment = { id: number; body: string };

const findByMarker = (comments: Comment[], marker: string): Comment | undefined =>
  comments.find((c) => c.body.includes(marker));

/** Recover the run's state from its comment, or a fresh one if absent/unparseable. */
function stateFrom(
  existing: Comment | undefined,
  meta: Pick<MatrixState, 'runNumber' | 'runUrl' | 'commit'>,
): MatrixState {
  return (existing?.body && parseState(existing.body)) || emptyState(meta);
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
    /** Only the init step creates the comment; report steps update-only (see below). */
    create: boolean;
    sleep?: (ms: number) => Promise<void>;
  },
): Promise<void> {
  const wait = opts.sleep ?? sleep;
  const marker = runMarker(opts.runId);

  for (let attempt = 0; attempt < 8; attempt++) {
    const existing = findByMarker(await client.list(opts.prNumber), marker);

    // Only the init step may create. Report steps run in parallel, so if they could create
    // too, two racing jobs (both seeing no comment) would each create one and break the
    // "one comment per run" guarantee. Init runs first (at the start of the run), so by the
    // time reports run the comment exists; if it doesn't, init was skipped — skip quietly.
    if (!existing && !opts.create) {
      core.warning('test-matrix: run comment not found (init step skipped?); skipping update.');
      return;
    }

    const state = stateFrom(existing, opts.meta);
    opts.merge(state);
    const body = renderComment(opts.runId, state);

    if (!existing) {
      await client.create(opts.prNumber, body);
      return;
    }
    await client.update(existing.id, body);

    if (findByMarker(await client.list(opts.prNumber), marker)?.body === body) return; // our write stuck
    await wait(200 + Math.random() * 400 * (attempt + 1)); // a concurrent job clobbered us
  }
  core.warning('test-matrix: comment update did not converge after retries.');
}

/** Wire `upsertWith` to the live GitHub PR-comment API. */
export async function upsert(opts: {
  token: string;
  runId: string;
  prNumber: number;
  create: boolean;
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
  await upsertWith(client, {
    runId: opts.runId,
    prNumber: opts.prNumber,
    merge: opts.merge,
    meta: runMeta(),
    create: opts.create,
  });
}
