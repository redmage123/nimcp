# Security Policy

## Supported Versions

We actively support the following versions with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 2.6.x   | :white_check_mark: |
| 2.5.x   | :white_check_mark: |
| < 2.5   | :x:                |

## Reporting a Vulnerability

We take security vulnerabilities seriously. If you discover a security issue, please follow responsible disclosure practices.

### Where to Report

**For security vulnerabilities:**
- **Preferred**: Use GitHub Security Advisories (Private)
  - Go to: https://github.com/redmage123/nimcp/security/advisories
  - Click "Report a vulnerability"
- **Alternative**: Email security@nimcp-project.org (if available) or open a private issue

**Please DO NOT open public issues for security vulnerabilities.**

### What to Include

A good security report includes:

1. **Description**: Clear explanation of the vulnerability
2. **Impact**: What could an attacker accomplish?
3. **Affected Versions**: Which versions are vulnerable?
4. **Reproduction Steps**: How to demonstrate the issue
5. **Proof of Concept**: Code or commands to reproduce (if safe to share)
6. **Suggested Fix**: Your ideas for mitigation (if any)

### Response Timeline

We aim to:
- **Acknowledge** your report within 48 hours
- **Provide initial assessment** within 7 days
- **Release a fix** within 30 days (for critical issues, faster if possible)
- **Publicly disclose** after fix is available (coordinated with you)

### Severity Levels

We classify vulnerabilities as:

**Critical** (CVSS 9.0-10.0):
- Remote code execution
- Privilege escalation to system level
- Data exfiltration of sensitive information

**High** (CVSS 7.0-8.9):
- Authentication bypass
- Significant information disclosure
- Denial of service against core functionality

**Medium** (CVSS 4.0-6.9):
- Limited information disclosure
- Localized denial of service
- Minor privilege escalation

**Low** (CVSS 0.1-3.9):
- Issues with minimal impact
- Requires significant preconditions

## Security Best Practices for Users

### Building from Source
```bash
# Verify the git tag signature
git verify-tag v2.6.2

# Use official releases only
git checkout v2.6.2

# Build with security hardening enabled (default)
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_HARDENING=ON
```

### Running in Production
```bash
# Run with minimal privileges
# Use containerization (Docker) when possible
# Enable address sanitizers during testing
cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON

# Monitor for unexpected behavior
# Log all system interactions
# Implement rate limiting
```

### Dependencies
- Regularly update system dependencies (Python, CUDA, etc.)
- Use package managers to track vulnerabilities
- Monitor GitHub security advisories

## Known Security Considerations

### Neural Network Systems
- **Adversarial Inputs**: Neural networks can be fooled by carefully crafted inputs
- **Model Inversion**: Attackers may extract training data from models
- **Poisoning Attacks**: Training data can be manipulated to bias models

**Mitigations**:
- Validate and sanitize all inputs
- Use differential privacy for sensitive training data
- Implement robust monitoring and anomaly detection

### GPU/CUDA Code
- **Memory Safety**: CUDA code bypasses some memory protections
- **Side Channels**: GPU timing attacks may leak information

**Mitigations**:
- We use explicit bounds checking in GPU kernels
- Sanitize GPU memory after sensitive computations
- Review all CUDA code carefully

### Network Communication (P2P/Distributed)
- **Authentication**: P2P nodes must verify identities
- **Encryption**: All network traffic should be encrypted

**Mitigations**:
- libsodium integration for encryption (when enabled)
- Implement authentication before accepting P2P connections
- Use TLS for web demo communications

## Security Features

NIMCP includes several security features:

### Compile-Time Protections
- Stack canaries (`-fstack-protector-strong`)
- Position Independent Executables (PIE)
- Format string protections
- Full RELRO (Read-Only Relocations)
- Fortify source (`-D_FORTIFY_SOURCE=2`)

### Runtime Protections
- Bounds checking in critical paths
- Input validation
- Memory sanitizers (optional during development)

### Optional Hardening
```bash
# Enable all sanitizers during testing
cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_TSAN=ON
```

## Vulnerability Disclosure Policy

When we fix a security vulnerability:

1. **Private Fix**: Develop and test the fix privately
2. **Coordinated Disclosure**: Notify reporter and allow time for patching
3. **Release**: Publish fix in a new version
4. **Advisory**: Publish security advisory with:
   - CVE identifier (if applicable)
   - Affected versions
   - Severity rating
   - Mitigation steps
   - Credit to reporter (if desired)
5. **Community Notice**: Announce through multiple channels

## Credit

We appreciate security researchers who help make NIMCP safer. With your permission, we will:
- Credit you in the security advisory
- Mention you in release notes
- Add you to our CONTRIBUTORS file

## Questions?

For security questions that aren't vulnerabilities:
- Open a GitHub Discussion with tag `security`
- Email security@nimcp-project.org

---

**Thank you for helping keep NIMCP and its users safe!**
