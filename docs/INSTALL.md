# Installing a downloaded build

Builds come from the repo's **Actions** tab: open the latest green run and
download the artifact for your platform. Each artifact is a zip.

## macOS
1. Unzip the download. Inside are `Ruminate-macOS-VST3.zip`,
   `Ruminate-macOS-AU.zip` and `Ruminate-macOS-Standalone.zip`. Unzip the
   one(s) you need.
2. Move the bundle into place:
   * `Ruminate.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   * `Ruminate.component` → `~/Library/Audio/Plug-Ins/Components/`
3. The build is not notarized, so macOS will quarantine it. Clear that once
   in Terminal (adjust the path for the one you installed):
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Ruminate.vst3
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Ruminate.component
   ```
4. Restart the DAW. For AU in Logic / GarageBand, if it doesn't appear, run
   `killall -9 AudioComponentRegistrar` and restart Logic, or open the Plug-in
   Manager and reset/rescan.

## Windows
1. Unzip the download.
2. Move the `Ruminate.vst3` folder into `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in the DAW.

## Linux
Copy `Ruminate.vst3` into `~/.vst3/` and rescan.
