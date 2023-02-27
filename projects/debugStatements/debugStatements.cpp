//===- debugStatements.cpp - Insert print statements to aid in debugging----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Debug.h>

using namespace llvm;

#define DEBUG_TYPE "debugStatements"

// list of functions to add print statements to
// if nothing, do all
cl::list<std::string> fnPrintList("fnPrintList", cl::desc("Specify functions to instrument. Defaults to all."), cl::CommaSeparated, cl::ZeroOrMore);


//--------------------------------------------------------------------------//
// Top level behavior
//--------------------------------------------------------------------------//
class DebugStatements : public ModulePass {
public:
	static char ID;             // Pass identification
	DebugStatements() : ModulePass(ID) { }

	bool runOnModule(Module &M);
	GetElementPtrInst* getGEPforPrint(StringRef* varName, BasicBlock*& bb);

private:

};

char DebugStatements::ID = 0;
static RegisterPass<DebugStatements> X("DebugStatements",
		"Insert print statements into the IR", false, true);

bool DebugStatements::runOnModule(Module &M) {

	//Get a reference to the print statement
	Type *charPointerType = PointerType::get(IntegerType::
			get(M.getContext(), 8), 0);
	Type* type_i32 = Type::getInt32Ty(M.getContext());
	FunctionType *printfTy = FunctionType::get(type_i32, std::vector<Type*> (1, charPointerType), true);
	FunctionCallee print = M.getOrInsertFunction("printf", printfTy);
	assert(print && "Print function not defined");

	//Define constant strings
	StringRef arrow = StringRef("-->");
	StringRef newLineChar = StringRef("\n");

	bool specificFlag = (fnPrintList.size() > 0);

	for (auto &F : M) {
		if (F.getBasicBlockList().size()  == 0)
			continue;

		// if there's something in the list, and this function isn't, continue
		if (specificFlag && (std::find(fnPrintList.begin(), fnPrintList.end(), F.getName().str()) == fnPrintList.end()) ) {
			continue;
		}
//		errs() << F.getName() << "\n";

		BasicBlock* entryBlock = &F.getEntryBlock();

		//Variable def'ns
		StringRef fnName = F.getName();
		std::vector<Value*> fnArgs;
		std::vector<Value*> argsArrow;
		std::vector<Value*> newlineArgs;

		//Arguments for printing the function name
		GetElementPtrInst* fnGEP = getGEPforPrint(&fnName, entryBlock);
		fnArgs.push_back(fnGEP);
		ArrayRef<Value*>* fnCallArgs = new ArrayRef<Value*>(fnArgs);

		//Arguments for printing an arrow
		GetElementPtrInst* arrowGEP = getGEPforPrint(&arrow, entryBlock);
		argsArrow.push_back(arrowGEP);
		ArrayRef<Value*>* callArgsArrow = new ArrayRef<Value*>(argsArrow);

		//Arguments for printing a new line
		GetElementPtrInst* newlineGEP = getGEPforPrint(&newLineChar, entryBlock);
		newlineArgs.push_back(newlineGEP);
		ArrayRef<Value*>* callArgsNewline = new ArrayRef<Value*>(newlineArgs);

		for (auto & bb : F) {
			StringRef bbName = bb.getName();
			BasicBlock* currBB = &bb;
			std::vector<Value*> bbArgs;

			//Arguments for printing the name of the basic block
			GetElementPtrInst* bbGEP = getGEPforPrint(&bbName, currBB);
			bbArgs.push_back(bbGEP);
			ArrayRef<Value*>* bbCallArgs = new ArrayRef<Value*>(bbArgs);

			//Create all the function calls: function arrow bb newline
			Instruction* insertionPoint = &*bb.getFirstNonPHI();
			if (isa<LandingPadInst>(insertionPoint)) {
				insertionPoint = insertionPoint->getNextNode();
			}
			CallInst* newlinePrint = CallInst::Create(print, *callArgsNewline, "", insertionPoint);
			CallInst* bbPrint = CallInst::Create(print, *bbCallArgs, "", newlinePrint);
			CallInst* arrowPrint = CallInst::Create(print, *callArgsArrow, "", bbPrint);
			CallInst* fnNamePrint = CallInst::Create(print, *fnCallArgs, "", arrowPrint);

			//Make bbGEP dominate all uses
			bbGEP->moveBefore(fnNamePrint);
		}

		//Move all the GEPs to the front of the entry block to dominate all uses
		Instruction* insertionPoint = entryBlock->getFirstNonPHI();
		if (isa<LandingPadInst>(insertionPoint)) {
			insertionPoint = insertionPoint->getNextNode();
		}
		newlineGEP->moveBefore(insertionPoint);
		arrowGEP->moveBefore(newlineGEP);
		fnGEP->moveBefore(arrowGEP);
	}

	return true;
}

GetElementPtrInst* DebugStatements::getGEPforPrint(StringRef* varName, BasicBlock*& bb){

	Module* currModule = bb->getParent()->getParent();
	Type* type_i8 = Type::getInt8Ty(currModule->getContext());

	//Create a char array (i8s)
	ArrayType * type_i8_array = ArrayType::get(type_i8,(unsigned long long int)(varName->size()+1));
	Constant * dataInit = ConstantDataArray::getString(currModule->getContext(), *varName);

	//Create a global variable and init to the array of i8s
	GlobalVariable * globalVal = dyn_cast<GlobalVariable>(
			currModule->getOrInsertGlobal(*varName, type_i8_array));
	globalVal->setConstant(true);
	globalVal->setInitializer(dataInit);
	globalVal->setLinkage(GlobalVariable::PrivateLinkage);
	globalVal->setUnnamedAddr( GlobalValue::UnnamedAddr() );
	globalVal->setAlignment(MaybeAlign(1));


	//Create constants for GEP arguments
	ConstantInt* zeroCI = ConstantInt::get(IntegerType::getInt32Ty(currModule->getContext()),0,false);
	Value* zeroVal = dyn_cast<Value>(zeroCI);

	//Assemble the GEP instruction
	std::vector<Value*> gepArgs;
	gepArgs.push_back(zeroVal);
	gepArgs.push_back(zeroVal);
	ArrayRef<Value*>* gepArgsArray;
	gepArgsArray = new ArrayRef<Value*>(gepArgs);

	//Insert the instruction into basic block
	GetElementPtrInst* gep = GetElementPtrInst::CreateInBounds(type_i8_array,
			globalVal,*gepArgsArray,varName->str(),bb->getTerminator());

	return gep;
}

