<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors -->

# Vox brand assets

| File | What it is |
|------|-----------|
| [`vox-logo.svg`](vox-logo.svg) | Full logo lockup — the symbol/mark plus the "vox" wordmark, transparent background. Use in documentation. |
| [`vox-mark.svg`](vox-mark.svg) | The symbol/mark only (no wordmark), cropped square. Source for the application icon and favicons. |

## Colors

| | Hex |
|---|---|
| Dark | `#494948` |
| Yellow | `#F8EA1B` |
| Blue | `#0B36FB` |

## Provenance & license

The Vox logo was generated with [Recraft.AI](https://www.recraft.ai/) under a **paid
subscription**. Under Recraft's paid-tier Terms of Service the subscriber **owns the generated
Assets** — Recraft assigns its copyright in them to the subscriber — for commercial use, with a
single restriction: **the Assets may not be used to train AI models**. On that basis the logo is
used and redistributed here, including inside the release artifacts.

- The one restriction to honour: never use the logo (or a derivative) as AI **training** data.
- This file records the logo's **AI-generated** provenance — for honesty and to support
  AI-content transparency expectations (e.g. the EU AI Act). Keep that disclosure.
- A purely AI-generated image may carry thin or no copyright (no human authorship) in some
  jurisdictions, so no strong exclusivity is asserted — consistent with the trademark note below.

"Vox" is the Latin word for "voice". It is **not** a registered trademark, and no trademark claim
is made.

## Regenerating the application icon

The multi-resolution Windows icon is built from `vox-mark.svg`. With `librsvg2-bin` and
`imagemagick` installed (Linux/WSL):

```sh
# one-liner (ImageMagick delegates SVG rendering to librsvg):
convert -background none vox-mark.svg -define icon:auto-resize=256,128,64,48,32,24,16 vox.ico
# ImageMagick 7: use `magick` instead of `convert`.
```

> Note on dark backgrounds: the mark and wordmark use a dark grey (`#494948`). On a dark page
> (e.g. GitHub dark mode) those elements have low contrast. If a dark-mode logo variant is needed,
> add a light recolouring and select it with a `<picture>` / `prefers-color-scheme` switch.
