# macOS 签名、公证与发布检查

本文件给维护者记录正式 macOS 发布流程。未签名 Beta 可以用于内部验证，但公开发布应完成 `codesign`、Apple notary service 公证、`stapler` 装订和 Gatekeeper 验证。

> 安全红线：Apple API key、notarytool 凭据、Developer ID 证书私钥、TRX 私钥、`.env`、token 和真实结果文件都不得写入 release、日志、issue、PR、Telegram 或任何远程服务。

## 前置条件

- Apple Developer ID Application 证书已安装在本机 Keychain。
- App Store Connect API key，或已通过 `xcrun notarytool store-credentials` 保存的 keychain profile。
- 干净构建目录和已通过的本地测试。
- 在隔离的发布机器/临时目录中操作；不要把用户结果 CSV 或私钥文件放进仓库、构建目录或 DMG staging 目录。

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# 未签名包 smoke；用于确认 CPack 布局，不作为正式公开发布物。
cpack --config build/CPackConfig.cmake -G DragNDrop
```

## 1. 准备可签名 staging 目录

CPack smoke 通过后，使用 `cmake --install` 生成一个干净 staging，再对 staging 内的 `.app` 签名。这样可以避免先打包 DMG、后修改 app 导致 DMG 内仍是未签名内容。

```bash
rm -rf build/macos-release-stage build/TRX-Vanity-signed.dmg
cmake --install build --prefix build/macos-release-stage

APP="build/macos-release-stage/TRX Vanity.app"
CLI="$APP/Contents/Resources/bin/trx_vanity"
IDENTITY="Developer ID Application: <Team Name> (<TEAMID>)"
```

## 2. 签名 app 内所有可执行文件

先签名嵌套 CLI，再签名外层 app。当前 launcher 不需要特殊 entitlements；如未来引入 hardened runtime 例外，必须在 release notes 中说明原因。

```bash
codesign --force --options runtime --timestamp \
  --sign "$IDENTITY" "$CLI"

codesign --force --options runtime --timestamp \
  --sign "$IDENTITY" "$APP"

codesign --verify --deep --strict --verbose=2 "$APP"
```

可选人工检查：

```bash
codesign -dv --verbose=4 "$APP" 2>&1 | grep -E 'Authority|TeamIdentifier|Runtime'
```

## 3. 从已签名 app 创建 DMG

```bash
hdiutil create -volname "TRX Vanity 1.0.0" \
  -srcfolder "build/macos-release-stage" \
  -ov -format UDZO "build/TRX-Vanity-signed.dmg"

hdiutil verify "build/TRX-Vanity-signed.dmg"
```

如果改用 CPack 产物发布，必须确认 DMG 内的 `TRX Vanity.app` 是已经签名后的版本，而不是 pre-sign smoke 产物。

## 4. 公证 DMG 并 staple

```bash
xcrun notarytool submit "build/TRX-Vanity-signed.dmg" \
  --keychain-profile "trx-vanity-notary" \
  --wait

xcrun stapler staple "build/TRX-Vanity-signed.dmg"
xcrun stapler validate "build/TRX-Vanity-signed.dmg"
```

如果使用 App Store Connect API key 参数，不要把 key id、issuer id、private key 文件或命令输出贴到 PR/issue/release 里。

## 5. Gatekeeper 验证

在 Downloads 和 Applications 两个位置都验证；建议使用一台没有开发证书的干净 macOS 用户环境复测首次打开体验。

```bash
spctl --assess --type open --context context:primary-signature \
  --verbose "build/TRX-Vanity-signed.dmg"

# 挂载、拖到 /Applications 后：
spctl --assess --type execute --verbose \
  "/Applications/TRX Vanity.app"
```

未签名 Beta 只能作为明确标注的测试构建发布：用户可能看到 Gatekeeper 拦截，只建议 Finder 右键“打开”一次，不要求用户关闭 Gatekeeper，也不要求用户运行来源不明的二进制。

## 6. SHA256 与发布记录

```bash
shasum -a 256 "build/TRX-Vanity-signed.dmg"
```

Release notes 必须记录：

- 版本号、平台和架构。
- 构建命令与测试结果。
- SHA256。
- 签名身份、notarization request 状态、staple 验证状态和 Gatekeeper `spctl` 结果。
- 未签名 Beta / 已签名正式版的差异说明。
- 已知问题和安全说明：默认不输出私钥；不要上传私钥、加密私钥文件、`.env`、token 或真实结果文件。
