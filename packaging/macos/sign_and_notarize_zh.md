# macOS 签名、公证与发布检查

本文件给维护者记录正式 macOS 发布流程。未签名 Beta 可以用于内部验证，但公开发布应完成 `codesign`、Apple notary service 公证、`stapler` 装订和 Gatekeeper 验证。

## 前置条件

- Apple Developer ID Application 证书。
- App Store Connect API key，或已配置的 Apple ID notarytool 凭据。
- 干净构建目录和已通过的本地测试。

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DragNDrop
```

## 签名 app

```bash
codesign --force --deep --options runtime \
  --sign "Developer ID Application: <Team Name> (<TEAMID>)" \
  "build/_CPack_Packages/Darwin/DragNDrop/trx_vanity-1.0.0-Darwin-*/TRX Vanity.app"

codesign --verify --deep --strict --verbose=2 \
  "build/_CPack_Packages/Darwin/DragNDrop/trx_vanity-1.0.0-Darwin-*/TRX Vanity.app"
```

## 公证 DMG

```bash
xcrun notarytool submit "build/trx_vanity-1.0.0-Darwin-$(uname -m).dmg" \
  --keychain-profile "trx-vanity-notary" \
  --wait

xcrun stapler staple "build/trx_vanity-1.0.0-Darwin-$(uname -m).dmg"
xcrun stapler validate "build/trx_vanity-1.0.0-Darwin-$(uname -m).dmg"
```

## Gatekeeper 验证

在 Downloads 和 Applications 两个位置都验证：

```bash
spctl --assess --type open --context context:primary-signature \
  --verbose "build/trx_vanity-1.0.0-Darwin-$(uname -m).dmg"

spctl --assess --type execute --verbose \
  "/Applications/TRX Vanity.app"
```

## SHA256 与发布记录

```bash
shasum -a 256 "build/trx_vanity-1.0.0-Darwin-$(uname -m).dmg"
```

Release notes 必须记录：

- 版本号、平台和架构。
- 构建命令与测试结果。
- SHA256。
- 签名、公证和 staple 状态。
- 已知问题和安全说明。

不要把 Apple API key、notarytool 凭据、私钥、token、`.env` 或真实结果文件写入 release、日志、issue、PR 或 Telegram。
