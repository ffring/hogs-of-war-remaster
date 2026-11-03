# Bounded camera observer

The optional mouse source currently preserves the experimental controlled-RMB input mapping and adds a diagnostic observer. It is installed on the owner machine for a manual test; the released 0.5 installers are unchanged. The requested mouse-controlled chase camera remains unimplemented.

Sampling runs on the existing foreground human-input callback. It records at most 4,096 CSV rows per game process: game/camera mode, selected weapon, validated controller and active pig, pig position and heading, camera fields, synthetic button mask and six configured game-bound keys. It does not record arbitrary system keys or write camera/character memory. All observed game ranges are checked with VirtualQuery before reads. Camera and pig angles use signed 32-bit integer values; pig-heading interpretation additionally requires object type 0x1357.

Rows are compacted into 64-row batches, flushed at most every two seconds or when full. A unique `HogsMouseObserver-<pid>-<tick>-<sequence>.csv` is created beside the ASI with CREATE_NEW, preserving existing files. The observer stops logging on write errors. These runtime logs are local and are not included in this repository.

The console fixture uses synthetic memory and verifies pointer validation, negative integer angles, active-pig transforms, game-bound key flags, 64-column serialization, exact variable-width multirow writes, full/partial drains, sink-failure retry and non-overwriting file creation. It and the release ASI load probe passed in an isolated Wine prefix. No game was launched for those checks. Actual in-game observation is pending.

The local installation was read back after taking the shared game.pid lock and confirming the game was closed. Ten core files, 140 original archives, 24 HD pages and both launchers were verified. This diagnostic installation does not establish improved camera behavior.
