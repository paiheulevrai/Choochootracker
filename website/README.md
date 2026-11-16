# ChooChooTracker website

English presentation site, built with Astro. Fira Sans is bundled locally; no
font service, analytics or audio/video player is loaded by the presentation pages.
The tracker only loads when visiting the app.

## Preview locally

Requires Node.js 20.19+ and npm. From this directory:

```sh
npm ci
npm run dev
```

Open <http://127.0.0.1:4321/Choochootracker/>. This is the single editing server:
Markdown, templates and CSS update live, including screenshots, the app and guides.
Edit only `src/`; never edit `dist/` or `.generated-public/`, which are generated.
Static assets are copied from the repository when the server starts. Restart it
if the tracker bundle or guide images change. The tracker is not recompiled.
The fixed port fails when already occupied instead of opening another version.
With the server running, `node scripts/check-live.mjs` checks the live pages,
app, guides and static assets.

For a production check, stop the editing server first, then run:

```sh
npm run build
npm run check
npm run preview
```

Preview uses the same URL and displays the last build. Restart `npm run dev`
to resume editing; Markdown changes do not update a production preview.

## Edit the content

- [Home introduction](src/content/home.md): headline, opening text and hero image.
- [Engine descriptions](src/content/featured-engines.md): all engine cards, their
  descriptions and optional screenshots. This is the homepage engine content.
- [Sampling](src/content/03-sampling.md) and [effects](src/content/04-effects.md):
  section headings and text, including the four-slot instrument modulation
  matrix (envelopes, LFOs and performance sources), followed by track-to-track
  audio modulation and its vocoder screenshot. Workflow steps are in the home introduction.
- `01-workflow.md` and `02-engines.md` are older drafts, not rendered on the site.
- [Project story](src/content/about.md), [downloads](src/content/downloads.md),
  [learning introduction](src/content/learn.md).

The block between `---` lines contains the title and image metadata; the text
below it is Markdown. Screenshot names refer to the
[field guide assets](../docs/user-guide/assets). Use descriptive imageAlt text
and keep screenshots at their original 640 × 480 resolution. All field guide assets are copied automatically by
[the asset copy script](scripts/copy-assets.mjs).

Homepage cards use their own images in
[`src/assets/homepage-screenshots/`](src/assets/homepage-screenshots/).
The native UI harness's `--capture-homepage` mode produces the captures,
including Fatty, DigiSophie and the track-to-track vocoder. See the
[build notes](../docs/build-notes.md) for the Windows harness.
Convert only the required BMPs to lossless 640 × 480 PNGs; retain the other
source images. Fatty uses `IDEColorThemes/GruvLight.cth`, DigiSophie uses
`IDEColorThemes/MatchaDark.cth`, and the vocoder uses
`IDEColorThemes/TokyoNightDark.cth`, distinct from the existing homepage themes.

The `unifont` dependency is held at 0.7.4 because 0.7.5 pulls in Undici 8,
which requires Node 22.19. The website does not use Astro's experimental
remote font provider; Fira Sans is bundled from Fontsource.

[Page layout](src/layouts/Page.astro) contains navigation and footer labels.
[Styles](src/styles/site.css) contain the anthracite/orange palette and
responsive layout. The [homepage template](src/pages/index.astro) contains
button labels, the feature strip and guide links.

## Cloudflare Pages

The Cloudflare Pages build uses the root URL and targets
`https://choochootracker.pages.dev` for canonical links. Outside Cloudflare,
the local preview keeps the repository subpath.

Create a Pages project from the GitHub repository and set:

- Production branch: `main`
- Root directory: `website`
- Build command: `npm ci && npm run build && npm run check`
- Build output directory: `dist`
- Project name: `choochootracker` (for `choochootracker.pages.dev`, if available)

Cloudflare Pages then deploys each push to `main`. The Node version is pinned by
[`website/.node-version`](.node-version). Rebuild after updating the checked-in
`web/dist/` app bundle or guide assets.

The homepage title is defined in [the homepage template](src/pages/index.astro).
Its description and portable-platform introduction are in
[home.md](src/content/home.md). The shared layout emits the `ChooChooTracker`
Open Graph site name and homepage `WebSite` structured data; this identifies
the project rather than its Cloudflare host. Google can choose a different
displayed site name and may retain an older label until recrawling.
To validate production metadata locally without rebuilding WebAssembly, run
`CF_PAGES=1 npm run build` followed by `CF_PAGES=1 npm run check`
(PowerShell: set `$env:CF_PAGES='1'` before both commands).

Downloads currently link to the releases list, which includes prereleases.
GitHub's `/releases/latest` shortcut excludes prereleases, so it is unsuitable
as the sole alpha download link. No version number is hard-coded here.

Browser projects do not automatically migrate from the Vercel origin; the
downloads page explains export/import. The existing Vercel deployment is not
changed by this setup.

## Manual theme

The [manual page](src/pages/manual.astro) renders the canonical
[USER_MANUAL.md](../docs/USER_MANUAL.md) with the same [Page layout](src/layouts/Page.astro),
fonts and [site theme](src/styles/site.css) as the presentation pages.
[Manual CSS](src/styles/manual.css) only controls document and contents layout;
use the shared theme variables for colours. Do not copy the standalone app manual
into the website. The asset script creates compatibility redirects for
`manual.html` and `play/manual.html`, preserving section anchors.

The [web app page](src/pages/play.astro) also uses the shared page layout. It
embeds the existing tracker in a same-origin frame, preserving browser storage
and keeping tracker themes and keyboard handlers separate from site navigation.
The asset script generates `play/runtime.html`; edit the original web wrapper
in `web/index.html` for tracker controls, not that generated file.
