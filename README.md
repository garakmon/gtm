# gtm

`gtm` is a (work in progress) Qt desktop tool for working with music from gen 3 [pret decomp projects](https://github.com/pret) that use the mp2k sound engine. Perhaps support for other engines and will come in the future.


## Status

This project is in active development.

What works today:
- Loading a supported project root and reading song metadata.
- Loading MIDI files referenced by the project song table.
- Semi-accurate playback.

What is still in progress:
- MIDI note and event editing.
- Full playback accuracy parity with hardware.
- See [`TODO.md`](TODO.md) for more.


## Build

See [`INSTALL.md`](INSTALL.md)


## Running

If it is your first time opening `gtm`, you should open your root decomp folder (eg.`path/to/pokeemerald`).
After a project has been successfully opened, `gtm` will attempt to reopen the most recent project and recent song from the application config.


## Repository Layout

`gtm` uses modules to organize its source code files. Generally, the categories are as follows:

- `src/app`: controller and project-loading code
- `src/sound`: sequencer, mixer, player, and song model
- `src/ui`: Qt widgets, views, and resources
- `src/util`: shared utility code, config, theming, and logging
- `src/deps`: vendored third-party code
- `media`: sample/test media checked into the repo
- `docs`: architecture and sequence diagrams
- `licenses`: copied third-party license texts that should ship with source releases


## Third-Party Components

Bundled third-party code and assets are listed in [`CREDITS.md`](CREDITS.md).

The project currently vendors:
- PortAudio
- Craig Sapp's `midifile`
- `orderedjson`, which is a fork built on `json11`
- IBM Plex Mono and IBM Plex Sans fonts
- Iconoir 7.11 svg icons


## License

`gtm` is licensed under the MIT License. See [`LICENSE`](LICENSE).

Bundled third-party components remain under their own licenses. Keep `THIRD_PARTY_NOTICES.md` and the files in `licenses/` with source or binary distributions.
