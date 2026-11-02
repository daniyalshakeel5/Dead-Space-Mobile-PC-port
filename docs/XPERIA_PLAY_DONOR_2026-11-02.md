# Xperia Play Vita-ready donor, confirmed November 2 2026

The user's private deadspace.zip is the exact 145903794-byte Vita donor.
SHA1: 61d51d8ba5374f97b5a4971a2d9d7da31baf840c
SHA256: 04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31

The archive contains the exact 4499468-byte Xperia 1.1.33 ARM ELF
(SHA1 0ed42b611415015807f759ec9b5457857143ce39).
There are 1745 game asset files (254747912 uncompressed bytes), 22 scene
map files, the audio set, game models and frontend textures.
The source ZIP contains its root under deadspace/.

IMPORTANT: EapRules' donor manifest identifies this exact build as
vita-rip-atc, not the full PVRTC build. ATC texture handling is needed.
The Vita-ready donor omits optional content but retains the main campaign.
Reference: https://github.com/EapRules/deadspace-native-arm

Local install, without uploading or distributing copyrighted assets:

    python -m tools.xperia_vita_donor_install --zip deadspace.zip --verify-only
    python -m tools.xperia_vita_donor_install --zip deadspace.zip --destination installed-xperia

The installer validates the entire ZIP SHA256 and key original resource hashes,
rejects traversal, ZIP symlinks, case collisions and suspicious sizes,
stages extracted assets transactionally, and refuses to overwrite installs.
Output: installed-xperia/assets and installed-xperia/lib/armeabi/libEAMGameDeadSpace.so.
An installer is not a gameplay runtime. ARM JNI imports and GLES/ATC support
are still required for a playable x86-64 build.
