Dead Space Mobile — Milestone 6 native host diagnostic update

1. Extract this ZIP anywhere OUTSIDE your existing dspc game project.
2. Double-click Install-Milestone6.cmd.
3. The installer finds your original dspc project, backs up old host code and
   generated tools/out files, then updates the host and translation tools.
4. If your existing Play Dead Space (WSL).cmd is present, it starts that.
   Otherwise use the Play configuration in VS Code Run and Debug.

No typed terminal commands are required.

The game assets, libDeadSpace.so, personal saves, and your existing VS Code
launch settings are not modified. No original EA files are contained in this
package. The diagnostic is not a verified game crash fix.

Log signatures on next run:
  [RESOURCE_ROOTS]      resolved asset and save roots
  [GUEST_RESOURCE_MISS] native failed resource reads
  [JAVA_ASSET_LIST]     requested asset directory (DS_ASSET_TRACE only)
  [TEXPACK_SOURCE_VECTOR] vector after original asset decoding
  [FATAL:TEXPACK_NULL_VECTOR] vector at the proven original fault site

Source development branch:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/tree/work/deadspace-milestone6-analysis-2026-11-04
