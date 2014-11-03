#include "pin.H"
#include "portability.H"
#include "instlib.H"
#include "histo.H"

#include <iostream>
#include <fstream>
#include <map>
#include<set>

using namespace histo;
using namespace std;
/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

#define MAP_SIZE 0x4000000 //todo: if long is 64bit, size should be larger
//#define MAP_SIZE (1<<18) //around cache size: 8M = 2**23/(2**6) = 2**17
#define WORDSHIFT1 6
#define WORDSHIFT2 2
#define Index1(x) (x>>WORDSHIFT1)%MAP_SIZE
#define Index2(x) (x>>WORDSHIFT2)%MAP_SIZE
// #define MAX_WINDOW 225 //8*28+1 do not count the window size>1 billion
typedef unsigned int stamp_t;
typedef unsigned long stamp_addr;
typedef unsigned long long stamp_tt;

const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);
//const uint32_t MAX_WINDOW = 5890; //index_to_value(5888) = 1 billion


static stamp_tt N = 0;//N: length of trace; M: maximum of footprint (total amount of different data);
static stamp_tt M[2], stamps[MAP_SIZE][2];//Used as address hash
static stamp_tt first_count_i[MAX_WINDOW][2];
static stamp_tt first_count[MAX_WINDOW][2];
static stamp_tt count_i[MAX_WINDOW][2];
static stamp_tt count_t[MAX_WINDOW][2];
struct timeval start;//start time of profiling
struct timeval finish;//stop time of profiling

stamp_tt prev_access1;
stamp_tt prev_access2;

stamp_tt prev_access;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,         "pintool",
    "o", "funcfreq.out", "specify trace file name");
KNOB<BOOL>   KnobPid(KNOB_MODE_WRITEONCE,                "pintool",
                     "i", "0", "append pid to output");

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

static INT32 Usage()
{
    cerr << "This pin tool collects frequency of call and return for each function\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}
/* ===================================================================== */
/* Class for Storing Reuse Time Frequency for Each Bin */
/* ===================================================================== */

class FUNCTIONHIST
{
  public:
    
    string funcName;
    
    stamp_tt reuseTime_i[MAX_WINDOW][2];
    stamp_tt reuseTime_t[MAX_WINDOW][2];
    
    FUNCTIONHIST() : funcName("") {}

};

/* ===================================================================== */
/* Class for Storing Reuse Distance for Each Bin */
/* ===================================================================== */

class REUSEDIST
{
public:
  
    
    double fp1;
    double fp2;
    
    REUSEDIST() : fp1(0), fp2(0) {}
    
    
    REUSEDIST(double footprint1, double footprint2) : fp1(footprint1), fp2(footprint2) {}
    
};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/*Map for Storing Reuse Tie*/
typedef map< ADDRINT, FUNCTIONHIST*> INS_HASH_SET;

static INS_HASH_SET InsSet;

/* ===================================================================== */

/*Map for Storing Reuse Tie*/
typedef map< stamp_t, REUSEDIST> REUSEDST_SET;

static REUSEDST_SET reuseDstSet;

/* ===================================================================== */

/*Start and ending addresses of executable image*/
ADDRINT highestAddress;
ADDRINT lowestAddress;

/* ===================================================================== */


/* ===================================================================== */


/*!
 A Function Call might have been previously instrumented, If so reuse the previous entry
 otherwise create a new one.
 */

static FUNCTIONHIST * Lookup( ADDRINT dstAddress)
{
    FUNCTIONHIST *& func =   InsSet[dstAddress];
    
    if( func == 0 )
    {
        func = new FUNCTIONHIST();
    }
    
    return func;
}


/* ===================================================================== */


VOID RecordMem(VOID * ip, VOID * addr,ADDRINT dest, bool userRtn)
{
    /*Address of memory accessed*/
    ADDRINT ref = (ADDRINT) addr;

    
    /*Get the function count to which instruction belongs*/
    FUNCTIONHIST *functionObject = Lookup(dest);
    

    
    if(functionObject->funcName == "")
    {
        string funcName = RTN_FindNameByAddress(dest);
        functionObject->funcName = funcName;
    }
    
   
    N++;
    stamp_t i, idx;

    
    for(i = 0; i < 2; i ++){
        if (!i) {
            idx = Index1(ref);
        }else {
            idx = Index2(ref);
        }
        
        
        stamp_tt prev_access = stamps[idx][i];
        stamps[idx][i] = N;
        
        idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS> (N - prev_access);
        
        if(!prev_access){
            M[i]++;
            first_count[idx][i] += 1;
            first_count_i[idx][i] += N - prev_access;
            
        }else{
            count_i[idx][i] += N - prev_access;
            count_t[idx][i] += 1;
            
            /************************************************/
            /*  For each function the reuse time histogram  */
            /************************************************/

            if (functionObject != NULL)
            {
                //frequency of reuse time
                functionObject->reuseTime_t[idx][i] += 1;
          
            }
            
        }
        
        
    }
    
    
}



/************************************************************************/


VOID Instruction(INS ins, VOID *v)
{
    ADDRINT addrOfIns = INS_Address(ins);
    
    //Only instrument nstructions contained in main executable
    if(addrOfIns > lowestAddress && addrOfIns < highestAddress)
    {        

        /*************************************************/
        /*  Per Function Reuse Time Freq Calculation     */
        /*************************************************/
        
    
        /*Get the routine to which the instructtion belongs - to be used in memory access*/
        RTN rtn = INS_Rtn(ins);
        ADDRINT rtnAddress = RTN_Address(rtn);
        
        if (INS_IsMemoryRead(ins)){
            
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem, IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_ADDRINT, rtnAddress,IARG_END);
        }
        if (INS_HasMemoryRead2(ins)){
            
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem, IARG_INST_PTR, IARG_MEMORYREAD2_EA, IARG_ADDRINT, rtnAddress, IARG_END);
            
        }
        if (INS_IsMemoryWrite(ins)){
            
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem, IARG_INST_PTR, IARG_MEMORYWRITE_EA,  IARG_ADDRINT, rtnAddress, IARG_END);
        }
        

    }
    
}



/* ===================================================================== */

VOID Image (IMG img, VOID *v)
{
  
    if(IMG_IsMainExecutable(img))
    {
        highestAddress = IMG_HighAddress(img);
        lowestAddress = IMG_LowAddress(img);
        
    }
    
}


/* ===================================================================== */

static std::ofstream* out = 0;
//int mapSize = 0;
VOID Fini(int n, void *v)
{
    SetAddress0x(1);
    
    
//    stamp_t i, idx, k;
//    stamp_tt ws, tmp;
    
    stamp_t i;
    stamp_tt ws;

    *out << "FUNCFREQ        1.0         0\n";  // header
    
    
    *out << "DATA:START" << endl<<endl;
    
    /*Dual Footprint And Reuse Distance*/    
    double sum=0.0, sum_i=0.0, fp, mr=1.0;
    double sum2=0.0, sum_i2=0.0, fp2, mr2=1.0;

    for(i=1;i<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);i++){
        
        ws = sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(i);
        fp = ( 1.0 * (( N - sum ) * ws + sum_i)) / ( N - ws + 1 );
        sum_i += count_i[i][0] + first_count_i[i][0];
        sum += count_t[i][0] + first_count[i][0];
        mr -= count_t[i][0] * 1.0 / N;

        
        fp2 = ( 1.0 * (( N - sum2 ) * ws + sum_i2)) / (N - ws + 1);
        sum_i2 += count_i[i][1] + first_count_i[i][1];
        sum2 += count_t[i][1] + first_count[i][1];
        mr2 -= count_t[i][1] * 1.0 / N;

        
        /*Add Reuse Distacnes for each bin idx in map*/
        REUSEDIST reuseDst(fp*(1<<WORDSHIFT1),fp2*(1<<WORDSHIFT2) );
        reuseDstSet[i] = reuseDst;
        
    }

    
    /*Per Function Output*/
    for( INS_HASH_SET::const_iterator it = InsSet.begin(); it !=  InsSet.end(); it++ )
    {
        *out<<endl<<endl;
        

        
        
        *out << "#Called Function Address "<<StringFromAddrint(it->first)<<" Name of Function is: "<<it->second->funcName<<endl;

        
        for(i=1;i<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);i++)
        {
            REUSEDIST reuseDst = reuseDstSet[i];
            
//            *out << "#CSize 1 - Index   "<<i<< "   #Reuse Distance   "<<StringFlt(reuseDst.fp1,5,8)<<"     #Reuse Time Count "<<decstr(it->second->reuseTime_t[i][0],8)
//           
//            <<"      #CSize 2 - Index   "<<i<<"    #Reuse Distance   "<<StringFlt(reuseDst.fp2,5,8)<<"     #Reuse Time Count "<<decstr(it->second->reuseTime_t[i][1],8)<<endl;
            
            
            *out <<"CSize 1 - #Reuse Distance  "<<StringFlt(reuseDst.fp1,5,8)<<"  #Reuse Time Count  "<<decstr(it->second->reuseTime_t[i][0],8)
            
            <<"     CSize 2 - #Reuse Distance  "<<StringFlt(reuseDst.fp2,5,8)<<"  #Reuse Time Count  "<<decstr(it->second->reuseTime_t[i][1],8)<<endl;
            
        }
        

      
        *out << "END OF FUNCTION" << endl<<endl;
        
    }
    
    
    *out<<endl<<endl;
    *out << "DATA:END" << endl;
    *out << "## eof\n";
    out->close();
   
    cout<<"Count of N   : "<<N<<endl;
    cout<<"Count of M1   : "<<M[0]<<endl;
    cout<<"Count of M2   : "<<M[1]<<endl;
    
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
        
    string filename =  KnobOutputFile.Value();
    if( KnobPid )
    {
        filename += "." + decstr( getpid_portable() );
    }
    out = new std::ofstream(filename.c_str());
    
    memset (stamps, 0, MAP_SIZE*sizeof(stamp_t)*2);
    memset (count_t, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (count_i, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (first_count, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (first_count_i, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    M[0] = 0;
    M[1] = 0;
    
    PIN_InitSymbols();
    
    
    
    
    IMG_AddInstrumentFunction(Image, 0);
    
    INS_AddInstrumentFunction(Instruction, 0);
    
    PIN_AddFiniFunction(Fini, 0);
    
    
    // Never returns
    PIN_StartProgram();
    
    
   
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
