# ProDOS boot template

`ProDOS_2_4_3.po` is the unmodified stable ProDOS 2.4.3 distribution
from the official [ProDOS-8/ProDOS8-Releases repository](https://github.com/ProDOS-8/ProDOS8-Releases),
commit `a95e84426af6b61045b02f9128ea25eb51a0470a`.

SHA-256 (also published in that repository's `SHA256SUM`):

```
398d333cb2ab92df9f8bb2cf64b946f2567116910eb8359cf4bdee5d4194f0fa
```

The build verifies this digest, copies the two boot blocks and extracts the
`PRODOS` operating-system file. Other utilities on the distribution disk are
not copied into the application image. ProDOS retains its original authorship
and copyright; it is a separate third-party operating system, not A2CALC code.
The complete original distribution is retained here for reproducibility.
