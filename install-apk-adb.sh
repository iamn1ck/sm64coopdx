adb install -r ./build/us_pc/sm64coopdx.apk
adb shell input keyevent KEYCODE_WAKEUP
adb shell am start -n com.maniscat2.sm64coopdx/com.maniscat2.sm64coopdx.sm64coopdxActivity