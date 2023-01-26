# imx-sdp

imx-sdp is a command-line utility that allows to boot NXP i.MX processors over
Serial Download Protocol (SDP) via the USB interface.

This is a re-implementation of [imx_usb_loader][imx_usb_loader] with the
following design goals:

* Only implement the bare minimum to boot multiple stages over SDP (USB)
* Don't require configuration files

## Invocation

    Usage: imx-sdp [OPTION]... <STAGE>...

    The following OPTIONs are available:

    -h, --help  print this usage message
    -V, --version  print version
    -w, --wait  wait for the first stage

    The STAGEs have the following format:

    <VID>:<PID>[,<STEP>...]
        VID  USB Vendor ID as 4-digit hex number
        PID  USB Product ID as 4-digit hex number

    The STEPs can be one of the following operations:

    write_file:<FILE>:<ADDRESS>
        Write the contents of FILE to ADDRESS
    jump_address:<ADDRESS>
        Jump to the IMX image located at ADDRESS

### Example invocation

    imx-sdp --wait \
        15a2:0080,write_file:SPL:00907400,jump_address:00907400 \
        1b67:5ffe,write_file:u-boot.img:877fffc0,jump_address:877fffc0

[imx_usb_loader]:https://github.com/boundarydevices/imx_usb_loader
