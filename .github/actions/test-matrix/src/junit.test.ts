// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { parseJunit } from './junit.js';

test('parseJunit counts pass/fail/skip and records per-test status', () => {
  const xml = `<?xml version="1.0"?><testsuite tests="3">
    <testcase name="A.pass"/>
    <testcase name="A.fail"><failure message="boom"/></testcase>
    <testcase name="A.skip"><skipped/></testcase>
  </testsuite>`;
  const r = parseJunit(xml);
  assert.equal(r.passed, 1);
  assert.equal(r.failed, 1);
  assert.equal(r.skipped, 1);
  assert.equal(r.total, 3);
  assert.deepEqual(r.tests, { 'A.pass': 'passed', 'A.fail': 'failed', 'A.skip': 'skipped' });
});

test('parseJunit handles a <testsuites> wrapper with multiple suites', () => {
  const xml = `<testsuites>
    <testsuite><testcase name="X.a"/></testsuite>
    <testsuite><testcase name="Y.b"><failure/></testcase></testsuite>
  </testsuites>`;
  const r = parseJunit(xml);
  assert.equal(r.passed, 1);
  assert.equal(r.failed, 1);
  assert.equal(r.total, 2);
});

test('parseJunit treats <error> as failed and disabled as skipped', () => {
  const xml = `<testsuite>
    <testcase name="E.err"><error/></testcase>
    <testcase name="D.dis" status="disabled"/>
  </testsuite>`;
  const r = parseJunit(xml);
  assert.equal(r.failed, 1);
  assert.equal(r.skipped, 1);
});

test('parseJunit labels a testcase with a non-string/absent name "unknown"', () => {
  const r = parseJunit('<testsuite><testcase/></testsuite>'); // no name attribute
  assert.deepEqual(r.tests, { unknown: 'passed' });
});

test('parseJunit yields zero counts for XML with no suites or an empty suite', () => {
  assert.equal(parseJunit('<root/>').total, 0); // neither <testsuite> nor <testsuites>
  assert.equal(parseJunit('<testsuite/>').total, 0); // a suite with no <testcase>
});
