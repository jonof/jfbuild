#!/bin/bash

set -ex

# Set up.
(base64 -d | bzip2 -d) >test.pcx <<EOT
QlpoOTFBWSZTWV07H3MAACv//+bUxETERUTExERERMREREDARERAxERAQMBAQEDAQEAEwAIcMw
AIp6ZNEAlPMlD8aT0DUaU9QBiNkaIRFP1MJMaJ4KNDQaAAAAADmmJgATABMAAJgACYNU80RMJq
nqGnqDQAAAAAA/YKNluB0boJowF0UGCBcFAE0E6rXKJCXxCqSDY2S4O6uMSiKGWpS388HXMMoo
ju5CJpzolEgqRlE3htYE3OzlZtnFDlArOSMsr5sJiW2Mwi10NepEkN+DQ3hYM8SvzaiPY2IpLh
ECHrQ7RxwhnGIa4kUKIK41LUaRMWUM4VTETTIIKLGs+kGDiOMM7GFM98r5jNmVSEMBEOlGIXVD
aw2g8DPSIOLC1rCnnlsNgbJvBAgnOcuey3Yz+BcB9Gkx+7aK+FN247ZvEGECXTlVe7Yz+h/gvB
dsZA0a7wriXhTse5d0s/UIziZ+6ClevsGBeCF4fDgJ576MnLupnyD0CRcTcKSCYatMTZ3DIDya
TKuAnyozuPhL7AwgR8xOq0pZ7hAD6tJll1f53vFvNe2GDUjlojKJnaiBQVnscLdbJanLxrlaUS
Y8oSnoBetMTeknHI8WooF969KT0AtJ8Rj51TQvXhRK0okw+L57xTpy0ket+2dpRMkuEtd6XG+h
FBtZrJEfJMSvMa4+LbhX9NGvN0iKC6O0Sga7TtmRQZshJYK4MsKwx0khHYK9c22nfIigbEMMje
fCVFBU1wlnmmB3zooGuFWpuJxj16td2Uvj6esb+6b2D7e80RdF8S0MVLyEvuuBZGKZCroeRZGK
YCrmeBYGOub1YyAtEVu/4u5IpwoSC6dj7m
EOT
(base64 -d | bzip2 -d) >tset.pcx <<EOT
QlpoOTFBWSZTWaZ7CZQAACv//+bUxERERcTERETERERExEBARMRARETAQEBAwEBAQMAEQAIcMw
AIp6ZBMgRT9JD8aNA1GlPUAaDyZEIin6mCmNGptMo0aDQAAAAA5gEwAmAAEwABMAAap5ogTJIa
eoaZA0AAAAA/oKM7aXRugm9gLooLIFsoAogpVa4xISUQqkg4HBLg7q41CiKGWpTD9eDrlGMUR3
chEy0+qrVIqkqSMw2YE3Ozlb20Cp2YFaTRlnKjCgnujERa6GapE0MrNDSMAzxK2ojWNiKS4hAh
76HQclYZxkDXUihZBZEtizRphyhoCuYiaoxBZYrB9QTDiOKrNDWlO98r2GnZJUCoCIdKMIurG1
huB4GekQcVU7ZinoktNgbHwBAgrnOW1V1S/yDwPowl79wnhRbvOx3mCyBVsxe591K/6H+B4Jsl
IGDW8OEiYU1YXJtdP1CU5F/ignYJ7BAvNRfrjvgKdPfTnel9VDg7g1MjMaSCYa9cjX4DMDzYS6
8BPlRpceDvUFkCoTE+vWlfsEQPqwl3V/fhAV807o4sVK2eUpF+k8SijSq4WbfF2r05cHMKRWXK
Lj1Au5UyM6N45nlKpAJ8cNaT1AtZ8hl51zRvTjQ5hSKxhNeKenLWWCYbpmFIttoltwVacKFIDc
vbB0ieS8ivMfJvFlwp+WrIG9qkAy0URqTadsykBouLbBTFdhWGOrYubYKttJbTvcpAMkGOZwPB
ykAt4o00Tid8ykAx4o2N5PMfLt17NvFXH07th49VzsH2dIjofEtTFOYo+pxLMxVmKOh5lmYuXi
Jc04liY7aQRk0EqUj/i7kinChIUz2EygA=
EOT
(base64 -d | bzip2 -d) >palette.dat <<EOT
QlpoOTFBWSZTWcfmHPIAAAT4AH////9ffVVUwAH8YYGiJpkE0Cmjyh5pM01NkmoGEGoqf+oCMK
nsKjPRTamgAADAaMhoMIBoBpoAAaniJMkaRoA0ekD1AAD+PSiu1ssASQTSkpA4MJNhqgbTF2Sh
Csy4a1DKSZmWCdVrGUa2q8BHAG6TEtXjaMXsq1jcwr2xgYi/GOUYRSu3Wxcu2UV5xsETg1liYi
YkZO4hhC88jYzgyomFXcWyQdIS0kuLs9hyGS0O4RIkmFGdoqcML6USq95Xmus44D1IbaRzNuhf
Md4uF3cRZL60SvhLrqqsPENIZvA7zxNvkNQrm4tpolXZLFFcc/QNIZiBrtrNvsPcbBeHkOTayF
byny1XjKHwHqQ3zrKnNX6GhcLLjlmJxvs1yU+2v+DsDlIbC0gqG7bU2dwiBxaTLtBPezpkelPg
GEC4VFzu2tZ5CQH2aTMru18BZwrlPVq6HV0EhneuZTTt6ZC3jCmXNUb6Wkhces6TkBeLUN+3b9
Di1aQV5vlacgKVcRo+rqp5q1spaSF6wqzFX51lRBUOVTSQxzxLi9bzhYtIcmcOQfVepczG+jdr
xV9yboeTlpBuIpnNTzfUtIbMm7EVTZiWJjJ06MRVxs155uWkGoGfQ4HpTVDDMrTSe6N+uqFsyv
WM3nBruK0jc98GtDo+x5wZeKdzzJ5it4p2N5PMV4CmJ4FgY65fKcXApa0/4u5IpwoSGPzDnkA=
EOT

rm -f tiles*.art tile0001.pcx

# Create two art files.
./arttool create -f 0 -o 0 -n 8
./arttool create -f 1 -o 8 -n 8
sha1sum -c - <<EOT
806e166b1b3ca01af0c1c03760ef262fcc0c283b  tiles000.art
4416941696e5b4f90c3d7ccfeaec85bc9ec89dfc  tiles001.art
EOT

# Insert tile 1.
./arttool addtile -x 8 -y 4 -ann 1 -ant 1 -ans 1 -regd 1 1 test.pcx
sha1sum -c - <<EOT
466256944f4eae585d33c4809ae2b031d30f72b9  tiles000.art
EOT

# Insert tile 2.
./arttool addtile -x 4 -y 6 -ann 2 -ant 3 -ans 2 -regd 0 2 test.pcx
sha1sum -c - <<EOT
1bb73d4df625c5b1b073106da14219bc11802e30  tiles000.art
EOT

# Insert tile 0.
./arttool addtile -x 2 -y 3 -ann 3 -ant 2 -ans 3 -regd 0 0 test.pcx
sha1sum -c - <<EOT
8da0b3a1ff2d83e459c71ab925fd21da48493b01  tiles000.art
EOT

# Replace tile 1.
./arttool addtile -x 1 -y 2 -ann 2 -ant 0 -ans 4 -regd 1 1 tset.pcx
sha1sum -c - <<EOT
fce717eb04f1ad62a60e3acf9759565af5b608be  tiles000.art
EOT

# Export tile 1.
./arttool exporttile 1
diff tset.pcx tile0001.pcx

# Remove tile 1.
./arttool rmtile 1
sha1sum -c - <<EOT
bade395ffc35a8f36e16cf5aa3b4a3482a1cbcfc  tiles000.art
EOT

# List tile information.
./arttool info > info.txt
diff -u info.txt - <<EOT
File tiles000.art
  Tile 0: 16x8 Xofs: 2, Yofs: 3, AnimType: 2, AnimFrames: 3, AnimSpeed: 3
  Tile 1: 0x0 Xofs: 1, Yofs: 2, AnimType: 0, AnimFrames: 2, AnimSpeed: 4, Registered
  Tile 2: 16x8 Xofs: 4, Yofs: 6, AnimType: 3, AnimFrames: 2, AnimSpeed: 2
  Tile 3: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 4: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 5: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 6: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 7: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
File tiles001.art
  Tile 8: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 9: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 10: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 11: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 12: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 13: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 14: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
  Tile 15: 0x0 Xofs: 0, Yofs: 0, AnimType: 0, AnimFrames: 0, AnimSpeed: 0
EOT

# Clean up.
rm -f tiles*.art tile0001.pcx test.pcx tset.pcx palette.dat info.txt
