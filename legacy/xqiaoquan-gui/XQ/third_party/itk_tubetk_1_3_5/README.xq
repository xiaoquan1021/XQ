ITK-TubeTK 1.3.5 narrow source vendoring
=========================================

Upstream: InsightSoftwareConsortium/ITKTubeTK release 1.3.5
License: Apache-2.0 (see LICENSE)
Locked archive: D:/XQ/research/ITKTubeTK-1.3.5.tar.gz
Archive SHA-256: 26972916EB332275106A6AF47CAE3182B0D035ECA329EC56069D630A24747A4C
Vendored: 2026-07-13

This subtree is a byte-for-byte subset of the locked upstream source. It is the
transitive local include/source closure needed by:

- tube::SegmentTubes
- itk::tube::TubeExtractor / RidgeExtractor / RadiusExtractor3
- tube::ConvertTubesToImage
- itk::tube::TubeSpatialObjectToImageFilter
- tube::ResampleImage
- itk::tube::ReResampleImageFilter

XQ does not modify the upstream algorithms. XQ-specific input validation,
resampling orchestration, provenance and result materialization live in
src/adapters/itk/ItkAutomaticVesselSegmenter.cpp.

Do not replace this subtree with, link against, or load binaries from
D:/XQ/research/itk-tubetk-install. That research installation belongs to a
different ITK build. The vendored sources compile against XQ's exact product
ITK 5.4.0 module closures and dynamic MSVC CRT.
