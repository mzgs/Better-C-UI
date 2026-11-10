# Better-C-UI

Native, immediate-mode UI components for C99, based on CUI. Build desktop applications for Windows, macOS and Linux, or run the examples in a browser with WebAssembly.

## Build and run

You need a C99 compiler and CMake 3.14 or newer. See [Getting started](docs/getting-started.md) for platform dependencies and integration instructions.

```sh
git clone https://github.com/mzgs/Better-C-UI.git
cd Better-C-UI
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
./build/examples/hello
```

With Visual Studio on Windows, run `build\examples\Release\hello.exe` instead.

## Documentation

- [Documentation index](docs/README.md)
- [Tutorial](docs/tutorial.md)
- [Examples](examples/)
- [Run the documentation site locally](web/README.md)
- [Contributing](docs/CONTRIBUTING.md)
- [Report an issue](https://github.com/mzgs/Better-C-UI/issues)

## License

[MIT](LICENSE). See [THIRD_PARTY.md](THIRD_PARTY.md) for bundled dependencies and their licenses.

## Credits

This project is based on [CUI by yappologistic](https://github.com/yappologistic/CUI). Credit goes to the original authors and contributors for their work. The original copyright and license notices are preserved.
