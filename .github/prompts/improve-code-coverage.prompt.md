---
name: improve-code-coverage
description: "Increase code coverage by adding tests only, and continue in batches until branch coverage is strictly greater than 70%."
disable-model-invocation: true
---

# Improve Code Coverage (Branch Coverage > 70%)

You are working in the `alx-home/promise` repository.

## Objective

Increase code coverage by adding tests only, and continue in batches until **branch coverage is strictly greater than 70%**.

## Mandatory Workflow (do not reorder)

1. Run the VS Code task **`Run act PR`**.
2. Wait until **`Run act PR`** fully completes.
3. Run the VS Code task **`Generate Coverage`**.
4. Read coverage outputs generated under `build/gcov`.
5. Identify the lowest-covered branches/lines first.
6. Add/adjust tests to target those low-coverage branches/lines, prioritizing the worst offenders.
7. Build and run tests using the CMake actions:
	- use CMake build action
	- use CMake test action
8. If a newly added test fails and appears to reveal a library bug (not a bad test assertion), leave the test in place but commented out with a `@fixme` comment explaining why it appears to be a bug.
9. Repeat from step 1 for a new batch.

## Hard Constraints

- Do **not** remove, disable, or alter gcov/coverage options or generated coverage file flow.
- Focus only on creating or adjusting tests to increase coverage.
- Prefer smallest meaningful test batches that improve branch coverage in measurable steps.
- Never stop early: continue iterating until branch coverage is **> 70%**.

## Coverage Inputs to Inspect

Use files in `build/gcov` (as available), including:

- `build/gcov/coverage.txt`
- `build/gcov/coverage.json`
- `build/gcov/coverage.xml`
- `build/gcov/summary.txt`
- `build/gcov/sonar-coverage.xml`

## Test Placement Guidance

- Add tests in the existing test tree under `test/`.
- Prefer extending focused suites already covering related behavior.
- Target untested conditional paths, error/edge branches, and alternate resolver/reject/chain flows.

## Reporting Per Batch

For each batch, report:

1. Which files/functions/branches were targeted.
2. What tests were added or updated.
3. Build/test result from CMake actions.
4. Updated branch coverage and delta from previous batch.
5. Any `@fixme` commented-out tests and rationale.

Terminate only when branch coverage is above 70%.
