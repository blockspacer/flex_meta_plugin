# About

Plugin for [https://github.com/blockspacer/flextool](https://github.com/blockspacer/flextool)

Plugin provides rules for source code transformation and generation.

See for details [https://blockspacer.github.io/flex_docs/plugins/](https://blockspacer.github.io/flex_docs/plugins/)

## Before installation

- [installation guide](https://blockspacer.github.io/flex_docs/download/)

## Installation

```bash
export CXX=clang++-6.0
export CC=clang-6.0

# NOTE: change `build_type=Debug` to `build_type=Release` in production
# NOTE: use --build=missing if you got error `ERROR: Missing prebuilt package`
CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
    cmake -E time \
      conan create . conan/stable \
      -s build_type=Debug -s cling_conan:build_type=Release \
      --profile clang \
          -o flex_meta_plugin:enable_clang_from_conan=False \
          -e flex_meta_plugin:enable_tests=True
```
