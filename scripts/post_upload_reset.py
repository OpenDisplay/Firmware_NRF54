"""pyocd `flash` leaves the nRF54 core halted after programming, so the device
does nothing until a manual reset. Reset-and-run automatically after upload."""

import time

Import("env")


def after_upload(source, target, env):
    if env.subst("$UPLOAD_PROTOCOL") != "pyocd":
        return

    mcu = (env.BoardConfig().get("build.mcu", "") or "").strip()
    pyocd_target = {
        "nrf54lm20a": "nrf54lm20a",
        "nrf54l15": "nrf54l",
    }.get(mcu, "nrf54l")
    freq = str(env.BoardConfig().get("upload.pyocd_frequency", 4_000_000))

    # The probe/target needs a moment to settle after flash; an immediate reset
    # does not stick.
    time.sleep(2)
    print("post_upload_reset: resetting %s to start firmware" % pyocd_target)
    env.Execute(
        '"%s" -m pyocd reset --target %s --frequency %s'
        % (env.subst("$PYTHONEXE"), pyocd_target, freq)
    )


env.AddPostAction("upload", after_upload)
