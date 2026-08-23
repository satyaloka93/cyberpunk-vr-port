---
type: Playbook
title: OKF maintenance and GitHub links
description: Keep the bundle current, readable, and navigable both locally and in GitHub's repository view.
tags: [okf, documentation, github, links, validation]
timestamp: 2026-08-10T07:52:00+09:00
---

# Purpose

The bundle is a current knowledge model, not a transcript or raw engineering worklog. Update a concept in place when testing changes the durable conclusion. Keep transient experiments in normal project notes until they produce a reusable result or warning.

# Writing rules

1. Keep one independently useful idea in each concept file.
2. State the current behavior first; retain old experiments only when they explain a safety boundary.
3. Prefer short headings, lists, and tables over long chronological paragraphs.
4. Update the relevant directory index and the root log in the same change.
5. Use the concept's body for evidence and caveats; avoid turning the root log into the primary documentation.

# GitHub-safe link policy

OKF v0.1 recommends bundle-root links such as `/fixes/example.md`. Those links are valid for an OKF consumer but GitHub interprets the leading slash outside the repository's `.okf/` directory. This project therefore uses document-relative links:

```markdown
[Neighbor](other.md)
[Other section](../fixes/example.md)
[Repository source](../../src/vr/core/vr_core.cpp)
```

Use full URLs only for external projects, releases, and citations. Relative repository links automatically follow the branch being viewed and avoid hardcoded GitHub owner/branch paths.

# Link repair and validation

From the repository root:

```bash
python3 scripts/check_okf_links.py --fix-root-links
python3 scripts/check_okf_links.py
```

The first command converts resolvable `/...` destinations to paths relative to each document. The second fails on GitHub-unsafe root links, missing local targets, or links that escape the repository.

Then run the deterministic OKF v0.1 validator in strict mode. Link correctness is an additional publication rule for this repository; the OKF specification itself permits broken links.

# Maintenance checklist

1. Read `.okf/index.md` and the concepts relevant to the change.
2. Amend stale claims and timestamps.
3. Add or repair cross-links and indexes.
4. Add one concise `**Maintenance**` entry to `.okf/log.md`.
5. Run the link checker and strict OKF validator.
6. Keep OKF changes reviewable and separate from unrelated source changes when practical.
