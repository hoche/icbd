# icbd - The ICB Server

_**NOTE: HEAD builds with CMake.**_

ICB (Internet Citizen's Band) is a renaming of Sean Casey's original
ForumNet (or fn) program. Much of the code here-in is Sean's, though
considerable bug-fixing and extensions have been made over the
years such that it may be somewhat unrecognizable from the original.
John Atwood deVries and Rich Dellaripa performed some significant
changes from Sean's sources. Jon Luini then took over as the main
maintainer of it around 1995 and made numerous bugfixes over the next
several years. In May of 2000, Michel Hoche-Mong performed some
organizational clean-up, created the autoconf files, added SSL handling,
and made a variety of bugfixes. In May of 2019, the code was imported
into Michel's github home at https://github.com/hoche/icbd.

All of the changes from the time Jon Luini took it over (approx 1995)
up to the time it was imported into github (v1.2c) are noted in the file
README.CHANGEHIST.

Since then:
  * UTF-8 support has been added for messages (but not usernames or groupnames).
  * The build system has been updated (CMake).
  * "brick" has been added (feature request)
  * TLS support has been added.

Building and installation instructions can be found in README.INSTALL.

TODO.md is a ToDo list for the developers.

General ICB resources are maintained at http://www.icb.net/

## Build (CMake)

### Dependencies

- **Required**: `gdbm` development headers/library (e.g. `libgdbm-dev`)
- **Optional (TLS listener)**: OpenSSL development package (e.g. `libssl-dev`)
- **For integration tests**: Python 3

### Configure + build (non-TLS)

`icbd` requires a compiled-in admin password at configure time:

```bash
cmake -S . -B build -DADMIN_PWD=change_me
cmake --build build -j
```

The server binary will be at `build/server/icbd`.

## Test suite

This repo uses **CTest** (unit tests in C, integration tests in Python that start `icbd` and speak the ICB protocol over TCP).

### Run tests (non-TLS)

```bash
cmake -S . -B build-test -DADMIN_PWD=test
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

### Run tests (with TLS enabled)

Configure with TLS support and run the suite. TLS integration tests will only be registered when TLS is compiled in.

```bash
cmake -S . -B build-test-tls -DADMIN_PWD=test -DICBD_ENABLE_SSL=ON
cmake --build build-test-tls -j
ctest --test-dir build-test-tls --output-on-failure
```

Notes:

- Integration tests create a temporary runtime directory and copy `prod/` fixtures into it.
- TLS integration tests generate a short-lived self-signed `icbd.pem` via `openssl` (so `openssl` must be available in `PATH`).
