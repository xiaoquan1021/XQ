ITKThickness3D source lock used by XQ

Upstream: ITKThickness3D
Tag: v5.3.0
Commit: 36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb
Tree: e03750b011699d2989d28700f0ffba6b61d64979
License: Apache-2.0 (see LICENSE)

Vendored files:

- itkBinaryThinningImageFilter3D.h
  SHA-256 73e9537c5cb6afc3ec9ecb809d0de73a93d7c923d00b938f28a3e83447177eda
- itkBinaryThinningImageFilter3D.hxx
  SHA-256 c7101f1ac5ef309bc60416dcbfe7c5b188cba323d60bb8017d66c5d065c4bcd6

This is the audited true-3D thinning implementation already accepted by the
vascular dependency task. XQ uses it as a header-only ITK filter; it does not
introduce a second ITK installation or a Python runtime.
