#include <iostream>
// #include "gcc_common.h"
// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "coretypes.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "memmodel.h"
#include "rtl.h"
#include "emit-rtl.h"

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

static struct plugin_info my_gcc_plugin_info = { "1.0", "Stack canary plugin" };
static int rng_guard_value;

namespace
{
    const pass_data stack_canary_data = 
    {
        RTL_PASS,
        "stack_canary",        /* name */
        OPTGROUP_NONE,          /* optinfo_flags */
        TV_NONE,                /* tv_id */
        PROP_gimple_any,        /* properties_required */
        0,                      /* properties_provided */
        0,                      /* properties_destroyed */
        0,                      /* todo_flags_start */
        0                       /* todo_flags_finish */
    };

    struct stack_canary : gimple_opt_pass
    {
        stack_canary(gcc::context *ctx)
            : gimple_opt_pass(stack_canary_data, ctx)
        {
        }


        static void insert_value(rtx insn)
        {
            rtx dec;

            
            // actions
            rtx xor_action;
            rtx sub_action;
            rtx mov_action;
            rtx psh_action;

            // helpers
            rtx mem;
            rtx label;
            rtx last;
            rtx tmp;;

            // Registers
            rtx rax = gen_rtx_REG(DImode, AX_REG);
            rtx rcx = gen_rtx_REG(DImode, CX_REG);
            rtx rdi = gen_rtx_REG(DImode, DI_REG);
            rtx rsp = gen_rtx_REG(DImode, SP_REG);

            // call rand()
            tmp = gen_rtx_SYMBOL_REF (Pmode, "rand");
            tmp = gen_rtx_CALL (VOIDmode, gen_rtx_MEM (FUNCTION_MODE, tmp), const0_rtx);
            emit_insn_before(tmp, insn);

            // push rax - save rand result in stack
            dec = gen_rtx_PRE_DEC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, dec);
            psh_action = gen_rtx_SET(mem, rax);
            emit_insn_before(psh_action, insn);

            // push rdi - save old arg
            dec = gen_rtx_PRE_DEC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, dec);
            psh_action = gen_rtx_SET(mem, rdi);
            emit_insn_before(psh_action, insn);

            // Set parameter for malloc (fastcall)
            mov_action = gen_movsi(rdi, GEN_INT(8));
            emit_insn_before(mov_action, insn);

            // call malloc(8)
            tmp = gen_rtx_SYMBOL_REF (Pmode, "malloc");
            tmp = gen_rtx_CALL (VOIDmode, gen_rtx_MEM (FUNCTION_MODE, tmp), const0_rtx);
            emit_insn_before(tmp, insn);

            // pop rdi - restore old arg
            tmp = gen_rtx_POST_INC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, tmp);
            tmp = gen_rtx_SET(rdi, mem);
            last = emit_insn_before(tmp, insn);

            // pop rcx - restore rand result
            tmp = gen_rtx_POST_INC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, tmp);
            tmp = gen_rtx_SET(rcx, mem);
            last = emit_insn_before(tmp, insn);

            // mov value, rax
            mov_action = gen_movsi(gen_rtx_MEM(Pmode, rax), rcx);
            emit_insn_before(mov_action, insn);

            // push rcx
            dec = gen_rtx_PRE_DEC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, dec);
            psh_action = gen_rtx_SET(mem, rcx);
            emit_insn_before(psh_action, insn);

            // add rsp, <sizeof(int)> //TODO: is needed??
            sub_action = gen_adddi3(stack_pointer_rtx, stack_pointer_rtx, GEN_INT(8));
            emit_insn_before(sub_action, insn);

        }


        static void check_value(rtx insn)
        {
            // Helpers
            rtx last, mem, tmp, label;

            // Actions
            rtx sub_action;

            // Registers
            rtx rbx = gen_rtx_REG(DImode, BX_REG);

            // sub rsp, <sizeof(int)> //TODO: is needed?
            sub_action = gen_subdi3(stack_pointer_rtx, stack_pointer_rtx, GEN_INT(8));
            last = emit_insn_after(sub_action, insn);

            // pop rbx
            tmp = gen_rtx_POST_INC(DImode, stack_pointer_rtx);
            mem = gen_rtx_MEM(DImode, tmp);
            tmp = gen_rtx_SET(rbx, mem);
            last = emit_insn_after(tmp, last);

            // TODO: Should cmp to the allocated value
            // cmp ALLOCATED, rbx
            tmp = gen_rtx_COMPARE(CCmode,
                rbx,
                gen_rtx_CONST_INT(VOIDmode, rng_guard_value));
            tmp = gen_rtx_SET(gen_rtx_REG(CCmode, FLAGS_REG), tmp);
            last = emit_insn_after(tmp, last);

            // jeq 
            label = gen_label_rtx(); /* Where we jump to */
            tmp = gen_rtx_EQ(VOIDmode, gen_rtx_REG(CCmode, FLAGS_REG), const0_rtx);
            tmp = gen_rtx_IF_THEN_ELSE(VOIDmode,
                tmp,                                 /* cmp               */
                gen_rtx_LABEL_REF(VOIDmode, label),  /* Ifeq              */
                pc_rtx);                             /* Else (do nothing) */
            last = emit_jump_insn_after(gen_rtx_SET(pc_rtx, tmp), last);
            JUMP_LABEL(last) = label;

            // Call abort()
            tmp = gen_rtx_SYMBOL_REF(Pmode, "abort");
            tmp = gen_rtx_CALL(Pmode, gen_rtx_MEM(QImode, tmp), const0_rtx);
            last = emit_insn_after(tmp, last);
            emit_label_after(label, last);
        }

        virtual unsigned int execute(function *fun) override
        {
            int idx;
            rtx_insn *insn;

            std::cerr << "FUNCTION '" << function_name(fun) << "\n" ;
            
            for (insn = get_insns(); insn; insn = NEXT_INSN (insn))
            {
                if (NOTE_P(insn) && (NOTE_KIND(insn) == NOTE_INSN_PROLOGUE_END)) // NOTE_INSN_PROLOGUE_END
                {
                    insert_value(insn);
                }
                    
                else if (NOTE_P(insn) && NOTE_KIND(insn) == NOTE_INSN_EPILOGUE_BEG)
                {
                    check_value(insn);
                    
                }
  
            }
            return 0;
        }

        virtual stack_canary* clone() override
        {
            // We do not clone ourselves
            return this;
        }

        private:

    };
}

int plugin_init (struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	// We check the current gcc loading this plugin against the gcc we used to
	// created this plugin
	if (!plugin_default_version_check (version, &gcc_version))
    {
        std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
		return 1;
    }

    register_callback(plugin_info->base_name,
            /* event */ PLUGIN_INFO,
            /* callback */ NULL, /* user_data */ &my_gcc_plugin_info);

    // Register the phase right after omplower
    struct register_pass_info pass_info;

    // Note that after the cfg is built, fun->gimple_body is not accessible
    // anymore so we run this pass just before the cfg one
    pass_info.pass = new stack_canary(g);
    pass_info.reference_pass_name = "pro_and_epilogue";
    pass_info.ref_pass_instance_number = 0;
    pass_info.pos_op = PASS_POS_INSERT_AFTER;

    register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

    return 0;
}
