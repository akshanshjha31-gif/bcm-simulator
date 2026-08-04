# Documentation Index

All documents are complete for software version **1.0.0**.

| Doc | ID | Contents |
|-----|----|----------|
| **[Project Report](ProjectReport.md)** | **BCM-PRJ-001** | **Start here** — how every phase was built, the techniques used, and how to test the whole project |
| [Software Requirements Specification](requirements/SRS.md) | BCM-SRS-001 | Numbered requirements |
| [Architecture Document](architecture/ArchitectureDocument.md) | BCM-ARC-001 | Layering, module catalogue, ADRs |
| [Interface Control Document](protocol/ICD.md) | BCM-ICD-001 | Wire format, CRC-8, command set, worked examples |
| [UML Model](uml/UML.md) | BCM-UML-001 | Class, state, sequence, concurrency, deployment |
| [Test Report & Traceability](test/TestReport.md) | BCM-TST-001 | Results, requirements matrix, defect record |
| [User Manual](manual/UserManual.md) | BCM-MAN-001 | Setup, wiring, operation, troubleshooting |
| [Release Notes](ReleaseNotes.md) | — | v1.0.0 summary and limitations |

## API reference

Generated from the source comments — 360 pages, currently zero warnings:

```powershell
tools\bcm.ps1 docs              # -> docs/api/html/index.html
tools\bcm.ps1 docs -SilArgs open
```

`docs/api/` is gitignored: it is generated output, and committing it would
bury real changes under thousands of regenerated HTML diffs.

## Layout

```
docs/
├─ ProjectReport.md          BCM-PRJ-001
├─ ReleaseNotes.md
├─ Doxyfile · mainpage.dox   API reference configuration
├─ requirements/SRS.md       BCM-SRS-001
├─ architecture/             BCM-ARC-001
├─ protocol/ICD.md           BCM-ICD-001
├─ uml/UML.md                BCM-UML-001
├─ test/TestReport.md        BCM-TST-001
└─ manual/UserManual.md      BCM-MAN-001
```

> A Software Design Document (`BCM-SDD-001`) was listed in the original plan
> but was not written as a separate artefact: its content is covered by the
> Architecture Document, the UML model and the Doxygen reference, and a fourth
> overlapping design document would have drifted rather than helped.
