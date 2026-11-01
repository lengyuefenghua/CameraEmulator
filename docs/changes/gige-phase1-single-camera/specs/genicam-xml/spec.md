## ADDED Requirements

### Requirement: XML and register semantics agree
The simulator SHALL serve GenApi 1.1 XML whose supported feature values, ranges, access modes, and enum values agree with the register behavior. The generated document SHALL validate against the pinned GenApi 1.1 schema and required reference-node contract.
#### Scenario: Client reads and changes a supported feature
Given a client fetched the XML, when it reads or writes a supported feature through GVCP, then the observed register value and subsequent XML projection agree.

### Requirement: Unsupported probes degrade explicitly
The simulator SHALL return a protocol-defined result or documented zero value for unsupported register probes and SHALL emit a rate-limited diagnostic without terminating the control session.
#### Scenario: Client probes an unknown register
Given an active control session, when the client reads an unknown register, then it receives zero and the session remains usable.
