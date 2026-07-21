# ESPHome Web Assets

`www-v2.js` is the ESPHome web server version 2 UI asset, vendored from:

```text
https://oi.esphome.io/v2/www.js
```

It is embedded by `esphome/plugins/melachaplug/melacha_config.yaml` with
`js_include` so the device web UI can load in homes without WAN internet. Keep
the `web_server.version` value and this asset in sync when updating ESPHome web
server UI assets.

