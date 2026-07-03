<div class="modt-title-page">
<p class="modt-title-name">Default System Title</p>
<p class="modt-title-subtitle">From requirements to code-facing design</p>
<p class="modt-title-note">A modeled product with a polished generated report.</p>
<table class="modt-title-metadata">
<tr><th>System</th><td>ReportOptions</td></tr>
</table>
</div>

<nav id="TOC" class="modt-contents">
<h2>Contents</h2>
<ul>
<li class="toc-level-1"><a href="#report-overview">Report overview</a></li>
<li class="toc-level-1"><a href="#domain">Domain</a></li>
<li class="toc-level-2"><a href="#glossary">Glossary</a></li>
<li class="toc-level-1"><a href="#analysis">Analysis</a></li>
<li class="toc-level-2"><a href="#supplementary-specification">Supplementary Specification</a></li>
<li class="toc-level-2"><a href="#use-cases">Use Cases</a></li>
<li class="toc-level-3"><a href="#use-case-1-exportdocumentation">ExportDocumentation</a></li>
<li class="toc-level-2"><a href="#system-operations">System Operations</a></li>
<li class="toc-level-3"><a href="#system-operation-1-generatereport-format-string">generateReport(format: string)</a></li>
<li class="toc-level-2"><a href="#operation-contracts">Operation Contracts</a></li>
<li class="toc-level-3"><a href="#operation-contract-1-generatereport-format">generateReport(format)</a></li>
<li class="toc-level-1"><a href="#design">Design</a></li>
<li class="toc-level-2"><a href="#classes">Classes</a></li>
<li class="toc-level-3"><a href="#class-1-reportjob">Class: ReportJob</a></li>
<li class="toc-level-1"><a href="#implementation">Implementation</a></li>
<li class="toc-level-2"><a href="#class-operations">Class Operations</a></li>
</ul>
</nav>

# Implementation Handoff Report {#report-overview}

_From requirements to code-facing design_

A modeled product with a polished generated report.

| Item | Count |
| --- | ---: |
| Requirements | 1 |
| Glossary terms | 1 |
| Use cases | 1 |
| System operations | 1 |
| Operation contracts | 1 |
| Classes | 1 |
| Relationships | 0 |

## Domain {#domain}

### Glossary {#glossary}

| Term | Definition | Rules |
| --- | --- | --- |
| Report | A generated project handoff document. | Must preserve project intent and implementation constraints. |

## Analysis {#analysis}

### Supplementary Specification {#supplementary-specification}

| Category | Requirement | Description |
| --- | --- | --- |
| reliability | Recoverable export | Documentation generation should not depend on diagram rendering. |

### Use Cases {#use-cases}

| Use Case | Primary Actor | Steps | Preconditions | Postconditions |
| --- | --- | ---: | ---: | ---: |
| ExportDocumentation | Developer | 1 | 1 | 1 |

#### ExportDocumentation {#use-case-1-exportdocumentation}

**Description:** Developer generates a report for implementation planning.

**Actor:** Developer

**Preconditions:**
- Project model is valid

**Flow of Events:**
1. generateReport 
    - format: pdf

**Postconditions:**
- Report artifact is written

### System Operations {#system-operations}

| Operation | Actor | Use Case |
| --- | --- | --- |
| generateReport(format: string) | Developer | ExportDocumentation |

#### generateReport(format: string) {#system-operation-1-generatereport-format-string}

**Actor:** Developer

**Use Case:** ExportDocumentation

**Preconditions:**
- Project model is valid

**Postconditions:**
- Report artifact is written

### Operation Contracts {#operation-contracts}

#### generateReport(format) {#operation-contract-1-generatereport-format}

**Use Case:** ExportDocumentation

**Preconditions:**
- Project model is valid

**Postconditions:**
- Markdown report is generated
- PDF report uses configured CSS when requested

## Design {#design}

### Classes {#classes}

| Class | Attributes | Methods | Responsibility Signal |
| --- | ---: | ---: | --- |
| ReportJob | 1 | 1 | 1 operations |

#### Class: ReportJob {#class-1-reportjob}

##### Attributes

| Attribute | Type | Visibility | Metadata |
| --- | --- | --- | --- |
| format | string | unspecified | - |

## Implementation {#implementation}

### Class Operations {#class-operations}

#### Class: ReportJob Operations {#class-operations-reportjob}

| Method | Visibility | Effects | Metadata | Pre/Post Conditions |
| --- | --- | --- | --- | --- |
| run() | + | - | - | - |

