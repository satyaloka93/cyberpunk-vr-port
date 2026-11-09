# Loader verification and messages

Status, Prepare and Install print the identity, embedded file version and full SHA-256 for `bin/x64/dxgi.dll`, `version.dll`, `winmm.dll`, and `red4ext/RED4ext.dll`.

- VERIFIED: bytes and embedded version match the catalog at the expected path. This does not certify runtime compatibility.
- UNKNOWN: hash is absent from the catalog. May be a newer/custom build, not necessarily corrupt. Version text is not proof of identity.
- WRONG-LOCATION: a recognized file is in another loader slot. For example ASI Loader renamed to winmm.dll does not satisfy RED4ext.
- VERSION-MISMATCH: known hash but unexpected version metadata; investigate the catalog/file instead of trusting the version alone.
- MISSING: no file at that path. Optional dxgi/version loaders are not installed automatically.

Unknown optional dxgi/version files are reported and preserved. Existing proxy preservation checks still reject conflicting replacements. Explicit DisableDxgiProxy remains an opt-out for dxgi only.

RED4ext requires both winmm.dll and RED4ext.dll to match known hashes and versions. Missing files can be repaired from the pinned, archive-hash-verified RED4ext 1.31.0 package; extracted components must also match the catalog. Unknown or misplaced existing RED4ext components stop installation BEFORE game writes, with path, identity, version, hash and next steps. Do not blindly delete or rename another mod's loader. A newer RED4ext build needs catalog maintenance; its version number alone is insufficient.

ASI Loader is NOT downloaded by this switcher. Latest Cheeky preview 8 is an upgrade and does NOT provide dxgi.dll. Users may therefore have different existing loaders despite identical port/Cheeky releases.

## Maintaining identities

Edit wabbajack/loader-identities.json. Record exact SHA-256, actual embedded FileVersion (null when absent), expected path, product identity and provenance. For RED4ext derive BOTH component hashes from the checksum-verified official ZIP and verify file versions. Do not learn a trusted hash automatically from an end user's unknown file. The initial ASI/Cheeky entries identify the known local deployment, not an assertion of official distribution provenance.

Package Loader-Identity.ps1 AND loader-identities.json with the main switcher; do not distribute only the updated main script. Run Test-LoaderIdentity.ps1, Test-CyberpunkVRSwitcher.ps1 and Test-Switcher022.ps1 before publishing. Update the catalog when changing pinned dependencies. No loader/runtime changes are required merely to run Status.
