#include <cstring>
#include <string.h>
#include <set>
#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include "pin.H"

extern "C" {
    #include "xed-interface.h"
}

typedef pair<string, int> PAIR;  

typedef struct InsCount
{
    string _insName;
    UINT64 _bitcount;

    UINT64 _BIDS;

    struct InsCount * _next;
} INS_COUNT;

struct CmpByValue {  
  bool operator()(const PAIR& lhs, const PAIR& rhs) {  
    return lhs.second > rhs.second;  
  }  
};  

ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
    // imdt figure:

static UINT64 instot = 0;
static string addrClass = "";

std::map<string, UINT64> InsMap;
std::map<UINT64, UINT64> ImdtMap;
std::map<UINT64, UINT64> AddrIntMap;
std::map<UINT64, string> AddrNameMap;

VOID insname_docount(string* insStr_ptr)
{
    string insStr = *insStr_ptr;
    if(InsMap.count(insStr)>0) {
        InsMap[insStr]++;
    } else {
        InsMap[insStr] = 1;
    }  
}

VOID immaddr_docount() 
{
    if(AddrIntMap.count((UINT64)0)>0) {
        AddrIntMap[(UINT64)0]++;
    } else {
        AddrIntMap[(UINT64)0] = 1;
    }    
}

VOID regaddr_docount() 
{
    if(AddrIntMap.count((UINT64)1)>0) {
        AddrIntMap[(UINT64)1]++;
    } else {
        AddrIntMap[(UINT64)1] = 1;
    }    
}

VOID addr_docount(UINT64 * BIDS_ptr)
{
    UINT64 BIDS = *BIDS_ptr;
    if(BIDS != 0) {
        if(AddrIntMap.count(BIDS)>0) {
            AddrIntMap[BIDS]++;
        } else {
            AddrIntMap[BIDS] = 1;
        }               
    }
}

VOID imm_docount(UINT32 *bitcount_ptr)
{
    UINT32 bitcount = *bitcount_ptr;
    if(ImdtMap.count(bitcount)>0) {
        ImdtMap[bitcount]++;
    } else {
        ImdtMap[bitcount] = 1;
    }   
}

static UINT32 OnInstruction(ADDRINT unsigned_value, long signed_value, bool is_signed, INT32 length_bits)
{
    // OutFile << "Contains an immediate value: " << "0x" << hex << setfill('0') << setw(length_bits/4)
    //          << unsigned_value << setfill(' ') << setw(1) << dec;
    UINT32 bitcount = 0;
    while(unsigned_value != 0) {
        unsigned_value = unsigned_value >> 1;
        bitcount++;
    }  
    return bitcount;
}

VOID GetOperLenAndSigned(INS ins, INT32 i, INT32& length_bits, bool& is_signed)
{
    xed_decoded_inst_t* xedd = INS_XedDec(ins);
    length_bits = 8*xed_decoded_inst_operand_length(xedd, i);
    is_signed = xed_decoded_inst_get_immediate_is_signed(xedd);
}

VOID Instruction(INS ins, VOID *v)
{   
    INS_COUNT *insc = new INS_COUNT;
    insc->_insName = INS_Mnemonic(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)insname_docount, IARG_PTR, &(insc->_insName), IARG_END);

    UINT32 n = INS_OperandCount(ins);

    for(UINT32 i=0; i<n; i++) {
        if(INS_OperandIsImmediate(ins, i)) {
            if(INS_OperandRead(ins, i)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)immaddr_docount, IARG_END);
            }
        }
        if(INS_OperandIsReg(ins, i)) {
            if(INS_OperandRead(ins, i)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)regaddr_docount, IARG_END);
            }
        }
    } 

    for(UINT32 i=0; i<n; i++) {
        UINT64 BIDS = 0;
        UINT32 bitcount = 0;
        if(INS_OperandIsImmediate(ins, i)) {
            // Get the value itself
            ADDRINT value = INS_OperandImmediate(ins, i);
            long signed_value = (long)value;
            // Get length information
            INT32 length_bits = -1;
            bool is_signed = false;
            GetOperLenAndSigned(ins, i, length_bits, is_signed);
            bitcount = OnInstruction(value, signed_value, is_signed, length_bits);
            insc->_bitcount = bitcount;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)imm_docount, IARG_PTR, &(insc->_bitcount), IARG_END);
        }
        if(INS_OperandIsMemory(ins, i)) {
            if(INS_OperandMemoryBaseReg(ins, i) != REG_INVALID()) {
                BIDS += 8;
            } 
            if(INS_OperandMemoryIndexReg(ins, i) != REG_INVALID()) {
                BIDS += 4;
            }
            if(INS_OperandMemoryDisplacement(ins, i) != 0) {
                BIDS += 2;
            } 
            if(INS_OperandMemoryScale(ins, i) != (UINT32)1) {
                BIDS += 1;
            } 
            insc->_BIDS = BIDS;
            if(INS_OperandRead(ins, i)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)addr_docount, IARG_PTR, &(insc->_BIDS), IARG_END);
            }
        }
    }
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "task1.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    AddrNameMap[(UINT64)0] = "立即数寻址";
    AddrNameMap[(UINT64)1] = "寄存器寻址";
    AddrNameMap[(UINT64)2] = "直接寻址";
    AddrNameMap[(UINT64)7] = "比例变址寻址";
    AddrNameMap[(UINT64)8] = "寄存器间接寻址";
    AddrNameMap[(UINT64)10] = "寄存器相对寻址";
    AddrNameMap[(UINT64)12] = "基址变址寻址";
    AddrNameMap[(UINT64)14] = "相对基址变址寻址";
    AddrNameMap[(UINT64)13] = "基址比例变址寻址";
    AddrNameMap[(UINT64)15] = "相对基址比例变址寻址";

    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    
    OutFile << endl;
    OutFile << "______________" << " 指令统计：" << "______________" << endl;
    instot = 0;
    for(std::map<string, UINT64>::iterator it=InsMap.begin(); it != InsMap.end(); ++it) {
        instot += it->second;
    }
    OutFile << "指令总数: " << instot << endl;
    //根据value排序
    vector<PAIR> InsMap_vec(InsMap.begin(), InsMap.end());  
    sort(InsMap_vec.begin(), InsMap_vec.end(), CmpByValue());  
    for (int i = 0; i != 10; ++i) {  
        OutFile << "指令名称: "
        << InsMap_vec[i].first << ", 次数: "
        << InsMap_vec[i].second << ", 百分比: "
        << InsMap_vec[i].second/(0.01*instot) << "%"
        << endl;  
    }  

    OutFile << endl;
    OutFile << "______________" << " 立即数统计：" << "______________" << endl;
    instot = 0;     
    for(std::map<UINT64, UINT64>::iterator it=ImdtMap.begin(); it != ImdtMap.end(); ++it) {
        instot += it->second;
    }
    OutFile << "立即数出现总次数: " << instot << endl;
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
    OutFile << "总计: " << instot << endl;

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
