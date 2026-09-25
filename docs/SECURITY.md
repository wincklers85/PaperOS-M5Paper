# Security notes

0.3.1-alpha is intended for trusted LAN development and hardware validation.

Implemented:
- salted SHA-256 admin-password verifier rather than clear-text admin password storage
- HttpOnly/SameSite session cookie
- authentication on mutating/system APIs after setup
- `/PaperOS` path sandbox for Web File Manager operations
- destructive Web UI operations require an explicit user confirmation; this is an interaction safeguard, not a substitute for authentication
- SD Recovery's optional same-card restore requires two UI confirmations, stages files up to 1 MiB before writing, and warns that other deleted data can still be overwritten

Known alpha limitations:
- Wi-Fi credentials are stored in LittleFS configuration without flash encryption
- the editable SD Wi-Fi export is plaintext by design; protect the card and downloaded backups
- HTTP is plain text on the LAN
- a single in-memory admin session is used
- separate long-lived API tokens are not implemented yet
- secure boot / flash encryption provisioning is not part of the alpha build profile

Do not expose port 80 directly to the public Internet.
