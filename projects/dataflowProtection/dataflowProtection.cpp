#define DEBUG_TYPE "dataflowProtection"

#include "dataflowProtection.h"

#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/CommandLine.h"

using namespace llvm;

//--------------------------------------------------------------------------//
// Command line options for the pass
//--------------------------------------------------------------------------//
// Replication rules
cl::opt<bool> noMemReplicationFlag ("noMemReplication", cl::desc("Do not duplicate variables in memory"));
cl::opt<bool> noLoadSyncFlag ("noLoadSync", cl::desc("Do not synchronize on data loads"));
cl::opt<bool> noStoreDataSyncFlag ("noStoreDataSync", cl::desc("Do not synchronize data on data stores"));
cl::opt<bool> noStoreAddrSyncFlag ("noStoreAddrSync", cl::desc("Do not synchronize address on data stores"));
cl::opt<bool> storeDataSyncFlag ("storeDataSync", cl::desc("Force synchronize data on data stores (not default)"));

// Replication scope
// note: any changes to list names must also be changed at the top of interface.cpp
cl::list<std::string> skipFnCl ("ignoreFns", cl::desc("Specify function to not protect. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> ignoreGlblCl ("ignoreGlbls", cl::desc("Specify global variables to not protect. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> skipLibCallsCl ("skipLibCalls", cl::desc("Specify library calls to not clone. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> replicateUserFunctionsCallCl ("replicateFnCalls", cl::desc("Specify user calls where the call, not the function body, should be triplicated. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> isrFunctionListCl ("isrFunctions", cl::desc("These functions are considered Interrupt Service Handlers and will be treated differently."), cl::CommaSeparated, cl::ZeroOrMore);
// should also be able to specify functions/globals to clone from command line
cl::list<std::string> cloneFnCl ("cloneFns", cl::desc("Specify function(s) to protect. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> cloneGlblCl ("cloneGlbls", cl::desc("Specify global(s) to protect. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
// specify function names which should return multiple values
cl::list<std::string> replReturnCl ("cloneReturn", cl::desc("Specify function(s) which should return multiple values. Defaults to none."), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> cloneAfterCallCl ("cloneAfterCall", cl::desc("Specify function(s) of which the argument(s) should be cloned after the function is called once (ie. scanf)"), cl::CommaSeparated, cl::ZeroOrMore);
cl::list<std::string> protectedLibCl ("protectedLibFn", cl::desc("Specify function(s) which should be treated as protected library functions."), cl::CommaSeparated, cl::ZeroOrMore);

// Other options
cl::opt<std::string> configFileLocation ("configFile", cl::desc("Location of configuration file"));
cl::opt<bool> ReportErrorsFlag ("countErrors", cl::desc("Instrument TMR'd code so it counts the number of corrections"), cl::value_desc("TMR error counting"));
cl::opt<bool> OriginalReportErrorsFlag ("reportErrors", cl::desc("Instrument TMR'd code so it reports if TMR corrected an error (deprecated)"), cl::value_desc("TMR error signaling (deprecated)"));
cl::opt<bool> InterleaveFlag ("i", cl::desc("Interleave instructions, rather than segmenting within a basic block. Default behavior."));
cl::opt<bool> SegmentFlag ("s", cl::desc("Segment instructions, rather than interleaving within a basic block"));
cl::list<std::string> globalsToRuntimeInitCl ("runtimeInitGlobals", cl::CommaSeparated, cl::ZeroOrMore);
cl::opt<bool> dumpModuleFlag ("dumpModule", cl::desc("Print out the module immediately before pass concludes. Option is for pass debugging."));
cl::opt<bool> verboseFlag ("verbose", cl::desc("Increase the amount of output"));
cl::opt<bool> noMainFlag ("noMain", cl::desc("There is no 'main' function in this module"));
cl::opt<bool> noCloneOperandsCheckFlag ("noCloneOpsCheck", cl::desc("Continue compilation even if instruction operands weren't correctly cloned."));
cl::opt<bool> countSyncsFlag ("countSyncs", cl::desc("Dynamic count of synchronization points"));
cl::opt<bool> protectStackFlag ("protectStack", cl::desc("Vote on values of return address and frame pointer before returning from function call."));


//--------------------------------------------------------------------------//
// Top level behavior
//--------------------------------------------------------------------------//
char dataflowProtection::ID = 0;
static RegisterPass<dataflowProtection> X("DataflowProtection",
		"Insert copies of IR to protect dataflow", false, false);

bool dataflowProtection::runOnModule(Module &M) {
	// Needed for the dataflowProtection pass to compile, never used in practice
	run(M,2);
	return true;
}

bool dataflowProtection::run(Module &M, int numClones) {
	// Remove user functions that are never called in the module to reduce code size, processing time
	// These are mainly inlined by prior optimizations
	removeUnusedFunctions(M);

	// Process user commands inside of the source code
	// Must happen before processCommandLine to make sure we don't clone things if not needed
	processAnnotations(M);

	// Remove annotations here so they aren't cloned
	removeAnnotations(M);

	// Make sure that the command line options are correct
	processCommandLine(M, numClones);

	// Populate the list of functions to touch
	populateFnWorklist(M);

	// First figure out which instructions are going to be cloned
	populateValuesToClone(M);

	// validate that the configuration parameters can be followed safely
	verifyOptions(M);

	// Now add new arguments to functions
	// (In LLVM you can't change a function signature, so we have to make new functions)
	// populateValuesToClone has to be called before this so we know which
	// instructions are cloned, and thus when functions need to have extra arguments
	cloneFunctionArguments(M);
	cloneFunctionReturnVals(M);

	// deal with function wrappers
	updateFnWrappers(M);

	// Parse the annotations on local variables within functions so that
	//  list of values to clone is up to date
	processLocalAnnotations(M);
	removeLocalAnnotations(M);

	// Once again figure out which instructions are going to be cloned
	// This need to be re-run after creating the new functions as the old
	// pointers will be stale
	populateValuesToClone(M);

	// Do the actual cloning
	cloneGlobals(M);
	cloneConstantExpr();
	cloneInsns();

	// Change clones to depend on the duplications
	updateCallInsns(M);
	updateInvokeInsns(M);

	// Insert error detection/handling
	insertErrorFunction(M, numClones);
	createErrorBlocks(M, numClones);

	// Determine where synchronization logic needs to be
	populateSyncPoints(M);

	// Insert synchronization statements
	processSyncPoints(M, numClones);

	// Global runtime initialization
	addGlobalRuntimeInit(M);
	updateRRFuncs(M);

	// stack protection
	insertStackProtection(M);

	// Clean up
	removeUnusedErrorBlocks(M);
	checkForUnusedClones(M);
	removeOrigFunctions();
	removeUnusedGlobals(M);

	// This is executed if code is segmented instead of interleaved
	moveClonesToEndIfSegmented(M);

	if (verboseFlag)
		PRINT_STRING("Removing unused functions...");
	/*
	 * Final check for unused functions.
	 * It's possible that there are circular dependencies here.
	 * For example, not removing a function because it's still used in a call,
	 *  but then removing the function that had that call in it right after.
	 * Keep calling until nothing new is removed.
	 */
	int numRemoved = 0;
	do {
		numRemoved = removeUnusedFunctions(M);
	} while (numRemoved > 0);
	// Make sure old calls to functions with replicated return values are removed
	validateRRFuncs();

	// Option executed when -dumpModule is passed in
	dumpModule(M);

	return true;
}

// set pass dependencies
void dataflowProtection::getAnalysisUsage(AnalysisUsage& AU) const {
	ModulePass::getAnalysisUsage(AU);
}
