# Security notes

0.1.0-alpha is intended for trusted LAN development and hardware validation.

Implemented:
- salted SHA-256 admin-password verifier rather than clear-text admin password storage
- HttpOnly/SameSite session cookie
- authentication on mutating/system APIs after setup
- `/PaperOS` path sandbox for Web File Manager operations
- explicit confirmation remains a Web UI concern for destructive operations

Known alpha limitations:
- Wi-Fi credentials are stored in LittleFS configuration without flash encryption
- HTTP is plain text on the LAN
- a single in-memory admin session is used
- separate long-lived API tokens are not implemented yet
- secure boot / flash encryption provisioning is not part of the alpha build profile

Do not expose port 80 directly to the public Internet.
