# XQ Integrated Rebuild

Medical image processing application - integrated rebuild with clean architecture.

## Status
Initial skeleton - implementation in progress.

## Structure
- `src/core/` - Core data model (XQProject, XQScene, XQDataNode hierarchy)
- `tests/core/` - Core module tests
- Build system TBD after dependency decisions

## Architecture Principles
- XQ-owned object model (no plugin framework coupling)
- No dependencies on MITK, BlueBerry, CTK, or legacy plugin systems
- Legacy XQ used only as behavioral reference
