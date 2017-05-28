#include <iostream>
#include <fstream>
#include <cstring>
#include <string.h>
#include <set>
#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "pin.H"

extern "C" {
	#include "xed-interface.h"
}


ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount

static UINT64 instot = 0;
static string insStr = "";
static string addrClass = "";
static UINT64 imdtBit = 0;

std::map<string, UINT64> InsMap;
std::map<string, UINT64> AddrMap;
std::map<UINT64, UINT64> ImdtMap;


static void OnInstruction(ADDRINT unsigned_value, long signed_value, bool is_signed, INT32 length_bits)
{
    OutFile << "Contains an immediate value: " << "0x" << hex << setfill('0') << setw(length_bits/4)
             << unsigned_value << setfill(' ') << setw(1) << dec;
    if (is_signed)
    {
        OutFile << " (signed: " << signed_value << "),";
    }
    OutFile << " operand is " << length_bits << " bits long." << endl;
}


/*
 * A routine to query the immediate operand per instruction. May be called at
 * instrumentation or analysis time.
 */
VOID GetOperLenAndSigned(INS ins, INT32 i, INT32& length_bits, bool& is_signed)
{
    xed_decoded_inst_t* xedd = INS_XedDec(ins);
    // To print out the gory details uncomment this:
    // char buf[2048];
    // xed_decoded_inst_dump(xedd, buf, 2048);
    // OutFile << buf << endl;
    length_bits = 8*xed_decoded_inst_operand_length(xedd, i);
    is_signed = xed_decoded_inst_get_immediate_is_signed(xedd);
}

// This function is called before every instruction is executed

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{	
	// instructions
    insStr = INS_Mnemonic(ins);
    if(InsMap.count(insStr)>0) {
    	InsMap[insStr]++;
    } else {
    	InsMap[insStr] = 1;
    }
    OutFile << insStr << " ";
    

    // addr
    const xed_decoded_inst_t* xedd = INS_XedDec(ins);
    const xed_iform_enum_t index=xed_decoded_inst_get_iform_enum(xedd);
    const char* buff=xed_iform_enum_t2str(index);
    addrClass = buff;
    if(AddrMap.count(addrClass)>0) {
    	AddrMap[addrClass]++;
    } else {
    	AddrMap[addrClass] = 1;
    }


    // imdt figure:

    UINT32 n = INS_OperandCount(ins);
    for(UINT32 i=0; i<n; i++) {
    	if(INS_OperandIsImmediate(ins, i)) {
    		// Get the value itself
            ADDRINT value = INS_OperandImmediate(ins, i);
            long signed_value = (long)value;
            // Get length information
            INT32 length_bits = -1;
            bool is_signed = false;
            GetOperLenAndSigned(ins, i, length_bits, is_signed);
            OnInstruction(value, signed_value, is_signed, length_bits);
    		OutFile << "imm" << ", ";
    	}
    	if(INS_OperandIsReg(ins, i)) 
    		{OutFile << "reg" << ", ";}
    	if(INS_OperandIsMemory(ins, i)) 
    		{OutFile << "mem" << ", ";}
    	if(INS_OperandIsImplicit(ins, i)) 
    		{OutFile << "imp" << ", ";}	
    }
    OutFile << endl;

    if(ImdtMap.count(imdtBit)>0) {
    	ImdtMap[imdtBit]++;
    } else {
    	ImdtMap[imdtBit] = 1;
    }


    // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_inscount, IARG_END);
    // INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)do_findcount, IARG_END);
    // INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)do_datacount, IARG_END);

}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "a.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    
	// //map operations
 //    for(std::map<string, UINT64>::iterator it=InsMap.begin(); it != InsMap.end(); ++it) {
 //    	instot += it->second;
 //    }
 //    OutFile << "tot: " << instot << endl;
 //    for(std::map<string, UINT64>::iterator it=InsMap.begin(); it != InsMap.end(); ++it) {
 //    	OutFile << "name: " << it->first << 
 //    	", count: " << it->second <<
 //    	", percent: " << it->second / (0.01*instot) << "%" << endl;
 //    }

 //   	//map operations
 //    for(std::map<string, UINT64>::iterator it=AddrMap.begin(); it != AddrMap.end(); ++it) {
 //    	instot += it->second;
 //    }
 //    OutFile << "tot: " << instot << endl;
 //    for(std::map<string, UINT64>::iterator it=AddrMap.begin(); it != AddrMap.end(); ++it) {
 //    	OutFile << "name: " << it->first << 
 //    	", count: " << it->second <<
 //    	", percent: " << it->second / (0.01*instot) << "%" << endl;
 //    }

   	//map operations
    for(std::map<UINT64, UINT64>::iterator it=ImdtMap.begin(); it != ImdtMap.end(); ++it) {
    	instot += it->second;
    }
    OutFile << "tot: " << instot << endl;
    for(std::map<UINT64, UINT64>::iterator it= ImdtMap.begin(); it != ImdtMap.end(); ++it) {
    	OutFile << "name: " << it->first << 
    	", count: " << it->second <<
    	", percent: " << it->second / (0.01*instot) << "%" << endl;
    }
    OutFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}	