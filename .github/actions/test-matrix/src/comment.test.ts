// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { upsertWith, type CommentClient } from './comment.js';
import { mergeJob, parseState, type MatrixState } from './render.js';
import type { ParsedJob } from './junit.js';

const meta = { runNumber: '1', runUrl: 'https://x/1', commit: 'abc1234' };
const noSleep = (): Promise<void> => Promise.resolve();
const pass = (name: string): ParsedJob => ({ passed: 1, failed: 0, skipped: 0, total: 1, tests: { [name]: 'passed' } });
const fold = (job: string, name: string) => (s: MatrixState): void => mergeJob(s, job, pass(name));

/** A programmable in-memory PR-comment store. */
function fakeClient(seed: { id: number; body: string }[] = []) {
  const comments = seed.map((c) => ({ ...c }));
  let nextId = 100;
  const calls = { list: 0, create: 0, update: 0 };
  const client: CommentClient = {
    list: async () => {
      calls.list++;
      return comments.map((c) => ({ ...c }));
    },
    create: async (_pr, body) => {
      calls.create++;
      comments.push({ id: nextId++, body });
    },
    update: async (id, body) => {
      calls.update++;
      const c = comments.find((x) => x.id === id);
      if (c) c.body = body;
    },
  };
  return { client, comments, calls };
}

test('init creates the run comment when none exists', async () => {
  const { client, comments, calls } = fakeClient();
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: true, merge: fold('x64', 'A.a') });
  assert.equal(calls.create, 1);
  assert.equal(comments.length, 1);
  assert.match(comments[0].body, /run=r1/);
  assert.deepEqual(parseState(comments[0].body)?.jobOrder, ['x64']);
});

test('a report (create=false) updates the same comment to fold in its column', async () => {
  const { client, comments } = fakeClient();
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: true, merge: fold('x64', 'A.a') });
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: false, merge: fold('tsan', 'A.a') });
  assert.equal(comments.length, 1);
  assert.deepEqual(parseState(comments[0].body)?.jobOrder, ['x64', 'tsan']);
});

test('a report (create=false) never creates, even after retrying — avoids the create race', async () => {
  const { client, comments, calls } = fakeClient();
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: false, merge: fold('x64', 'A.a') });
  assert.equal(calls.create, 0);
  assert.equal(comments.length, 0); // init never created one; the report gives up after retries
  assert.ok(calls.list >= 2); // it polled rather than skipping on the first miss
});

test('a report waits for a late init comment instead of skipping', async () => {
  const comments: { id: number; body: string }[] = [];
  let list = 0;
  const client: CommentClient = {
    list: async () => {
      list++;
      if (list === 2) comments.push({ id: 1, body: '<!-- vox-test-matrix run=r1 -->' }); // init finishes late
      return comments.map((c) => ({ ...c }));
    },
    create: async () => assert.fail('a report must not create'),
    update: async (id, body) => {
      const c = comments.find((x) => x.id === id);
      if (c) c.body = body;
    },
  };
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: false, merge: fold('x64', 'A.a') });
  assert.ok(parseState(comments[0].body)?.jobOrder.includes('x64')); // found the late comment and folded in
});

test('a different run id gets its own comment', async () => {
  const seed = [{ id: 1, body: '<!-- vox-test-matrix run=OLD -->\nstale' }];
  const { client, comments, calls } = fakeClient(seed);
  await upsertWith(client, { runId: 'NEW', prNumber: 1, meta, sleep: noSleep, create: true, merge: fold('x64', 'A.a') });
  assert.equal(calls.create, 1);
  assert.equal(comments.length, 2);
});

test('retries when a concurrent write clobbers ours', async () => {
  const comments = [{ id: 1, body: '<!-- vox-test-matrix run=r1 -->' }];
  let list = 0;
  const client: CommentClient = {
    list: async () => {
      list++;
      // The verify-read right after our first update shows a rival's clobber, forcing a retry.
      if (list === 2) return [{ id: 1, body: 'CLOBBERED by a rival job' }];
      return comments.map((c) => ({ ...c }));
    },
    create: async () => assert.fail('should update, not create'),
    update: async (id, body) => {
      const c = comments.find((x) => x.id === id);
      if (c) c.body = body;
    },
  };
  await upsertWith(client, { runId: 'r1', prNumber: 1, meta, sleep: noSleep, create: false, merge: fold('x64', 'A.a') });
  assert.ok(list >= 4); // attempt 1 (list+verify) clobbered -> attempt 2 (list+verify) sticks
  assert.ok(parseState(comments[0].body)?.jobOrder.includes('x64'));
});
