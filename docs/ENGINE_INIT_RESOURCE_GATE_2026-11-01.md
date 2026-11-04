# Engine initialization: next original-code dependency gate

Branch: `work/deadspace-remote-preflight-2026-11-01`.

## Facts already demonstrated by original APK research

- Original function `0x2803E4` creates a resource handle via `0x28B3A4`, checks readiness through `0x28A5F4`, and on the supported success path invokes `0x284DF8`, `0x283CB4`, `0x286918`; cleanup reaches `0x28A644` and `0x284F00`.
- Original resource readiness `0x28A5F4` is translated in `src/original_resource_handle.cpp`. Mode zero invokes provider vtable offset `+0x50`; its concrete target has **not** been reconstructed.
- Original registration `0x284178` and 27 startup registration sites are native, but they do not load actual resources.
- Engine state must **not** advance to 1 merely because mocks or isolated tests return success.

## Bounded disassembly workflow

Use the confirmed original, personally owned ARM ELF, locally or on an ephemeral private runner. Install `pyelftools` and `capstone` into the research environment; the production executable does not depend on either.

```sh
python3 tools/deadspace_arm_callgraph_probe.py \
  --elf /private/libEAMGameDeadSpace.so \
  --va 0x2803e4 --size 0x80 \
  --output resource-query-calls.json
```

The length is an **inspection window**, not a claimed verified function boundary. Adjust it only after validating basic-block boundaries against the actual instructions. The report includes assembly operand strings and addresses but not raw ELF bytes or game data. Keep original binaries and per-game resource data private. Never commit extracted assets.

Investigate, in order:
1. Confirm complete control flow, real return semantics and argument registers for `0x2803E4`.
2. Trace `0x28B3A4` creation and find the actual provider object/vtable construction.
3. Resolve the concrete `+0x50` provider implementation using original type construction and vtable data, then reconstruct it in C++ with failure handling.
4. Recover `0x284DF8`, `0x283CB4`, `0x286918`, and the cleanup functions with original allocation ownership and error paths.
5. Integrate those recovered services into `original_execute_known_engine_startup`. Verify against original call order, real resource fixtures and Windows/Linux CTest before permitting the stage-one transition.

The metadata probe does **not** resolve indirect calls, prove a function boundary, or perform symbolic execution; inspect vtables and argument flow separately. Do not claim native engine initialization based on test callbacks.
