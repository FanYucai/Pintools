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

typedef struct distCount
{
    vector<REG> _RregVec;
    vector<REG> _WregVec;
    struct distCount * _next;
} DIST_COUNT;

ofstream OutFile;
map<UINT64, UINT64> finalCount;
map<REG, UINT64> distMap;
vector<REG> regW;
vector<REG> regR;

VOID docount(vector<REG>* regR_ptr)
{
    regR = *regR_ptr;
    for(vector<REG>::iterator it=regR.begin(); it != regR.end(); ++it) {
        if(distMap.count((*it)) > 0) {
            UINT32 step = distMap[*it];
            if(finalCount.count(step) > 0) {
                finalCount[step]++;
            } else {
                finalCount[step] = 1;
            }
        }
    }   
}

VOID fillReg(vector<REG>* regW_ptr)
{
    regW = *regW_ptr;
    for(vector<REG>::iterator it=regW.begin(); it != regW.end(); ++it) {
        distMap[*it] = 0;
    }
}

VOID autoAdd()
{
    for(map<REG, UINT64>::iterator it=distMap.begin(); it!=distMap.end(); ++it) {
        it->second += 1;
    }
    // 每执行一个指令距离加一
}

VOID Instruction(INS ins, VOID *v)
{   
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)autoAdd, IARG_END);
    DIST_COUNT *distc = new DIST_COUNT;
    vector<REG> vec1;
    vector<REG> vec2;
    distc->_RregVec = vec1;
    distc->_WregVec = vec2;
    UINT32 n = INS_OperandCount(ins);
    
    for(UINT32 i=0; i<n; i++) {
        if(INS_OperandIsReg(ins, i) && INS_OperandRead(ins, i)){
            REG _reg;
            _reg = INS_OperandReg(ins, i);
            distc->_RregVec.push_back(_reg);
        } else if(INS_OperandIsReg(ins, i) && INS_OperandWritten(ins, i)) {
            // 获取reg名
            REG _reg;
            _reg = INS_OperandReg(ins, i);
            distc->_WregVec.push_back(_reg);
        }
    } 
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(distc->_RregVec), IARG_END);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)fillReg, IARG_PTR, &(distc->_WregVec), IARG_END);
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "task2.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    UINT64 instot = 0;
    OutFile << endl;
    OutFile << "______________" << " 依赖距离统计：" << "______________" << endl;
    instot = 0;
    for(std::map<UINT64, UINT64>::iterator it=finalCount.begin(); it != finalCount.end(); ++it) {
        instot += it->second;
    }

    OutFile << "tot: " << instot << endl;
    for(std::map<UINT64, UINT64>::iterator it=finalCount.begin(); it != finalCount.end(); ++it) {
        OutFile << it->first << 
        ", " << it->second <<
        ", " << it->second / (0.01*instot) << "%" << endl;
        if(it->first >= 30) break;
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
