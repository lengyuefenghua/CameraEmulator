## ADDED Requirements

### Requirement: Camera configuration and lifecycle UI
The application SHALL let the user create, edit, start, and stop the supported camera configuration and SHALL present runtime state, Wintun adapter lifecycle, host-side and camera-side addresses, frame rate, drops, and diagnostics without blocking the UI thread.
#### Scenario: User starts a configured camera
Given a valid camera configuration, when the user starts it, then the UI reflects initializing, Wintun-ready, and running states and later reflects stop or error events.

### Requirement: Visible deployment and startup failures
The application SHALL show a clear failure state when elevation, Wintun adapter creation, host-side address configuration, or required tunnel service setup fails, without silently using an existing interface.
#### Scenario: Required Wintun setup is denied
Given adapter creation is denied, when the user starts the simulator, then startup fails visibly and reports the affected setup failure without fallback.
