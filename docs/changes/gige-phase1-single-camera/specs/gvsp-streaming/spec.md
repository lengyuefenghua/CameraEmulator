## ADDED Requirements

### Requirement: Continuous fixed-image GVSP acquisition
The simulator SHALL emit complete standard GVSP image frames through the Wintun peer to the configured destination using the negotiated packet size and packet delay. Frame identifiers SHALL advance, and stream queue drops or send failures SHALL be observable in diagnostics and counters.
#### Scenario: Client starts and stops a stream
Given a client owns control privilege and has configured a valid stream destination, when it starts acquisition, then it receives complete frames until acquisition is stopped.

### Requirement: Preload and buffer safety
The simulator SHALL finish image preload before acquisition can start and SHALL not reuse a frame buffer until all queued/sending references and the conservative reuse delay have cleared.
#### Scenario: Acquisition requested during preload
Given image preload is still running, when acquisition start is requested, then the request returns Busy and no partially initialized frame is transmitted.
