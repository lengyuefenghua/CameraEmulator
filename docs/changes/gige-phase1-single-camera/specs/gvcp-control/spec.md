## ADDED Requirements

### Requirement: IPv4 device discovery
The simulator SHALL answer valid GigE Vision discovery commands with the matching camera identity, network parameters, device mode, and an accessible XML URL. A reply SHALL use the simulated camera IPv4 address as its source address through the Wintun peer.
#### Scenario: Client discovers a configured camera
Given a started Wintun peer with distinct host-side and camera-side IPv4 addresses, when a valid discovery command arrives through the peer, then the client receives one complete discovery acknowledgement and can fetch that camera's XML URL.

### Requirement: Control privilege and heartbeat
The simulator SHALL enforce the supported control-channel privilege and release ownership after the configured heartbeat interval expires. Unsupported event and action commands SHALL return an explicit unsupported result rather than being silently ignored.
#### Scenario: Heartbeat expiry releases ownership
Given a client owns control privilege, when its heartbeat expires, then ownership is cleared and another client can acquire it.
