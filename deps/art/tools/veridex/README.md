appcompat.sh
============

Given an APK, finds API uses that fall into the
blocklist/max-target-X/unsupported APIs.

NOTE: appcompat is still under development. It can report
API uses that do not execute at runtime, and reflection uses
that do not exist. It can also miss on reflection uses.

To build it:
> m appcompat

To run it:
> appcompat --dex-file=test.apk
