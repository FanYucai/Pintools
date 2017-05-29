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
std::map<UINT64, UINT64> AddrIntMap;
std::map<UINT64, string> AddrNameMap;

static void OnInstruction(ADDRINT unsigned_value, long signed_value, bool is_signed, INT32 length_bits)
{
    OutFile << "Contains an immediate value: " << "0x" << hex << setfill('0') << setw(length_bits/4)
             << unsigned_value << setfill(' ') << setw(1) << dec;
    UINT32 bitcount = 0;
    while(unsigned_value != 0) {
        unsigned_value = unsigned_value >> 1;
        bitcount++;
    }
    if(ImdtMap.count(bitcount)>0) {
        ImdtMap[bitcount]++;
    } else {
        ImdtMap[bitcount] = 1;
    }     

    if (is_signed)
    {
        OutFile << " (signed: " << signed_value << "),";
    }
    OutFile << " bitcount: " << bitcount << endl;
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
AddrNameMap[(UINT64)0] = "立即数寻址";
AddrNameMap[(UINT64)1] = "寄存器寻址";
AddrNameMap[(UINT64)2] = "直接寻址";
AddrNameMap[(UINT64)8] = "寄存器间接寻址";
AddrNameMap[(UINT64)10] = "寄存器相对寻址";
AddrNameMap[(UINT64)12] = "基址变址寻址";
AddrNameMap[(UINT64)14] = "相对基址变址寻址";
AddrNameMap[(UINT64)7] = "比例变址寻址";
AddrNameMap[(UINT64)13] = "基址比例变址寻址";
AddrNameMap[(UINT64)15] = "相对基址比例变址寻址";
	

    // instructions
    insStr = INS_Mnemonic(ins);
    if(InsMap.count(insStr)>0) {
    	InsMap[insStr]++;
    } else {
    	InsMap[insStr] = 1;
    }
    // OutFile << insStr << " ";
    

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
    UINT32 ImmFlag = 0;
    UINT32 MemFLag = 0;
    UINT32 RegFlag = 0;
    
    UINT32 n = INS_OperandCount(ins);
    for(UINT32 i=0; i<n; i++) {
        UINT64 BIDS = 0;
    	if(INS_OperandIsImmediate(ins, i)) {
    		// Get the value itself
            ADDRINT value = INS_OperandImmediate(ins, i);
            long signed_value = (long)value;
            // Get length information
            INT32 length_bits = -1;
            bool is_signed = false;
            GetOperLenAndSigned(ins, i, length_bits, is_signed);
            OnInstruction(value, signed_value, is_signed, length_bits);
    		// OutFile << "imm" << ", ";
            ImmFlag = 1;
    	}
    	if(INS_OperandIsReg(ins, i)) {
            RegFlag = 1;
            // OutFile << "reg" << ", ";
        }
    	if(INS_OperandIsMemory(ins, i)) {
            MemFLag = 1;
            // OutFile << "mem" << ", ";
            // OutFile << "BIDS: ";
            if(INS_OperandMemoryBaseReg(ins, i) != REG_INVALID()) {
                // OutFile << "1";
                BIDS += 8;
            } else {
                // OutFile << "0";
            }
            if(INS_OperandMemoryIndexReg(ins, i) != REG_INVALID()) {
                // OutFile << "1";
                BIDS += 4;
            } else {
                // OutFile << "0";
            }
            if(INS_OperandMemoryDisplacement(ins, i) != (UINT32)0) {
                // OutFile << "1";
                BIDS += 2;
            } else {
                // OutFile << "0";
            }
            if(INS_OperandMemoryScale(ins, i) != (UINT32)1) {
                // OutFile << "1";
                BIDS += 1;
            } else {
                // OutFile << "0";
            }
            // OutFile << INS_OperandMemoryDisplacement(ins, i) << "|";
            // OutFile << INS_OperandMemoryScale(ins, i) << "|, ";    
            // OutFile << ", ";
            
            if(BIDS != 0) {
                if(AddrIntMap.count(BIDS)>0) {
                    AddrIntMap[BIDS]++;
                } else {
                    AddrIntMap[BIDS] = 1;
                }               
            }
        }
    	// if(INS_OperandIsImplicit(ins, i)) {
     //        OutFile << "imp" << ", ";
     //    }	

    }
    
    if(ImmFlag == 1 && MemFLag == 0) {
        if(AddrIntMap.count((UINT64)0)>0) {
            AddrIntMap[(UINT64)0]++;
        } else {
            AddrIntMap[(UINT64)0] = 1;
        }
    } else if(RegFlag == 1 && MemFLag == 0) {
        if(AddrIntMap.count((UINT64)0)>0) {
            AddrIntMap[(UINT64)1]++;
        } else {
            AddrIntMap[(UINT64)1] = 1;
        }
    }

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
    
    OutFile << endl;
    OutFile << "______________" << " 指令统计：" << "______________" << endl;
	instot = 0;
    for(std::map<string, UINT64>::iterator it=InsMap.begin(); it != InsMap.end(); ++it) {
    	instot += it->second;
    }

    OutFile << "tot: " << instot << endl;
    for(std::map<string, UINT64>::iterator it=InsMap.begin(); it != InsMap.end(); ++it) {
    	OutFile << it->first << 
    	", " << it->second <<
    	", " << it->second / (0.01*instot) << "%" << endl;
    }


    OutFile << endl;
    OutFile << "______________" << " 立即数统计：" << "______________" << endl;
    instot = 0;   	
    for(std::map<UINT64, UINT64>::iterator it=ImdtMap.begin(); it != ImdtMap.end(); ++it) {
    	instot += it->second;
    }
    OutFile << "tot: " << instot << endl;
    for(std::map<UINT64, UINT64>::iterator it= ImdtMap.begin(); it != ImdtMap.end(); ++it) {
    	OutFile << "有效位数: " << it->first << 
    	", " << it->second <<
    	", " << it->second / (0.01*instot) << "%" << endl;
    }

    OutFile << endl;
    OutFile << "______________" << " 寻址方式统计：" << "______________" << endl;
    instot = 0;
    for(std::map<UINT64, UINT64>::iterator it=AddrIntMap.begin(); it != AddrIntMap.end(); ++it) {
        instot += it->second;
    }
    OutFile << "tot: " << instot << endl;
    for(std::map<UINT64, UINT64>::iterator it= AddrIntMap.begin(); it != AddrIntMap.end(); ++it) {
        OutFile << "寻址方式：" << AddrNameMap[it->first] << 
        ", 次数: " << it->second <<
        ", 占比: " << it->second / (0.01*instot) << "%" << endl;
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
