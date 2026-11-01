# Milestone 4: GitHub-hosted checkpoint intake

**State:** metadata/safety tooling is implemented and tested using fake ZIPs.
The actual `dspc_milestone4.zip` uploaded to ChatGPT has **not** been read or
merged. Its presence in ChatGPT does not make its bytes available to GitHub.

## Option A: checkpoint contains only your own source

If no proprietary game assets, decompiled copyrighted game binaries,
credentials, or third-party material are present, push the *source files*
as a separate GitHub branch in this repository and share that branch. The
GitHub connector can then inspect each file and make reviewable integration
commits. **The public repository makes every public branch accessible.**

## Option B: checkpoint contains game assets or binary research inputs

Use a private HTTPS object-storage download URL with a short expiration;
do **not** attach the archive to a public issue, PR, commit, release, or Actions
artifact. In the repository:

1. Open **Settings → Secrets and variables → Actions**.
2. Add `DS_MILESTONE4_URL` as a repository secret with the private,
   temporary HTTPS URL.
3. Once `.github/workflows/milestone-intake.yml` exists on the default
   branch, open **Actions → Dead Space Mobile - Milestone 4 intake audit**
   and choose **Run workflow**.
4. Inspect job steps and the `deadspace-milestone4-private-metadata-only`
   artifact. The GitHub-hosted runner downloads the archive into temporary
   storage, inspects its ZIP metadata, and deletes it. Public artifacts contain
   **only aggregate extension counts, hashes, sizes, and warning codes**,
   never file contents or full entry paths.
5. Revoke the temporary URL after running. A green test-only workflow
   without the private download **does not** mean Milestone 4 was inspected.

**Important:** GitHub's manually dispatched workflows normally must be
present on the repository's default branch. This audit remains on the
research PR until reviewed/merged. On the feature branch, push/PR checks
exercise it with *synthetic data only*.

## Intake and safety gates

The script `tools/milestone_archive_audit.py` refuses to certify unsafe
archive entries, path traversal, ZIP symlinks, ambiguous/colliding names,
Git repository metadata entries, extreme compression ratios, and oversized
archives. Restricted binary extensions trigger a licensing/provenance
review. It **never unpacks, executes, imports, or publishes** code from
the ZIP. A passing audit is necessary but not sufficient for source
integration, and does not prove a PC game implementation is present.

## Integration once files are accessible

1. Compare all project-authored source changes against the existing C++17
   foundation; preserve prior tests and don't overwrite verified commits.
2. Identify claimed game subsystems in Milestone 4 and validate each against
   original APK evidence; mark unverified ones explicitly.
3. Integrate one cohesive feature at a time into reviewable GitHub commits.
4. Build and test on GitHub-hosted Windows and Linux runners.
5. Keep the original APK/assets private and out of releases.

The current green native builds are **keyboard/mouse diagnostics only**,
not Dead Space gameplay or a translated Android executable.
