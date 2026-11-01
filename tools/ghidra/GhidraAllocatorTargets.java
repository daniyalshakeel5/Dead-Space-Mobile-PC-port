// Targeted metadata-only mapping of the original allocator owner/provider.
// Ghidra import image base is ELF VA + 0x10000; do not treat as host pointers.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class GhidraAllocatorTargets extends GhidraScript {
    private static final long BIAS = 0x10000L;
    private String jsonAddress(Address a) {
        if (a == null) return "null";
        return "\"0x" + Long.toHexString(a.getOffset()) + "\"";
    }
    private String inspect(long elfVA) throws Exception {
        Address start = toAddr(elfVA + BIAS);
        Function fn = currentProgram.getFunctionManager().getFunctionContaining(start);
        if (fn == null) return "{\"elf_entry\":\"0x"+Long.toHexString(elfVA)+"\",\"found\":false}";
        List<String> calls = new ArrayList<>();
        List<String> indirects = new ArrayList<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(fn.getBody(), true);
        int instructions = 0;
        while (it.hasNext() && instructions++ < 512) {
            Instruction inst = it.next();
            String mnemonic = inst.getMnemonicString().toLowerCase();
            if (mnemonic.startsWith("ldr") && inst.getDefaultOperandRepresentation(0).equalsIgnoreCase("pc"))
                indirects.add("\"0x"+Long.toHexString(inst.getAddress().getOffset()-BIAS)+"\"");
            for (Reference ref : inst.getReferencesFrom()) {
                if (!ref.getReferenceType().isCall()) continue;
                Address target = ref.getToAddress();
                if (target == null) continue;
                calls.add("\"0x"+Long.toHexString(inst.getAddress().getOffset()-BIAS)+":0x"+
                          Long.toHexString(target.getOffset()-BIAS)+"\"");
            }
        }
        Collections.sort(calls);
        return "{\"elf_entry\":\"0x"+Long.toHexString(elfVA)+
            "\",\"found\":true,\"ghidra_function_start\":"+jsonAddress(fn.getEntryPoint())+
            ",\"instruction_count_bounded\":"+instructions+
            ",\"direct_call_sites\":["+String.join(",",calls)+
            "],\"pc_destination_load_sites\":["+String.join(",",indirects)+"]}";
    }
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length!=1) throw new IllegalArgumentException("output JSON required");
        long[] roots={0x0027e930L,0x0027f790L,0x00281d54L,0x00280520L};
        List<String> nodes=new ArrayList<>();
        for(long va:roots) nodes.add(inspect(va));
        String payload="{\"kind\":\"original-allocator-target-metadata\",\"schema\":1,"+
            "\"address_bias\":65536,\"roots\":["+String.join(",",nodes)+"]}\n";
        Path out=Path.of(args[0]);
        Files.createDirectories(out.getParent());
        Files.writeString(out,payload,StandardCharsets.UTF_8);
        println("Allocator target metadata written (no executable bytes).");
    }
}
