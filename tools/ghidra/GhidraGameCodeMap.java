// Ghidra 12.1: inspect the USER-CONFIRMED upstream game's original ARM ELF.
// Reports a limited call graph and sanitized high-P-code operation counts.
// No raw decompiled source, strings, bytes, proprietary symbols or assets are
// emitted to GitHub logs/artifacts. Independent PC implementation follows
// only after code-level behavior has been verified.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class GhidraGameCodeMap extends GhidraScript {
    private static final String[] ANCHORS = {
        "eglSwapBuffers", "eglCreateContext", "eglMakeCurrent",
        "glDrawElements", "glDrawArrays", "glUseProgram", "glTexImage2D",
        "glCompressedTexImage2D", "AAssetManager_open", "AAsset_read",
        "slCreateEngine", "pthread_create", "clock_gettime", "JNI_OnLoad"
    };
    private static final int MAX_FUNCTIONS = 36;
    private static final int MAX_CALLERS = 80;
    private static final int MAX_DECOMPILATION_SECONDS = 12;

    // Follow the exact original startup path, not unrelated renderer imports.
    // Publish callgraph/address metadata only, never disassembled source.
    private void appendEntryChain(StringBuilder out, Function root, int depth,
                                  Set<String> seen, int maxNodes) throws Exception {
        if(root==null || root.isExternal() || seen.size()>=maxNodes || depth>4) return;
        final String id=addr(root);
        if(!seen.add(id))return;
        if(seen.size()>1)out.append(',');
        out.append("{\"entry\":").append(escape(id));
        out.append(",\"depth\":").append(depth);
        out.append(",\"code_address_count\":").append(root.getBody().getNumAddresses());
        InstructionIterator it=currentProgram.getListing().getInstructions(root.getBody(),true);
        int instructionCount=0;
        boolean justReturn=true;
        while(it.hasNext() && instructionCount<65536) {
            Instruction inst=it.next();instructionCount++;
            if(!(instructionCount==1 &&
                 inst.getMnemonicString().equalsIgnoreCase("bx") &&
                 inst.getDefaultOperandRepresentation(0).equalsIgnoreCase("lr"))) {
                justReturn=false;
            }
        }
        out.append(",\"instruction_count\":").append(instructionCount);
        out.append(",\"exact_bx_lr_only\":").append(instructionCount==1 && justReturn);
        List<Function> callees=new ArrayList<>();
        for(Function child:root.getCalledFunctions(monitor)) {
            if(!child.isExternal())callees.add(child);
        }
        callees.sort(Comparator.comparing(GhidraGameCodeMap::addr));
        out.append(",\"native_callees\":[");
        for(int i=0;i<Math.min(20,callees.size());i++) {
            if(i>0)out.append(',');
            out.append(escape(addr(callees.get(i))));
        }
        out.append("]}");
        if(depth<4)
            for(Function child:callees)appendEntryChain(out,child,depth+1,seen,maxNodes);
    }
    private static String escape(String value) {
        StringBuilder out=new StringBuilder("\"");
        for (int i=0;i<value.length();++i) {
            char c=value.charAt(i);
            if (c=='\"'||c=='\\') out.append('\\').append(c);
            else if (c=='\n'||c=='\r') out.append(' ');
            else if (c<0x20) out.append('?');
            else out.append(c);
        }
        return out.append("\"").toString();
    }
    private static String addr(Function f) {
        return "0x" + f.getEntryPoint().toString();
    }
    private int directCallers(Function f) {
        try {return f.getCallingFunctions(monitor).size();}
        catch (Exception ex) {return 0;}
    }
    private static void stringField(StringBuilder out,String key,String value) {
        out.append(escape(key)).append(':').append(escape(value));
    }
    @Override
    public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length!=1) throw new IllegalArgumentException("output JSON path required");
        final List<Function> internals=new ArrayList<>();
        final List<Function> externals=new ArrayList<>();
        final List<String> jni=new ArrayList<>();
        for(FunctionIterator it=currentProgram.getFunctionManager().getFunctions(true);
            it.hasNext();) {
            Function f=it.next();
            if(f.isExternal()) externals.add(f);
            else internals.add(f);
            if (!f.isExternal() && (f.getName().equals("JNI_OnLoad") ||
                                     f.getName().startsWith("Java_"))) {
                jni.add(f.getName());
            }
        }
        for(Function f:currentProgram.getFunctionManager().getExternalFunctions())
            if (!externals.contains(f)) externals.add(f);
        Collections.sort(jni);
        Map<String,Integer> counts=new TreeMap<>();
        Map<Function,Set<String>> reached=new LinkedHashMap<>();
        Map<Function,Integer> distance=new HashMap<>();
        for(String anchor:ANCHORS) {
            int total=0;
            List<Function> matched=new ArrayList<>();
            for(Function f:internals)
                if(f.getName().equals(anchor)) matched.add(f);
            for(Function f:externals)
                if(f.getName().equals(anchor)) matched.add(f);
            for(Function f:matched) {
                Set<Function> first=f.getCallingFunctions(monitor);
                total+=first.size();
                int limit=0;
                for(Function caller:first) {
                    if(caller.isExternal() || limit++>=MAX_CALLERS) continue;
                    reached.computeIfAbsent(caller,k->new LinkedHashSet<>()).add(anchor);
                    distance.merge(caller,1,Math::min);
                    int grandparents=0;
                    for(Function parent:caller.getCallingFunctions(monitor)) {
                        if(parent.isExternal() || grandparents++>=MAX_CALLERS) continue;
                        reached.computeIfAbsent(parent,k->new LinkedHashSet<>()).add(anchor);
                        distance.merge(parent,2,Math::min);
                    }
                }
            }
            counts.put(anchor,total);
        }
        for(Function f:internals) {
            if(f.getName().equals("JNI_OnLoad") || f.getName().startsWith("Java_")) {
                reached.computeIfAbsent(f,k->new LinkedHashSet<>()).add("jni");
                distance.put(f,0);
            }
        }
        List<Function> candidates=new ArrayList<>(reached.keySet());
        candidates.sort(Comparator
            .comparingInt((Function f)->distance.getOrDefault(f,99))
            .thenComparingInt((Function f)->-reached.get(f).size())
            .thenComparing(GhidraGameCodeMap::addr));
        DecompInterface decompiler=new DecompInterface();
        decompiler.toggleCCode(false);
        decompiler.toggleSyntaxTree(true);
        decompiler.setSimplificationStyle("decompile");
        decompiler.openProgram(currentProgram);

        StringBuilder result=new StringBuilder("{");
        stringField(result,"kind","arm32-native-game-code-reconstruction-map");
        result.append(",\"schema\":1,\"analyzed_functions\":").append(internals.size());
        result.append(",\"jni_export_count\":").append(jni.size());
        result.append(",\"jni_symbols\":[");
        // Names are ABI metadata from the public upstream APK. No bodies.
        for(int i=0;i<Math.min(35,jni.size());++i) {
            if(i>0)result.append(',');
            result.append(escape(jni.get(i)));
        }
        result.append("],\"public_import_caller_counts\":{");
        int idx=0;
        for(Map.Entry<String,Integer> entry:counts.entrySet()) {
            if(idx++>0)result.append(',');
            result.append(escape(entry.getKey())).append(':').append(entry.getValue());
        }
        result.append("},\"candidate_functions\":[");
        for(int i=0;i<Math.min(MAX_FUNCTIONS,candidates.size());++i) {
            if(i>0)result.append(',');
            Function f=candidates.get(i);
            result.append("{\"entry\":").append(escape(addr(f)));
            result.append(",\"distance\":").append(distance.getOrDefault(f,99));
            result.append(",\"code_address_count\":").append(f.getBody().getNumAddresses());
            result.append(",\"direct_callers\":").append(directCallers(f));
            result.append(",\"anchor_families\":[");
            int n=0;
            List<String> fam=new ArrayList<>(reached.get(f));
            Collections.sort(fam);
            for(String key:fam) {
                if(n++>0)result.append(',');
                result.append(escape(key));
            }
            result.append("]");
            try {
                DecompileResults decomp=decompiler.decompileFunction(
                    f,MAX_DECOMPILATION_SECONDS,monitor);
                HighFunction high=decomp.getHighFunction();
                if(high!=null) {
                    Map<String,Integer> opcodes=new TreeMap<>();
                    Iterator<PcodeOpAST> opIt=high.getPcodeOps();
                    int total=0;
                    while(opIt.hasNext() && total<300000) {
                        PcodeOpAST op=opIt.next(); ++total;
                        String opName=op.getMnemonic();
                        if(opName.equals("CALL") || opName.equals("CALLIND") ||
                           opName.equals("CBRANCH") || opName.equals("BRANCH") ||
                           opName.equals("BRANCHIND") || opName.equals("RETURN") ||
                           opName.equals("LOAD") || opName.equals("STORE") ||
                           opName.equals("MULTIEQUAL")) {
                            opcodes.merge(opName,1,Integer::sum);
                        }
                    }
                    result.append(",\"pcode_ops\":").append(total);
                    result.append(",\"control_flow\":{");
                    int j=0;
                    for(Map.Entry<String,Integer> count:opcodes.entrySet()) {
                        if(j++>0)result.append(',');
                        result.append(escape(count.getKey())).append(':')
                            .append(count.getValue());
                    }
                    result.append('}');
                } else result.append(",\"decompilation\":\"unavailable\"");
            } catch(Exception failure) {
                result.append(",\"decompilation\":\"failed\"");
            }
            result.append('}');
        }
        result.append("],\"critical_runtime_abi\":[");
        final String[] critical = {
            "JNI_OnLoad",
            "Java_com_ea_DeadSpace_DeadSpace_runEntryPoint",
            "Java_com_ea_blast_MainActivity_NativeOnCreate",
            "Java_com_ea_blast_MainActivity_NativeOnPause",
            "Java_com_ea_blast_MainActivity_NativeOnResume",
            "Java_com_ea_blast_MainActivity_NativeOnStop",
            "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
            "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged",
            "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
            "Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent",
            "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown",
            "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp",
            "Java_com_ea_EAIO_EAIO_Startup",
            "Java_com_ea_EAIO_EAIO_Shutdown"
        };
        int emitted=0;
        for (String name: critical) {
            Function target=null;
            for (Function f:internals)
                if (f.getName().equals(name)) {target=f;break;}
            if(target==null)continue;
            if(emitted++>0)result.append(',');
            result.append("{\"jni_export\":").append(escape(name));
            result.append(",\"native_entry\":").append(escape(addr(target)));
            result.append(",\"native_code_addresses\":").append(target.getBody().getNumAddresses());
            result.append(",\"direct_native_callees\":[");
            List<String> child=new ArrayList<>();
            for (Function callee:target.getCalledFunctions(monitor)) {
                if (!callee.isExternal())
                    child.add(addr(callee));
                else {
                    for(String publicAnchor:ANCHORS)
                        if (callee.getName().equals(publicAnchor))
                            child.add("API:"+publicAnchor);
                }
                if(child.size()>=40)break;
            }
            Collections.sort(child);
            for(int i=0;i<child.size();i++) {
                if(i>0)result.append(',');
                result.append(escape(child.get(i)));
            }
            result.append("],\"ordered_direct_call_sites\":[");
            int siteCount=0;
            InstructionIterator calls=currentProgram.getListing().getInstructions(
                target.getBody(),true);
            while(calls.hasNext() && siteCount<100) {
                Instruction instruction=calls.next();
                for(Reference reference:instruction.getReferencesFrom()) {
                    if(!reference.getReferenceType().isCall()) continue;
                    Function called=currentProgram.getFunctionManager().getFunctionAt(
                        reference.getToAddress());
                    if(called==null) called=currentProgram.getFunctionManager()
                        .getFunctionContaining(reference.getToAddress());
                    if(called==null) continue;
                    if(siteCount++>0) result.append(',');
                    result.append("{\"at\":").append(escape("0x"+instruction.getAddress()));
                    result.append(",\"target\":").append(escape(
                        called.isExternal() ? "API:"+called.getName() : addr(called)));
                    result.append('}');
                    if(siteCount>=100) break;
                }
            }
            result.append("]}");
        }
        result.append("],\"entrypoint_native_call_chain\":[");
        Function gameEntry=null;
        for(Function f:internals)
            if(f.getName().equals("Java_com_ea_DeadSpace_DeadSpace_runEntryPoint")) {
                gameEntry=f; break;
            }
        appendEntryChain(result,gameEntry,0,new LinkedHashSet<String>(),100);
        // Original ELF dynsym runEntryPoint has ELF vaddr 0x00035de8.
        // Ghidra imported the library with a non-zero image base. Never
        // confuse ELF vaddrs with Ghidra's re-based listing addresses.
        if(gameEntry==null)throw new IllegalStateException(
            "Missing original JNI game entry for Ghidra load bias proof");
        long ghidraBias=gameEntry.getEntryPoint().getOffset()-0x00035de8L;
        if(ghidraBias<0 || ghidraBias>0x10000000L)
            throw new IllegalStateException("Unexpected original ELF import bias");
        long verifiedFrameElfVaddr=0x0027ef68L;
        ghidra.program.model.address.Address addrAt=
            toAddr(verifiedFrameElfVaddr+ghidraBias);
        result.append("],\"ghidra_image_load_bias\":")
            .append(escape(String.format("0x%x",ghidraBias)));
        result.append(",\"original_elf_frame_target\":")
            .append(escape(String.format("0x%x",verifiedFrameElfVaddr)));
        result.append(",\"mapped_ghidra_frame_target\":")
            .append(escape("0x"+addrAt.toString()));
        result.append(",\"frame_update_native_call_chain\":[");
        Function realFrame=currentProgram.getFunctionManager().getFunctionAt(addrAt);
        if(realFrame==null)realFrame=currentProgram.getFunctionManager()
            .getFunctionContaining(addrAt);
        appendEntryChain(result,realFrame,0,new LinkedHashSet<String>(),75);
        result.append("],\"frame_branch_target_instruction_window\":[");
        Instruction frameInstruction=currentProgram.getListing().getInstructionAt(addrAt);
        for(int n=0;frameInstruction!=null && n<44;n++) {
            if(n>0)result.append(',');
            result.append("{\"at\":").append(escape("0x"+frameInstruction.getAddress()));
            result.append(",\"mnemonic\":").append(escape(frameInstruction.getMnemonicString()));
            // Operands are a bounded *instruction-level* research sample, not
            // a full proprietary source listing or exported library bytes.
            result.append(",\"operands\":")
                .append(escape(frameInstruction.toString().replaceFirst(
                    "^\\\\S+\\\\s*", "")));
            result.append(",\"flows\":[");
            ghidra.program.model.address.Address[] flows=frameInstruction.getFlows();
            for(int k=0;k<Math.min(flows.length,5);k++) {
                if(k>0)result.append(',');
                result.append(escape("0x"+flows[k]));
            }
            result.append("]}");
            frameInstruction=frameInstruction.getNext();
        }
        result.append("],\"frame_callback_exact_return_only\":");
        Function frame=null;
        for(Function f:internals)
            if(f.getName().equals("Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame")) {
                frame=f;break;
            }
        boolean bxOnly=false;
        if(frame!=null) {
            InstructionIterator ins=currentProgram.getListing()
                .getInstructions(frame.getBody(),true);
            if(ins.hasNext()) {
                Instruction one=ins.next();
                bxOnly=!ins.hasNext() &&
                    one.getMnemonicString().equalsIgnoreCase("bx") &&
                    one.getDefaultOperandRepresentation(0).equalsIgnoreCase("lr");
            }
        }
        result.append(bxOnly);
        result.append(",");
        stringField(result,"limitations",
            "Function and calling-API evidence only. ARM code is not translated; "+
            "indirect calls, stripped symbols and original game control flow "+
            "require further function-level analysis. No gameplay verified.");
        result.append("}\n");
        Files.writeString(Path.of(args[0]),result.toString(),StandardCharsets.UTF_8);
        println("Original ARM function map complete; only numeric/control-flow metadata emitted.");
    }
}
