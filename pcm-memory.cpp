
// written by Hosein Mohammadi Makrani
//            George Mason University



/*!     
\implementation of a simple performance counter monitoring utility
*/
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>

#include <sys/time.h> // for gettimeofday()
#endif
#include <fstream>
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <bitset>
#include "cpucounters.h"
#include "utils.h"


/*Edited by Devang*/
#include<cstdlib>
#include<sys/wait.h>
#include<sys/types.h>

#define VM_ADDRESS dev@192.168.56.102
#define DELAY 1000

#define SIZE (10000000)
#define PCM_DELAY_DEFAULT 1.0 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs
#define PCM_CALIBRATION_INTERVAL 50 // calibrate clock only every 50th iteration
#define MAX_CORES 4096


//Programmable iMC counter
#define READ 0
#define WRITE 1
#define READ_RANK_A 0
#define WRITE_RANK_A 1
#define READ_RANK_B 2
#define WRITE_RANK_B 3
#define PARTIAL 2
#define DEFAULT_DISPLAY_COLUMNS 2


//-------------- General file



using namespace std;

template <class IntType>
double float_format(IntType n)
{
    return double(n) / 1024 / 1024;
}

std::string temp_format(int32 t)
{
    char buffer[1024];
    if (t == PCM_INVALID_THERMAL_HEADROOM)
        return "N/A";

    sprintf(buffer, "%2d", t);
    return buffer;
}

std::string l3cache_occ_format(uint64 o)
{
    char buffer[1024];
    if (o == PCM_INVALID_QOS_MONITORING_DATA)
        return "N/A";

    sprintf(buffer, "%6d", (uint32) o);
    return buffer;
}


///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketBWHeader(uint32 no_columns, uint32 skt)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--             Socket "<<setw(2)<<i<<"             --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--     Memory Channel Monitoring     --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketRankBWHeader(uint32 no_columns, uint32 skt)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--               Socket "<<setw(2)<<i<<"               --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--           DIMM Rank Monitoring        --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, float* iMC_Rd_socket_chan, float* iMC_Wr_socket_chan)
{
    for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
        // check all the sockets for bad channel "channel"
        unsigned bad_channels = 0;
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            if (iMC_Rd_socket_chan[i*num_imc_channels + channel] < 0.0 || iMC_Wr_socket_chan[i*num_imc_channels + channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                ++bad_channels;
        }
        if (bad_channels == no_columns) { // the channel is missing on all sockets in the row
            continue;
        }
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- Mem Ch "<<setw(2)<<channel<<": Reads (MB/s): "<<setw(8)<<iMC_Rd_socket_chan[i*num_imc_channels+channel]<<" --|";
        }
        cout << endl;
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|--            Writes(MB/s): "<<setw(8)<<iMC_Wr_socket_chan[i*num_imc_channels+channel]<<" --|";
        }
        cout << endl;
    }
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, const ServerUncorePowerState * uncState1, const ServerUncorePowerState * uncState2, uint64 elapsedTime, int rankA, int rankB)
{
    for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
        if(rankA >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch "<<setw(2)<<channel<<" R " << setw(1) << rankA <<": Reads (MB/s): "<<setw(8)<<(float) (getMCCounter(channel,READ_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): "<<setw(8)<<(float) (getMCCounter(channel,WRITE_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
        }
        if(rankB >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch "<<setw(2) << channel<<" R " << setw(1) << rankB <<": Reads (MB/s): "<<setw(8)<<(float) (getMCCounter(channel,READ_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): "<<setw(8)<<(float) (getMCCounter(channel,WRITE_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
        }
    }
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketBWFooter(uint32 no_columns, uint32 skt, float* iMC_Rd_socket, float* iMC_Wr_socket, uint64* partial_write)
{
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Mem Read (MB/s) : "<<setw(8)<<iMC_Rd_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Mem Write(MB/s) : "<<setw(8)<<iMC_Wr_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" P. Write (T/s): "<<dec<<setw(10)<<partial_write[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Memory (MB/s): "<<setw(11)<<std::right<<iMC_Rd_socket[i]+iMC_Wr_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
}

///////////////////////////////////////////////////////////////////////////////////// pcm-memory
float display_bandwidth(float *iMC_Rd_socket_chan, float *iMC_Wr_socket_chan, float *iMC_Rd_socket, float *iMC_Wr_socket, uint32 numSockets, uint32 num_imc_channels, uint64 *partial_write, uint32 no_columns )
{
    float sysRead = 0.0, sysWrite = 0.0;
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);

    while(skt < numSockets)
    {
        // Full row
        if ( (skt+no_columns) <= numSockets )
        {
            printSocketBWHeader (no_columns, skt);
            printSocketChannelBW(no_columns, skt, num_imc_channels, iMC_Rd_socket_chan, iMC_Wr_socket_chan);
            printSocketBWFooter (no_columns, skt, iMC_Rd_socket, iMC_Wr_socket, partial_write);
            for (uint32 i=skt; i<(skt+no_columns); i++) {
                sysRead += iMC_Rd_socket[i];
                sysWrite += iMC_Wr_socket[i];
            }
            skt += no_columns;
        }
        else //Display one socket in this row
        {
            cout << "\
                \r|---------------------------------------|\n\
                \r|--             Socket "<<skt<<"              --|\n\
                \r|---------------------------------------|\n\
                \r|--     Memory Channel Monitoring     --|\n\
                \r|---------------------------------------|\n\
                \r"; 
            for(uint64 channel = 0; channel < num_imc_channels; ++channel)
            {
                if(iMC_Rd_socket_chan[skt*num_imc_channels+channel] < 0.0 && iMC_Wr_socket_chan[skt*num_imc_channels+channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                    continue;
                cout << "|--  Mem Ch "
                    <<channel
                    <<": Reads (MB/s):"
                    <<setw(8)
                    <<iMC_Rd_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n|--            Writes(MB/s):"
                    <<setw(8)
                    <<iMC_Wr_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n";

            }
            cout << "\
                \r|-- NODE"<<skt<<" Mem Read (MB/s):  "<<setw(8)<<iMC_Rd_socket[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" Mem Write (MB/s) :"<<setw(8)<<iMC_Wr_socket[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" P. Write (T/s) :"<<setw(10)<<dec<<partial_write[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" Memory (MB/s): "<<setw(8)<<iMC_Rd_socket[skt]+iMC_Wr_socket[skt]<<"     --|\n\
                \r|---------------------------------------|\n\
                \r";

            sysRead += iMC_Rd_socket[skt];
            sysWrite += iMC_Wr_socket[skt];
            skt += 1;
        }
    }
    cout << "\
        \r|---------------------------------------||---------------------------------------|\n\
        \r|--                   System Read Throughput(MB/s):"<<setw(10)<<sysRead<<"                  --|\n\
        \r|--                  System Write Throughput(MB/s):"<<setw(10)<<sysWrite<<"                  --|\n\
        \r|--                 System Memory Throughput(MB/s):"<<setw(10)<<sysRead+sysWrite<<"                  --|\n\
        \r|---------------------------------------||---------------------------------------|" << endl;
		return (sysRead+sysWrite);
}

const uint32 max_sockets = 256;
const uint32 max_imc_channels = 8;

///////////////////////////////////////////////////////////////////////////////////// pcm-memory
float calculate_bandwidth(PCM *m, const ServerUncorePowerState uncState1[], const ServerUncorePowerState uncState2[], uint64 elapsedTime, bool csv, bool & csvheader, uint32 no_columns)
{
    //const uint32 num_imc_channels = m->getMCChannelsPerSocket();
    float iMC_Rd_socket_chan[max_sockets][max_imc_channels];
    float iMC_Wr_socket_chan[max_sockets][max_imc_channels];
    float iMC_Rd_socket[max_sockets];
    float iMC_Wr_socket[max_sockets];
    uint64 partial_write[max_sockets];

    for(uint32 skt = 0; skt < m->getNumSockets(); ++skt)
    {
        iMC_Rd_socket[skt] = 0.0;
        iMC_Wr_socket[skt] = 0.0;
        partial_write[skt] = 0;

        for(uint32 channel = 0; channel < max_imc_channels; ++channel)
        {
            if(getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) == 0.0 && getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) == 0.0) //In case of JKT-EN, there are only three channels. Skip one and continue.
            {
                iMC_Rd_socket_chan[skt][channel] = -1.0;
                iMC_Wr_socket_chan[skt][channel] = -1.0;
                continue;
            }

            iMC_Rd_socket_chan[skt][channel] = (float) (getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));
            iMC_Wr_socket_chan[skt][channel] = (float) (getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));

            iMC_Rd_socket[skt] += iMC_Rd_socket_chan[skt][channel];
            iMC_Wr_socket[skt] += iMC_Wr_socket_chan[skt][channel];

            partial_write[skt] += (uint64) (getMCCounter(channel,PARTIAL,uncState1[skt],uncState2[skt]) / (elapsedTime/1000.0));
        }
    }

  float bw=0.0;
      bw=display_bandwidth(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write, no_columns);
  return bw;
}



int main(int argc, char * argv[])
{



 int memfrq=0, memch=0, memcap=0,maxf=0,numcore=0,insize=0;
   memcap = atoi(argv[1]);
   memfrq = atoi(argv[2]);
   memch = atoi(argv[3]);
   maxf = atoi(argv[4]);
   numcore = atoi(argv[5]);
   insize = atoi(argv[6]);
   
   /*Edited by Devang*/
   int pid_status=-100;
   pid_t pid,pid_result;
   uint32 count=0;
 
   ofstream myfile;
   float mbw = 0.0;
   cout << "mem freq is:" << memfrq << " channel is: " << memch << " dram capacity is: " << memcap << "core count is:" << numcore << "input size is:" << insize;
   
	
#ifdef PCM_FORCE_SILENT
    null_stream nullStream1, nullStream2;
    std::cout.rdbuf(&nullStream1);
    std::cerr.rdbuf(&nullStream2);
#endif

    cerr << endl;
//  cerr << " Intel(r) Performance Counter Monitor " << INTEL_PCM_VERSION << endl;
    cerr << " Intel(r) Performance Counter Monitor " << endl;
    cerr << endl;
//    cerr << INTEL_PCM_COPYRIGHT << endl;
    cerr << endl;

    // if delay is not specified: use either default (1 second),
    // or only read counters before or after PCM started: keep PCM blocked
   double delay = -1.0;

    char *sysCmd = NULL;
    char **sysArgv = NULL;
    bool show_core_output = true;
    bool show_partial_core_output = false;
    bool show_socket_output = true;
    bool show_system_output = true;
    bool csv_output = false;
    bool reset_pmu = false;
   
    long diff_usec = 0; // deviation of clock is useconds between measurements
    int calibrated = PCM_CALIBRATION_INTERVAL - 2; // keeps track is the clock calibration needed
    unsigned int numberOfIterations = 0; // number of iterations
    std::bitset<MAX_CORES> ycores;
	
	bool csv = false;
	bool csvheader=false;
	uint32 no_columns = DEFAULT_DISPLAY_COLUMNS; // Default number of columns is 2
	
	int rankA = -1, rankB = -1; // memory

	int imc_profile = 0; // power
	
    string program = string(argv[0]);

    PCM * m = PCM::getInstance();
   

    if (true)
    {
       // cerr << "\n Resetting PMU configuration" << endl;
       // m->resetPMU();
    }

   

    // program() creates common semaphore for the singleton, so ideally to be called before any other references to PCM
    PCM::ErrorCode status = m->program();

    switch (status)
    {
    case PCM::Success:
        break;
    case PCM::MSRAccessDenied:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
        exit(EXIT_FAILURE);
    case PCM::PMUBusy:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
        cerr << "Alternatively you can try running Intel PCM with option -r to reset PMU configuration at your own risk." << endl;
        exit(EXIT_FAILURE);
    default:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (Unknown error)." << endl;
        exit(EXIT_FAILURE);
    }

    cerr << "\nDetected " << m->getCPUBrandString() << " \"Intel(r) microarchitecture codename " << m->getUArchCodename() << "\"" << endl;

	
	ServerUncorePowerState * BeforeState = new ServerUncorePowerState[m->getNumSockets()];
    ServerUncorePowerState * AfterState = new ServerUncorePowerState[m->getNumSockets()];
    uint64 BeforeTime = 0, AfterTime = 0;
	
	
	
    std::vector<CoreCounterState> cstates1, cstates2;
    std::vector<SocketCounterState> sktstate1, sktstate2;
    SystemCounterState sstate1, sstate2;
    const int cpu_model = m->getCPUModel();
    uint64 TimeAfterSleep = 0;
    PCM_UNUSED(TimeAfterSleep);

	delay = PCM_DELAY_DEFAULT;
    m->setBlocked(false);

    int fr = maxf; // for 3 different core frrequency
    //int fr = 0;
    //for(fr = 0 ; fr < maxf ; fr++)
    for( ; fr == maxf ; fr++)
    {
		cout << " Start to run Benchmarks...\n";



		if(fr==0)
		{
			system("cpupower frequency-set -f 2100000");
		}
		else if(fr==1)
		{
			system("cpupower frequency-set -f 1900000");
		}			
		else
		{
			system("cpupower frequency-set -f 1200000");
		}

/*		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: wordcount 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
//Edit by Devang/
         pid= fork();
         if (pid==0)
         {
	 cout<<"This is child process";
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
	  //execl("/bin/su hduser -c  \"ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh\" ", "run.sh",NULL); 

         //Activate this for running on VM/
	 //execl( "/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
	 
	 //Activate this for running on Server Machine/
	 execl("/home/hosein/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh","run.sh", NULL);
	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }
	 
//	 system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh");

	for(count=0; pid_status<0 ; count++) {
	
//---------------------------------------------------------------
//                     Reading states and time befor running
    		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
 		for(uint32 i=0; i<m->getNumSockets(); ++i)
	        BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
        		MySystem(sysCmd, sysArgv);
		}

		//-----Delay-----/
		MySleepMs(DELAY);
		//-----Delay-----/

	
		pid_result=waitpid(pid,&pid_status,WNOHANG);
		std::cout<<"This loop is running with current count: "<<count<<"\n";
		std::cout<<"Status of Child: "<< pid_status;
		std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running	
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
		//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//					Writing information to report
		myfile << "wordcount," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";

	}
	std::cout<<"Application_count --> Hadoop:Wordcount --> "<<count<<"\n";
	pid_status = -100; //To re-initialize value for the next application/

//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");


//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	

		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: wordcount 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/wordcount/spark/run.sh");
	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
	 cout<<"This is child process";

         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/spark/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/spark/run.sh", NULL);

         //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/spark/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/micro/wordcount/spark/run.sh","run.sh", NULL);

	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//-----------------------------------------------------------------
//                     Reading states and time befor running
    		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
 		for(uint32 i=0; i<m->getNumSockets(); ++i)
        		BeforeState[i] = m->getServerUncorePowerState(i); 

	 	if (sysCmd != NULL)
		{
        		MySystem(sysCmd, sysArgv);
		}	
		//-----Delay-----//
		MySleepMs(DELAY);
		//-----Delay-----//

		pid_result=waitpid(pid,&pid_status,WNOHANG);
		std::cout<<"This loop is running with current count: "<<count<<"\n";
		std::cout<<"Status of Child: "<< pid_status;
		std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time befor running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "wordcount," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";

	}
	std::cout<<"Application_count --> Spark:Wordcount --> "<<count<<"\n";
	pid_status = -100; //To re-initialize value for the next application/

//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/
		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: sort 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/sort/hadoop/run.sh");
	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
	 cout<<"This is child process";

	 //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/sort/hadoop/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/sort/hadoop/run.sh", NULL);  

	 //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/sort/hadoop/run.sh", NULL);
	 
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/micro/sort/hadoop/run.sh","run.sh", NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//----------------------------------------------------------------
//                     Reading states and time befor running
    		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
		BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}
		
		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

		pid_result=waitpid(pid,&pid_status,WNOHANG);
		std::cout<<"This loop is running with current count: "<<count<<"\n";
		std::cout<<"Status of Child: "<< pid_status;
		std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "sort," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";

	}
	std::cout<<"Application_count --> Hadoop:Sort --> "<<count<<"\n";
	pid_status = -100; //To re-initialize value for the next application/
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	


		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: sort 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/sort/spark/run.sh");

         pid= fork();
         if (pid==0)
         {
	 cout<<"This is child process";

	 //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/sort/spark/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/sort/spark/run.sh", NULL); 
	 
	 //Activate this for running on VM/
         //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/sort/spark/run.sh", NULL);
	 
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/micro/sort/spark/run.sh","run.sh", NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {
//---------------------------------------------------------------	
//                     Reading states and time befor running
		m->getAllCounterStates(sstate1, sktstate1, cstates1);
		BeforeTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
       		BeforeState[i] = m->getServerUncorePowerState(i); 

	 	if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

		pid_result=waitpid(pid,&pid_status,WNOHANG);
		std::cout<<"This loop is running with current count: "<<count<<"\n";
		std::cout<<"Status of Child: "<< pid_status;
		std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "sort," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Spark:Sort --> "<<count<<"\n";
	pid_status=-100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
/*
		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: terasort 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/terasort/hadoop/run.sh");

	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";
	  //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/terasort/hadoop/run.sh", NULL);
	 //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/terasort/hadoop/run.sh", NULL);

	 //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/terasort/hadoop/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/micro/terasort/hadoop/run.sh","run.sh", NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {
	
//---------------------------------------------------------------
//                     Reading states and time befor running
		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
	 	for(uint32 i=0; i<m->getNumSockets(); ++i)
	        	BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

        	pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
        	std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
            	AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "terasort," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";


	}
	std::cout<<"Application_count --> Hadoop:Terasort --> "<<count<<"\n";
	pid_status=-100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	

		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: terasort 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/terasort/spark/run.sh");

	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";

         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/terasort/spark/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/terasort/spark/run.sh", NULL); 
	
	 //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/terasort/spark/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/micro/terasort/spark/run.sh","run.sh", NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------	
//                     Reading states and time befor running
		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
	 	for(uint32 i=0; i<m->getNumSockets(); ++i)
	        BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

       		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

	 	pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
	        std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "terasort," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Spark:Terasort --> "<<count<<"\n";
	pid_status = -100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/
/*
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: sleep 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
//                     Reading states and time befor running
    m->getAllCounterStates(sstate1, sktstate1, cstates1);
    BeforeTime = m->getTickCount();
 for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

	 if (sysCmd != NULL)
		 {
        MySystem(sysCmd, sysArgv);
		}	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
 system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/sleep/hadoop/run.sh");
//---------------------------------------------------------------
//						Reading states and time after running

	m->getAllCounterStates(sstate2, sktstate2, cstates2);
	AfterTime = m->getTickCount();
	for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
myfile << "sleep," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.6";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
myfile << "," << numcore << "," << insize;			
myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

  mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
	myfile <<","<<mbw<< "\n";
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/

/*

		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: sleep 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
//                     Reading states and time befor running
    m->getAllCounterStates(sstate1, sktstate1, cstates1);
    BeforeTime = m->getTickCount();
 for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

	 if (sysCmd != NULL)
		 {
        MySystem(sysCmd, sysArgv);
		}	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
 system("/home/hoseinmmm/project/HiBench-master/bin/workloads/micro/sleep/spark/run.sh");
//---------------------------------------------------------------
//						Reading states and time after running

	m->getAllCounterStates(sstate2, sktstate2, cstates2);
	AfterTime = m->getTickCount();
	for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
myfile << "sleep," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.6";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
myfile << "," << numcore << "," << insize;			
myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

  mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
	myfile <<","<<mbw<< "\n";
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
----older comment ended here/
*/
/*		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: nweight 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
//                     Reading states and time befor running
    m->getAllCounterStates(sstate1, sktstate1, cstates1);
    BeforeTime = m->getTickCount();
 for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

	 if (sysCmd != NULL)
		 {
        MySystem(sysCmd, sysArgv);
		}	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
 system("/home/hoseinmmm/project/HiBench-master/bin/workloads/graph/nweight/spark/run.sh");
//---------------------------------------------------------------
//						Reading states and time after running

	m->getAllCounterStates(sstate2, sktstate2, cstates2);
	AfterTime = m->getTickCount();
	for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
myfile << "nweight," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.6";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
myfile << "," << numcore << "," << insize;			
myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

  mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
	myfile <<","<<mbw<< "\n";
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/
/*
	
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: bayes 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");

//Edit by Devang/
         //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";
         //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
          //execl("/bin/su hduser -c  \"ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh\" ", "run.sh",NULL); 

         //Activate this for running on VM/
         //execl( "/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/ml/bayes/hadoop/run.sh", NULL);

         //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/ml/bayes/hadoop/run.sh","run.sh", NULL);
         
	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }
//               std::cout<<"Pid of the the script invoked on other system = "<<pid<<"\n";

// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/ml/bayes/hadoop/run.sh");

	for(count=0; pid_status<0 ; count++) {
//---------------------------------------------------------------
//                     Reading states and time befor running
                m->getAllCounterStates(sstate1, sktstate1, cstates1);
                BeforeTime = m->getTickCount();
                for(uint32 i=0; i<m->getNumSockets(); ++i)
                BeforeState[i] = m->getServerUncorePowerState(i);

                if (sysCmd != NULL)
                {
                        MySystem(sysCmd, sysArgv);
                }

                //-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/


                pid_result=waitpid(pid,&pid_status,WNOHANG);
                std::cout<<"This loop is running with current count: "<<count<<"\n";
                std::cout<<"Status of Child: "<< pid_status;
                std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
            		AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "bayes," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";

	}

	std::cout<<"Application_count --> Hadoop:Bayes --> "<<count<<"\n";
        pid_status = -100; //To re-initialize value for the next application/

//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/
/*
		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: bayes 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
//                     Reading states and time befor running
    m->getAllCounterStates(sstate1, sktstate1, cstates1);
    BeforeTime = m->getTickCount();
 for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

	 if (sysCmd != NULL)
		 {
        MySystem(sysCmd, sysArgv);
		}	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
 system("/home/hoseinmmm/project/HiBench-master/bin/workloads/ml/bayes/spark/run.sh");
//---------------------------------------------------------------
//						Reading states and time after running

	m->getAllCounterStates(sstate2, sktstate2, cstates2);
	AfterTime = m->getTickCount();
	for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
myfile << "bayes," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.6";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
myfile << "," << numcore << "," << insize;			
myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

  mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
	myfile <<","<<mbw<< "\n";
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
/-----older comment 2
*/

/*		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: kmeans 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/ml/kmeans/hadoop/run.sh");

	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";

         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/ml/kmeans/hadoop/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/ml/kmeans/hadoop/run.sh", NULL);         

	 //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/ml/kmeans/hadoop/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/ml/kmeans/hadoop/run.sh","run.sh", NULL);

	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------	
//                     Reading states and time befor running
		m->getAllCounterStates(sstate1, sktstate1, cstates1);
		BeforeTime = m->getTickCount();
	 	for(uint32 i=0; i<m->getNumSockets(); ++i)
        		BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

       		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

	 	pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
	        std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "kmeans," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Hadoop:Kmeans --> "<<count<<"\n";
	pid_status= -100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	

		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: kmeans 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/ml/kmeans/spark/run.sh");
	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";

	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/ml/kmeans/spark/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/ml/kmeans/spark/run.sh", NULL);

	 //Activate this for running on VM//
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/ml/kmeans/spark/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/ml/kmeans/spark/run.sh","run.sh",NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------	
//                     Reading states and time befor running
		m->getAllCounterStates(sstate1, sktstate1, cstates1);
	    	BeforeTime = m->getTickCount();
	 	for(uint32 i=0; i<m->getNumSockets(); ++i)
	        BeforeState[i] = m->getServerUncorePowerState(i); 

	 	if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

   		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

	 	pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
        	std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "kmeans," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	
	std::cout<<"Application_count --> Spark:Kmeans --> "<<count<<"\n";
	pid_status=-100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/

/*	
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: pagerank 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/websearch/pagerank/hadoop/run.sh");

	 //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";

	 //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/hadoop/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/hadoop/run.sh", NULL);

	 //Activate this for running on VM/
         //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/hadoop/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/websearch/pagerank/hadoop/run.sh","run.sh", NULL);

         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------
//                     Reading states and time befor running
	    	m->getAllCounterStates(sstate1, sktstate1, cstates1);
    		BeforeTime = m->getTickCount();
 		for(uint32 i=0; i<m->getNumSockets(); ++i)
		BeforeState[i] = m->getServerUncorePowerState(i); 

	 	if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}

		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

	        pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
	        std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "pagerank," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Hadoop:Pagerank --> "<<count<<"\n";
	pid_status = -100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	


		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:spark  Application: pagerank 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//---------------------------------------------------------------
	
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");
// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/websearch/pagerank/spark/run.sh");

         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";

         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/spark/run.sh", NULL);
	 //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/spark/run.sh", NULL);

         //Activate this for running on VM/
	 //execl("/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/websearch/pagerank/spark/run.sh", NULL);
         
	 //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/websearch/pagerank/spark/run.sh","run.sh", NULL);

	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

	for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------	
//                     Reading states and time befor running
    		m->getAllCounterStates(sstate1, sktstate1, cstates1);
		BeforeTime = m->getTickCount();
	 	for(uint32 i=0; i<m->getNumSockets(); ++i)
		        BeforeState[i] = m->getServerUncorePowerState(i); 

		if (sysCmd != NULL)
		{
	        MySystem(sysCmd, sysArgv);
		}
       
		//-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/

	 	pid_result=waitpid(pid,&pid_status,WNOHANG);
	        std::cout<<"This loop is running with current count: "<<count<<"\n";
        	std::cout<<"Status of Child: "<< pid_status;
	        std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running
		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "pagerank," << "spark," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Spark:Pagerank --> "<<count<<"\n";
	pid_status = -100;
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
*/

/*
		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: aggregation 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");

	 pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";
         //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
          //execl("/bin/su hduser -c  \"ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh\" ", "run.sh",NULL); 

         //Activate this for running on VM/
         //execl( "/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/sql/aggregation/hadoop/run.sh", NULL);

         //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/sql/aggregation/hadoop/run.sh","run.sh", NULL);
         
	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }


// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/sql/aggregation/hadoop/run.sh");

	 for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------
//                     Reading states and time befor running
                m->getAllCounterStates(sstate1, sktstate1, cstates1);
                BeforeTime = m->getTickCount();
                for(uint32 i=0; i<m->getNumSockets(); ++i)
                BeforeState[i] = m->getServerUncorePowerState(i);

                if (sysCmd != NULL)
                {
                        MySystem(sysCmd, sysArgv);
                }

                //-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/


                pid_result=waitpid(pid,&pid_status,WNOHANG);
                std::cout<<"This loop is running with current count: "<<count<<"\n";
                std::cout<<"Status of Child: "<< pid_status;
                std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running

		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
        	    AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "aggregation," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}

	std::cout<<"Application_count --> Hadoop:Aggregation --> "<<count<<"\n";
        pid_status = -100; //To re-initialize value for the next application//
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	


		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: join 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");

//Edit by Devang/
         //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";
         //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
          //execl("/bin/su hduser -c  \"ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh\" ", "run.sh",NULL); 

         //Activate this for running on VM/
         //execl( "/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/sql/join/hadoop/run.sh", NULL);

         //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/sql/join/hadoop/run.sh","run.sh", NULL);
         
	 _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/sql/join/hadoop/run.sh");
//---------------------------------------------------------------
//						Reading states and time after running
	
	 for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------
//                     Reading states and time befor running
                m->getAllCounterStates(sstate1, sktstate1, cstates1);
                BeforeTime = m->getTickCount();
                for(uint32 i=0; i<m->getNumSockets(); ++i)
                BeforeState[i] = m->getServerUncorePowerState(i);

                if (sysCmd != NULL)
                {
                        MySystem(sysCmd, sysArgv);
                }

                //-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/


                pid_result=waitpid(pid,&pid_status,WNOHANG);
                std::cout<<"This loop is running with current count: "<<count<<"\n";
                std::cout<<"Status of Child: "<< pid_status;
                std::cout<<"Return value of WAITPID:"<<pid_result;		

		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "join," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Hadoop:Join --> "<<count<<"\n";
        pid_status = -100; //To re-initialize value for the next application/
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	

		
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//					Framework:Hadoop  Application: scan 
//-----------------------------------------------------------------
//-----------------------------------------------------------------	
//						Open report
		myfile.open("hibench.csv",fstream::app);
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//----------------------------------------------------------------
//						Call the program
//system("/usr/local/hadoop/bin/hadoop  jar /usr/local/hadoop/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.7.0.jar wordcount randtext28g hwcout");

//Edit by Devang/
         //pid_t pid,pid_result;
         pid= fork();
         if (pid==0)
         {
         cout<<"This is child process";
         //execl("/bin/su", "hoseinmmm", "-c", "ssh dev@192.168.56.102 -i /home/hoseinmmm/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
         //execl("/usr/bin/ssh", "ssh","dev@192.168.56.102","/home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh", NULL);
          //execl("/bin/su hduser -c  \"ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/micro/wordcount/hadoop/run.sh\" ", "run.sh",NULL); 

         //Activate this for running on VM/
         //execl( "/bin/su", "hduser", "-c", "ssh hduser@192.168.56.101 -i /home/hduser/.ssh/id_rsa /home/dev/project/HiBench-master/bin/workloads/sql/scan/hadoop/run.sh", NULL);

         //Activate this for running on Server Machine/
         execl("/home/hosein/project/HiBench-master/bin/workloads/sql/scan/hadoop/run.sh","run.sh", NULL);
         _exit(1);
         }
         else if (pid > 0)
         {
         //int status=300;
         //std::cout<<"Status_before: "<<status<<"\n";
         //waitpid(pid,&status,0);
         //std::cout<<"Status_after: "<<status<<"\n";
         //std::cout<<"Process: "<<getpid()<<" with child "<<pid<<"\n";
         }

// system("/home/hoseinmmm/project/HiBench-master/bin/workloads/sql/scan/hadoop/run.sh");

        for(count=0; pid_status<0 ; count++) {

//---------------------------------------------------------------
//                     Reading states and time befor running
                m->getAllCounterStates(sstate1, sktstate1, cstates1);
                BeforeTime = m->getTickCount();
                for(uint32 i=0; i<m->getNumSockets(); ++i)
                BeforeState[i] = m->getServerUncorePowerState(i);

                if (sysCmd != NULL)
                {
                        MySystem(sysCmd, sysArgv);
                }

                //-----Delay-----/
                MySleepMs(DELAY);
                //-----Delay-----/


                pid_result=waitpid(pid,&pid_status,WNOHANG);
                std::cout<<"This loop is running with current count: "<<count<<"\n";
                std::cout<<"Status of Child: "<< pid_status;
                std::cout<<"Return value of WAITPID:"<<pid_result;

//---------------------------------------------------------------
//                     Reading states and time after running

		m->getAllCounterStates(sstate2, sktstate2, cstates2);
		AfterTime = m->getTickCount();
		for(uint32 i=0; i<m->getNumSockets(); ++i)
	            AfterState[i] = m->getServerUncorePowerState(i);
//---------------------------------------------------------------
//						Remove output file	
//system("/usr/local/hadoop/bin/hdfs dfs -rmr -skipTrash  hwcout");
//---------------------------------------------------------------
//						Writing information to report
		myfile << "scan," << "hadoop," << memcap<<","<<memfrq<<","<<memch;
		if(fr==0){
			myfile <<",2.1";}
		else if(fr==1){
			myfile <<",1.9";}			
		else{
			myfile <<",1.2";}
		myfile << "," << numcore << "," << insize;			
		myfile << ","<<(double(AfterTime-BeforeTime)/1000)<<","<<getCoreIPC(sstate1, sstate2)<<","<<getL3CacheHitRatio(sstate1, sstate2)<<","<<getL2CacheHitRatio(sstate1, sstate2);
		myfile << ","<<(getCoreCStateResidency(0, sstate1, sstate2)*100.)<<","<<getConsumedJoules(sktstate1[0], sktstate2[0])<<","<<getDRAMConsumedJoules(sktstate1[0], sktstate2[0]);
		myfile << ","<< (((getConsumedJoules(sktstate1[0], sktstate2[0]))+(getDRAMConsumedJoules(sktstate1[0], sktstate2[0])))*(double(AfterTime-BeforeTime)/(1000*1000)));
		myfile << ","<<((getConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
		myfile << ","<<((getDRAMConsumedJoules(sktstate1[0], sktstate2[0]))/(double(AfterTime-BeforeTime)/1000));
//                                   pcm-memory to calculate BW

		mbw = calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
		myfile <<","<<mbw<< "\n";
	}
	std::cout<<"Application_count --> Hadoop:Scan --> "<<count<<"\n";
        pid_status = -100; //To re-initialize value for the next application/
//---------------------------------------------------------------
//						Closing report

	myfile.close();
//						Clear DRAM
		system("sync && echo 3 | sudo tee /proc/sys/vm/drop_caches");
//-----------------------------------------------------------------
///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------	
//------older comment 3
*/
if (true)
    {
       // cerr << "\n Resetting PMU configuration" << endl;
      //  m->resetPMU();
    }

        if (m->isBlocked()) 
		{
            // in case PCM was blocked after spawning child application: break monitoring loop here
          //  break;
        }

    }
	


    exit(EXIT_SUCCESS);
}

