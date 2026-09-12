# Iris

[![Build Iris](https://github.com/atgehrhardt/iris/actions/workflows/build.yml/badge.svg)](https://github.com/atgehrhardt/iris/actions/workflows/build.yml)

Iris is a controller-focused fork of
[Moonlight Android](https://github.com/moonlight-stream/moonlight-android) for
streaming from Prism. It retains Moonlight's controller gyro, handheld motion
fallback, controller rumble, and trigger-rumble support while adding
device-agnostic calibration for up to four independent rear controls.

In Iris, open **Settings → Gamepad Settings → Rear button calibration**. Select
the primary controller, choose how many rear controls the device has, and press
each control in the order it should be exposed. One to four controls are
supported, and calibration defaults to the common two-button layout. A control
may originate from the primary controller or a separate Android input device.
Iris sends the results through Moonlight's existing four-paddle protocol fields.
Prism automatically creates a virtual DualSense Edge. The common two-control
layout maps directly to its left and right paddles; optional third and fourth
controls map to Fn1 and Fn2. A calibrated profile reports the PlayStation
controller family and uses the handheld motion sensors when the controller
does not expose its own, even if Android labels the built-in pad as Xbox.

For handhelds whose built-in controls do not expose their own Android vibrator,
enable **Emulate rumble support with vibration** in Gamepad Settings. Iris then
advertises standard rumble for player one and routes host feedback to the
handheld vibrator. A calibrated primary controller is announced as soon as the
stream connects, allowing Steam to discover it without waiting for the first
button or stick event.

Iris uses the application ID `dev.prism.iris`, so it installs alongside the
official Moonlight app. See
[the controller architecture](docs/controller-architecture.md) for the protocol
boundary, auxiliary-device routing, and why InputPlumber is optional.

## Building Iris

Initialize submodules and build the non-root APK:

```shell
git submodule update --init --recursive
./gradlew assembleNonRootDebug
```

Release builds can be signed locally by setting `IRIS_KEYSTORE_FILE`,
`IRIS_KEYSTORE_PASSWORD`, `IRIS_KEY_ALIAS`, and `IRIS_KEY_PASSWORD` and running
`./gradlew assembleNonRootRelease`.

## Releasing Iris

The **Release Iris** workflow builds a signed non-root release APK and
publishes a GitHub release automatically when a `v*` tag is pushed. It runs
Android lint, unit tests, the debug build, and the secret scan before building
and verifying the signed APK. Releases include the APK, R8 mapping, native
debug symbols, SHA-256 checksums, and generated release notes.

Before the first release, configure these repository Actions secrets under
**Settings → Secrets and variables → Actions**:

| Secret | Value |
| --- | --- |
| `IRIS_KEYSTORE_BASE64` | Base64-encoded release keystore (`base64 -w 0 iris-release.jks` on Linux) |
| `IRIS_KEYSTORE_PASSWORD` | Keystore password |
| `IRIS_KEY_ALIAS` | Signing key alias |
| `IRIS_KEY_PASSWORD` | Signing key password |

Keep the release keystore backed up and reuse it for future releases so existing
installations can update. Signing credentials are required; the workflow fails
with the missing secret names if they are absent.

For each release:

1. Update `versionName` and increment `versionCode` in `app/build.gradle`, then
   merge the change into `master`.
2. Tag that commit with `v` followed by the exact `versionName` and push the tag.
   For example, for `versionName "0.1.0"`:

   ```shell
   git switch master
   git pull --ff-only
   git tag v0.1.0
   git push origin v0.1.0
   ```

The workflow rejects tags that do not match the app version. The resulting
`iris-<version>.apk` is available on the
[Releases page](https://github.com/atgehrhardt/iris/releases) and can be followed
by Obtainium. Re-running a tag release replaces its assets.

To verify signing before publishing, run **Release Iris → Run workflow** on a
branch in the Actions tab. Branch runs upload signed assets as a workflow
artifact without creating a GitHub release; runs targeting a matching `v*` tag
also publish the release.

## Keeping up with Moonlight

**Sync Moonlight upstream** checks `moonlight-stream/moonlight-android`'s
`master` daily at 08:23 UTC and can also be run from the Actions tab on `master`.
Once this workflow is merged into Iris's default branch and scheduled workflows
are enabled, it automatically merges clean upstream updates into `master` after
the secret scan, Android lint, unit tests, and debug APK build pass. No additional
token or signing secrets are needed. Checks run explicitly on the exact merge
commit through the reusable build workflow; bot pushes do not trigger push CI.

Conflicts, upstream workflow changes, and failed checks stop the sync. The run
summary lists conflicting files. If `master` changes during validation, rerun
the workflow to validate a fresh merge. Temporary candidate branches are removed
after checks finish. Repository rules still apply: if direct bot pushes to
`master` are prohibited, the final push fails and a maintainer must integrate the
update through the repository's normal review process. The workflow does not
bypass protection rules or publish releases.

Iris retains Moonlight's protocol and Git ancestry, but its launcher, controller
handling, branding, and release configuration have diverged. A clean Git merge
and passing CI do not replace testing streaming, rear controls, gyro, rumble,
and HDR on a device after significant upstream changes.

The initial integration on 2026-09-12 brought in Moonlight `98c12beb` and
resolved the `app/build.gradle` conflicts while preserving Iris's application
IDs, versioning, signing, RecyclerView dependency, and tests. It incorporates
Moonlight's updated SDK, NDK, Gradle, and dependency requirements.

For future conflicts, perform a manual merge and preserve these Iris-specific
settings:

```shell
git switch -c merge-moonlight origin/master
git fetch https://github.com/moonlight-stream/moonlight-android.git master
git merge --no-ff FETCH_HEAD
# Resolve conflicts, then git add the resolved files and git commit.
git submodule update --init --recursive
./gradlew lintNonRootDebug testNonRootDebugUnitTest assembleNonRootDebug
```

Review and merge that branch using the normal contribution process. Subsequent
conflicts require the same manual integration; automation never chooses one
side of a conflict automatically.

## Upstream project

[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/232a8tadrrn8jv0k/branch/master?svg=true)](https://ci.appveyor.com/project/cgutman/moonlight-android/branch/master)
[![Translation Status](https://hosted.weblate.org/widgets/moonlight/-/moonlight-android/svg-badge.svg)](https://hosted.weblate.org/projects/moonlight/moonlight-android/)

[Moonlight for Android](https://moonlight-stream.org) is an open source client for NVIDIA GameStream and [Sunshine](https://github.com/LizardByte/Sunshine).

Moonlight for Android will allow you to stream your full collection of games from your Windows PC to your Android device,
whether in your own home or over the internet.

Moonlight also has a [PC client](https://github.com/moonlight-stream/moonlight-qt) and [iOS/tvOS client](https://github.com/moonlight-stream/moonlight-ios).

You can follow development on our [Discord server](https://moonlight-stream.org/discord) and help translate Moonlight into your language on [Weblate](https://hosted.weblate.org/projects/moonlight/moonlight-android/).

## Moonlight downloads
* [Google Play Store](https://play.google.com/store/apps/details?id=com.limelight)
* [Amazon App Store](https://www.amazon.com/gp/product/B00JK4MFN2)
* [F-Droid](https://f-droid.org/packages/com.limelight)
* [APK](https://github.com/moonlight-stream/moonlight-android/releases)

## Building upstream Moonlight
* Install Android Studio and the Android NDK
* Run ‘git submodule update --init --recursive’ from within moonlight-android/
* In moonlight-android/, create a file called ‘local.properties’. Add an ‘ndk.dir=’ property to the local.properties file and set it equal to your NDK directory.
* Build the APK using Android Studio or gradle

## Authors

* [Cameron Gutman](https://github.com/cgutman)  
* [Diego Waxemberg](https://github.com/dwaxemberg)  
* [Aaron Neyer](https://github.com/Aaronneyer)  
* [Andrew Hennessy](https://github.com/yetanothername)

Moonlight is the work of students at [Case Western](http://case.edu) and was
started as a project at [MHacks](http://mhacks.org).
