# Native WebView add-on

Build with `-DCUI_WEBVIEW=ON`, include `<cui/cui_webview.h>`, and link
`cui::webview`. This static add-on links the matching CUI core transitively,
including when the core is shared. Its platform browser libraries belong to
the add-on; linking only `cui::cui` does not load them.

For installed packages, request `find_package(cui REQUIRED COMPONENTS webview)`.
With pkg-config, use `cui-webview` instead of `cui`. The core keeps the layout
slot and unavailable placeholder so gallery and headless rendering still work.

See [setup, runtime requirements and checks](../../docs/integration.md#native-webview-setup)
and the [API guide](../../docs/components-more.md#web-view). The add-on shares
internal frame/app structures with CUI and must use the same version and text
shaping configuration. CMake handles those flags; the amalgamated distribution
emits separate `cui_webview.h` and `cui_webview.c` files.
