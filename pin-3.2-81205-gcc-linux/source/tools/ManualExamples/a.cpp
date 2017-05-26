#include <iostream>
#include <fstream>
#include <cstring>
#include <string.h>
#include <set>
#include <map>
#include "pin.H"


ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 inscount[10000000] = {};
static string insname[10000000] = {};
static UINT64 instot = 0;
static UINT64 setIter = 0;
static string setStr = "";

//static UINT64 findcount[1000] = {};
//static string findclass[1000] = {};
//static UINT64 findtot = 0;

//static UINT64 datacount[1000] = {};
//static string dataclass[1000] = {};
//static UINT64 datatot = 0;

// This function is called before every instruction is executed
VOID do_inscount() 
{ 
    inscount[setIter]++;
    insname[instot++] = setStr;
}
    
// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{	
    setStr = INS_Mnemonic(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_inscount, IARG_END);
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
    //OutFile << "insCount " << inscount << endl;
    //OutFile << "findCount " << findcount << endl;
    //OutFile << "dataCount " << datacount << endl;
    
    
    for(UINT64 i=0; i<instot; i++) 
    {
        OutFile << "ins #" << i << ": " << insname[i] << endl;
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
