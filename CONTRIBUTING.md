# Contributing to Synth1X

Bug reports, documentation improvements, and code contributions are welcome.
Use [GitHub Issues](https://github.com/yasuyuki-baba/synth1x/issues) to report a
bug or propose a feature. Check for an existing issue before opening a new one.
For a larger change, open an issue to discuss the approach before starting.

## Report a bug

- Include the Synth1X version, your operating system and CPU architecture, and
  whether you are using the standalone app or a plugin. For plugins, include
  the DAW version and format (VST3, CLAP, or AUv2).
- Describe how to reproduce the issue, what you expected, and what happened.
  For audio or performance issues, include the sample rate, buffer size,
  oversampling setting, and any relevant MIDI events.
- If a particular patch triggers the issue, include a minimal example you
  have permission to share.

## Open a pull request

1. Fork the repository and branch from `main`. Use a short, descriptive name,
   such as `fix/preset-refresh`, `feature/midi-routing`, or `docs/install-guide`.
   Write code, documentation, and commit messages in English.
2. Follow the [development guide](DEVELOPMENT.md#build-and-test) to build the
   affected targets and run the relevant tests. For changes to DSP or patch
   loading, follow the [before/after comparison procedure](docs/validation/reference-comparisons.md#beforeafter-regression-procedure).
3. In your PR, explain what changed, link the related issue if there is one,
   and list the checks you ran. For visible UI changes, include a screenshot
   when it helps reviewers see the difference.

See [AGENTS.md](AGENTS.md) for repository conventions, the [product contracts](docs/README.md)
for expected behavior, and [LICENSE](LICENSE) for the project's license.
