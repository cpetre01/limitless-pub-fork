/**
* DaemonMonitor		Provides information about CPU, IO, Networks and GPUs
+ stats in a node.
* @version              v1.1
*/

#include <cstdlib>
#include <optional>
#include <string>
#include <csignal>
#include <iostream>
#include <vector>
#include <chrono>
#include <sys/time.h>
#include <filesystem>
#include "system_features.hpp"
#include "daemon_monitor.hpp"
#include "net_info.hpp"
#include "cpu_info.hpp"
#include "memory_info.hpp"
#include "Packed_sample.hpp"
#include "temp_info.hpp"

#if ENABLE_POWERCOLLECTOR
    #include "power_cpu_info.hpp"
#endif

#if ENABLE_REDIS
    #include "redisdb.hpp"
#endif

#if ENABLE_IBA
    #include "cliente_monitor_iba.hpp"
#else
    #include "cliente_monitor.hpp"
#endif

#if ENABLE_GPU
    #include "gpu_info.hpp"
#endif

//Influx link
#include "influxdb.hpp"


using namespace std;

/**************************************************************************************************************/

Hw_conf hw_features;
int mode = -1;
Packed_sample *ps;
Packed_sample _last_ps;
int server_socket;
int threshold = 0;
int opt, port = 0;
string server, server2, server3, es_addr;

extern int influxport;
extern string token;
extern string org;
extern string bucket;

bool verbose = false;

int error = 0;
int heartbit = 0;
int tmrd = 0;
int top_relation = 0, top_counter = 0;
int net_reducer = 0;
int only_hotspots = 0;
int th_cpu = 0, th_mem = 0, th_io = 0, th_net = 0, th_ener = 0;
std::vector<int> thresholds(5);
std::chrono::milliseconds tmilisleep;

int n_devices_io = 0, n_cpu = 0,  n_cores = 0, n_interfaces = 0, n_samples = 0, n_core_temps = 0;
std::vector<int> comparePositions;
unsigned int tinterval = 1;
int lastcpu=0;
int lastmem=0;
int currcpu=0;
int currmem=0;


/* Alarms*/
int cpu_lo = 10;
int cpu_hi = 90;
int mem_lo = 15;
int mem_hi = 80;
int com_hi = 75;


#if ENABLE_IBA
int ccti_increase = -1;
#endif

/*
****************************************************************************************************************
* Computes the size of each package from the number of devices, interfaces, and samples that are sent in each packet.
****************************************************************************************************************
*/

void signalHandler(const int _signal){
	cout << endl;
	cout << "Signal received: " << _signal << endl;
	delete ps;
	exit(_signal);
}

void sigAlarmHandler(int _signal){
 	signal(SIGALRM,sigAlarmHandler);
}

//Setup signal handler for termination 
void setup_signal_term(){
	signal(SIGINT, signalHandler);
        signal(SIGALRM,sigAlarmHandler);

}

/**
 * If sample values are in a determined range, it won't be sended (to reduce net usage)
 * @param ps
 * @return 0 if sample must be sended, 1 if not.
 */
int checkRangePS(const Packed_sample* ps){
    int cont = 0;
    if (comparePositions.empty()){
        int pos = 12;
        comparePositions.push_back(pos);
        pos++;
        comparePositions.push_back(pos);
        pos++; // points to energy
        for (int a = 0; hw_features.n_devices_io > a; a++){
            pos+=2;
            comparePositions.push_back(pos);
        }
        for (int a = 0; hw_features.n_interfaces > a; a++){
            pos+=2;
            comparePositions.push_back(pos);
        }
        pos += 2*hw_features.n_core_temps;
        pos++;
        comparePositions.push_back(pos);
        pos++;
        comparePositions.push_back(pos);
    }

    for (auto i : comparePositions){//i; i < ps->packed_ptr; i++){
        if ((_last_ps.packed_buffer[i] + threshold) < ps->packed_buffer[i] || (_last_ps.packed_buffer[i] - threshold) >
            ps->packed_buffer[i])
            cont++;
    }

    if (cont > 0) {
        cont = 1;
        memcpy(&_last_ps.packed_buffer, ps->packed_buffer, sizeof(ps->packed_buffer));
    }
    return cont;
}

/**
 * If certain metrics exceed certain thresholds, an alarm is triggered. 
 * @param ps
 */
void checkAlarm(const Packed_sample* ps){

    int a1=0, a2=0, a3=0, a4=0, a5=0;

    if ((unsigned int)ps->packed_buffer[10] > mem_hi){ a1 = 1; }
    if ((unsigned int)ps->packed_buffer[10] < mem_lo){ a2 = 1; }
    if ((unsigned int)ps->packed_buffer[11] > cpu_hi){ a3 = 1; }
    if ((unsigned int)ps->packed_buffer[11] < cpu_lo){ a4 = 1; }

    int pos = 11;// cpu
    pos += hw_features.n_cpu;
    for (int a = 0; hw_features.n_devices_io > a; a++){ pos+=2; }
    for (int a = 0; hw_features.n_interfaces > a; a++){
        pos+=2;
	      if((unsigned int)ps->packed_buffer[pos] > com_hi){ a5 = 1; }
    }

    /*char buf[64];
    snprintf(buf, 64,"%i:%i:%i:%i:%i",a1, a2, a3, a4, a5); //app deleted because it is not trivial to get it
    reply = (redisReply*)redisCommand(c, "SET %s %s", hw_features.hostname, buf);
    freeReplyObject(reply);*/
}

/**
* Set values from file
*/
bool setParams(const string& input_file){
    std::ifstream initfile(input_file);
    bool error = false;
    std::string line;
    int param_n = 0;

    while (std::getline(initfile, line)) {
        if (!line.empty() > 0 && line.at(0) != '#') {
            switch (param_n) {
                case 0: // interval time
                    tinterval = strtol(line.c_str(), nullptr, 10);
                    tmilisleep = std::chrono::milliseconds(tinterval);
                    if (verbose) cout << "Interval time: " << tinterval << endl;
                    param_n++;
                    break;
                case 1: //port
                    port = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Port: " << port << endl;
                    param_n++;
                    break;
                case 2: //num samples
                    n_samples = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Num samples: " << n_samples << endl;
                    param_n++;
                    break;
                case 3: // server ip
                    server = line;
                    if (verbose) cout << "Server ip: " << server << endl;
                    param_n++;
                    break;
                case 4: // ES ip
                    es_addr = line;
                    if (verbose) cout << "Database IP: " << es_addr << endl;
                    param_n++;
                    break;
                case 5: // bitmap mode (0 off, 1 on)
                    hw_features.modo_bitmap = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Bitmap: " << hw_features.modo_bitmap << endl;
                    param_n++;
                    break;
                case 6: // threshold filter (0 no, 1 yes)
                    net_reducer = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold filter: " << net_reducer << endl;
                    param_n++;
                    break;
                case 7: //threshold filter value
                    threshold = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold filter value: " << threshold << endl;
                    param_n++;
                    break;
                case 8: // top interval
                    top_relation = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Interval TOP: " << top_relation << endl;
                    param_n++;
                    break;
                case 9: // TMR (0 simple, 1 triple)
                    tmrd = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "TMR mode: " << tmrd << endl;
                    param_n++;
                    break;
                case 10: // backup IP 1
                    server2 = line;
                    if (server2 == "-1") server2.clear();
                    if (verbose) cout << "Server backup 1: " << (server2.empty() ? "NULL" : server2) << endl;
                    param_n++;
                    break;
                case 11: // backup IP 2
                    server3 = line;  // Se usa std::string
                    if (server3 == "-1") server3.clear();
                    if (verbose) cout << "Server backup 2: " << (server3.empty() ? "NULL" : server3) << endl;
                    param_n++;
                    break;
                case 12:
                    only_hotspots = strtol(line.c_str(), nullptr, 10);
                    if (only_hotspots != 0)
                        if (verbose) cout << "Notify only hotspots.\n";
                    else
                        if (verbose) cout << "Notify all.\n";
                    param_n++;
                    break;
                case 13:
                    th_mem = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold MEM: " << th_mem << endl;
                    param_n++;
                    break;
                case 14:
                    th_cpu = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold CPU: " << th_cpu << endl;
                    param_n++;
                    break;
                case 15:
                    th_io = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold IO: " << th_io  << endl;
                    param_n++;
                    break;
                case 16:
                    th_net = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold NET: " << th_net << endl;
                    param_n++;
                    break;
                case 17:
                    th_ener = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Threshold Energy: " << th_ener << endl;
                    param_n++;
                    break;
#if ENABLE_IBA
                case 18:
                    ccti_increase = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "CCTI_increase: " << ccti_increase << "\n";
                    param_n++;
                    break;
#endif
#if ENABLE_INFLUX
#if ENABLE_IBA
                case 19:
#else
                case 18:
#endif
                    influxport = strtol(line.c_str(), nullptr, 10);
                    if (verbose) cout << "Influx Port: " << influxport << endl;
                    param_n++;
                    break;
#if ENABLE_IBA
                case 20:
#else
                case 19:
#endif
                    token = line;
                    if (verbose) cout << "Influx Token: " << token << endl;
                    param_n++;
                    break;
#if ENABLE_IBA
                case 21:
#else
                case 20:
#endif
                    org = line;
                    if (verbose) cout << "Influx Org: " << org << endl;
                    param_n++;
                    break;
#if ENABLE_IBA
                case 22:
#else
                case 21:
#endif
                    bucket = line;
                    if (verbose) cout << "Influx Bucket: " << bucket << endl;
                    param_n++;
                    break;
#if ENABLE_IBA
                case 23:
#else
                case 22:
#endif
                    if (verbose) cout << "More lines than expected in init.dae file.\n";
                    break;
#else // !ENABLE_INFLUX
#if ENABLE_IBA
                case 19:
                    if (verbose) cout << "More lines than expected in init.dae file.\n";
                    break;
#else
                case 18:
                    if (verbose) cout << "More lines than expected in init.dae file.\n";
                    break;
#endif
#endif // ENABLE_INFLUX
                default:
                    error = true;
                    break;
            }
        }
    }
    return error;
}

/**
 * Update internal params from a file edited by user
 */
void updateConfParams() {
    std::cout << "\n********** Updating parameters *************" << std::endl;

    if (const bool error = setParams(CONF_FILE); !error) {
        int err = 0;
        //update thresholds
        thresholds[0] = th_mem;
        thresholds[1] = th_cpu;
        thresholds[2] = th_ener;
        thresholds[3] = th_io;
        thresholds[4] = th_net;

        err = init_socket(server, port);
        if (err == -1) {
            std::cerr << "Socket could not be initialized" << endl;
        }
        if (!server2.empty()) {
            err = init_socket_backup1(server2, port);
            if (err == -1) {
                std::cerr << "Backup 1 Socket could not be initialized" << endl;
            }
        }
        if (!server3.empty()) {
            err = init_socket_backup2(server3, port);
            if (err == -1) {
                std::cerr << "Backup 2 Socket could not be initialized" << endl;
            }
        }
    }
}

/**
 * Socket initialization and TMR configuration.
 * @return Error if a connection fails
 */
int initalizeSocketsTMR() {
    //INIT Communications: Check that everything is correct
    if (server.empty()) {
        std::cerr << "ERROR: NO address for master was given." << std::endl;
        return -1;
    }
    if ((port < 1024) || (port > 65535)) {
        std::cerr << "Error: Port must be in the range 1024 <= port <= 65535" << std::endl;
        return -1;
    }

    // Initialise sockets (TMR if enabled)
    error = init_socket(server, port);
    if (error == -1) {
        std::cerr << "Socket could not be initialized" << endl;
        return error;
    }
    if (!server2.empty()) {
        error = init_socket_backup1(server2, port);
        if (error == -1) {
            std::cerr << "Backup 1 Socket could not be initialized" << endl;
            return error;
        }
    }
    if (!server3.empty()) {
        error = init_socket_backup2(server3, port);
        if (error == -1) {
            std::cerr << "Backup 2 Socket could not be initialized" << endl;
            return error;
        }
    }
    return error;
}

/**
 * Checks if the user has specified a file to upload the configuration online
 * @param last_conf_update last modification time
 * @param init first iteration
 */
void checkConfigurationUpdate(std::filesystem::file_time_type last_conf_update,
                              bool init) {
    if(std::filesystem::exists(CONF_FILE)){
        if(init){
            init = false;
            auto ftime = std::filesystem::last_write_time(CONF_FILE);
            last_conf_update = ftime;
            updateConfParams();
        }
        else{
            auto ftime = std::filesystem::last_write_time(CONF_FILE);
            if(ftime > last_conf_update){
                last_conf_update = ftime;
                updateConfParams();
            }
        }
    }
}

std::optional<std::string> get_env_var(const char* name) {
    const char* val = std::getenv(name);
    if (val == nullptr) {
        return std::nullopt;
    }
    return std::string(val);
}

int main(int argc, char *argv[]) {

    using namespace std::chrono;
    using clk = high_resolution_clock;
    int i = -1;
    clk::time_point t1, t2;
    clk::duration difft;

    std::string job_name = "Default";

    stringstream sargv;
    string sarg;

    string init_file_path = INIT_FILE;
    auto verbose_env = get_env_var("VERBOSE");

    if (verbose_env == "1" || verbose_env == "true") {
        verbose = true;
    }
    
    if (argc == 2) {
        if (argv[1] == "-h" || argv[1] == "--help") {
            cerr << "Usage: " << argv[0] << " [path_to_init_file]" << endl;
            exit(0);
        }
        init_file_path = argv[1];
    } else if (argc > 2) {
        cerr << "Usage: " << argv[0] << " [path_to_init_file]" << endl;
        exit(1);
    }

    // SET PARAMS FROM INPUT FILE
    setParams(init_file_path);

    tmilisleep = std::chrono::milliseconds(tinterval);
    thresholds[0] = th_mem;
    thresholds[1] = th_cpu;
    thresholds[2] = th_ener;
    thresholds[3] = th_io;
    thresholds[4] = th_net;


    // **************  REDIS INITIALIZATION *********
#if ENABLE_REDIS
    redisContext* rcontext = InitializeRedisConnection(es_addr);
    if(rcontext == nullptr) cerr << "Redis context error" << endl;
#endif
    //************************************************

    //Setup for signal termination
    setup_signal_term();

    // Read number of processor
    error = read_n_processors_pi(hw_features.cpus, hw_features.n_cpu, hw_features.n_cores);

    if (error != EOK) {
        cerr << "Error reading processors: " << error << endl;
        return -1;
    }

    // Get Ip Address
    get_addr(hw_features.ip_addr_s, hw_features.hostname);
    if (verbose) cout << "hostname es: " << hw_features.hostname << endl;

    // Get memory total
    get_mem_total(hw_features.mem_total);

    header_append("IP_Addr Mem(GB) MemUsage(%) NCPU NCores CPUBusy(%) ");

#if ENABLE_PI
    header_append("Temp{ºC %related} Pow(W) ");
#endif
#if ENABLE_AMD
    header_append("Temp{cpu1ºC cpu2ºC} ");
#endif

#if ENABLE_POWERCOLLECTOR 
    // Get power path
    error = get_power_path(hw_features.pwcpu_features, hw_features.path_dir, hw_features.n_cpu);
#endif

#if ENABLE_IOCOLLECTOR
    // Read number of devices
    error = read_n_devices(hw_features.io_dev, hw_features.n_devices_io);
    if (error != EOK) {
        cerr << "Error reading IO devices: " << error << endl;
        //exit(0);
    }
    header_append(" ");
#endif

#if ENABLE_NETWORKCOLLECTOR
    error = read_n_net_interface(hw_features.net_interfaces, hw_features.n_interfaces);
    if (error != EOK) {
        cerr << "Error reading network interfaces: " << error << endl;
        //exit(0);
    }
    else
        log_concat_interfaces(hw_features);
#endif

#if ENABLE_GPU
    error = read_n_gpu(hw_features.gpus, hw_features.n_gpu, hw_features.GPU_DEVICES_COMPATIBLE, &hw_features.cuLib,
                       &hw_features.nvmlLib);
    if (verbose) cout << "El numero de gpus es: " << hw_features.n_gpu << endl;
    if (error == EGPU) {
        hw_features.GPU_DEVICES_COMPATIBLE = CUDA_NO_COMPATIBLE;
    }
    else
        log_concat_gpus(hw_features);
#endif

    ps = new Packed_sample(hw_features, tinterval, n_samples, threshold);

    // Print on the screen with the output format of the data.
    if (verbose) cout << get_header_line() << endl;

    // Initialize sockets and set TMR if data is provided.
    error = initalizeSocketsTMR();

    //SEND CONFIGURATION PACKET to LDS
    send_conf_packet(&hw_features);

    //Time when the conf file was updated
    std::filesystem::file_time_type last_conf_update;
    bool init = true;

    while (true) {
        t1 = clk::now();

        // checks if the user has included a file to update the configuration online (without restart)
        checkConfigurationUpdate(last_conf_update, init);

        // Clear log_line
        log_clear();

        // Concat ip address
        log_append(hw_features.ip_addr_s);

        // *************************  MEMORY USAGE ******************
        error = read_memory_stats();
        if (error != EOK) {
            cerr << "Error: " << error << endl;
            return -1;
        }

        // *************************** CPU USAGE ************************
        //hw_features.cores.clear();
        read_cpu_stats(hw_features.cpus /*, hw_features.cores*/, hw_features.n_cpu, hw_features.n_cores);

#if ENABLE_PI
    //*************************** TEMP REACHED **********************
	get_temperature_pi();
	get_power_pi();    
#endif



#if ENABLE_AMD
    get_temperature_amd();
#endif


#if ENABLE_POWERCOLLECTOR
    // *************************** POWER USAGE **************************
    get_power(hw_features.pwcpu_features, hw_features.path_dir, hw_features.n_cpu);
#endif


#if ENABLE_IOCOLLECTOR
    // ************************** DEVICES USAGE ************************
    read_devices_stats(hw_features.io_dev, tinterval);
#endif

#if ENABLE_NETWORKCOLLECTOR
    // **************************** NET USAGE ******************************
    read_net_stats(hw_features.net_interfaces);
#endif

#if ENABLE_GPU
    // **************************** GPU USAGE ******************************
    if (hw_features.GPU_DEVICES_COMPATIBLE == CUDA_COMPATIBLE) {
        read_gpu_stats(hw_features.gpus, hw_features.cuLib, hw_features.nvmlLib);
    }
#endif

    int xmitdata = 0, xmitwait = 0;
#if ENABLE_IBA
    //***************************** INFINIBAND ********************************
    std::string hex_guid;
    DoIBAstuff(hw_features.hostname,xmitdata, xmitwait, hex_guid);
    //MakeIBADecision(ccti_increase, xmitdata, xmitwait); --> commented on not to interfere in testing phase
#endif
        
#if ENABLE_INFLUX
	SendDataToInflux_v2(hw_features.hostname, xmitdata, xmitwait, es_addr);
#endif

        // ************************ PACKET TRANSFER ***********************
        if (i != -1) {
            //Monitoring packet
            ps->pack_sample_s(get_log_line());

            //To append the jobname from Slurm, uncomment this line
            //ps->pack_sample_prometheus(get_log_line(), job_name);

            //To send generic packets uncomment this line and fill out the string
            //ps->pack_sample_generic("campo14;18446744073709551615;campo2;543;campo3;18446744073709551615;campo4;1111;campo5;123;campo6;543;");
            //ps->packed_ptr++;

            if (verbose) cout << "Sending: " << get_log_line() << endl;
            i++;

#if ENABLE_REDIS
            //Send to redis
            string aux; 
            stringstream ss(get_log_line());
            vector<string> vals, labels;
            while(getline(ss, aux, ' ')){
                vals.push_back(aux);
            }
            ss.clear();
            ss.str(get_header_line());
            while(getline(ss, aux, ' ')){
                if(aux != "")
                    labels.push_back(aux);
            }
            
            if (labels.size() != vals.size()) {
                std::cerr << "Error: there are " << labels.size()
                        << " labels and " << vals.size()
                        << " values\n";
            } else {
                // Old method to build the redis command. Now it is auto-generated.
                /*std::ostringstream redisCommand;
                redisCommand << "HSET monitor:" << hw_features.hostname;

                for (std::size_t i = 0; i < labels.size(); ++i) {
                    redisCommand << ' ' << labels[i]
                                << ' ' << vals[i];
                }

                // Additional values if required
                redisCommand << " xmitdata " << xmitdata
                            << " xmitwait " << xmitwait
                            << " GUID " << hex_guid;

                const std::string strcmd = redisCommand.str();
                if (verbose) std::cout << strcmd << '\n';
                const bool redisresult = SendToRedis_old(rcontext, strcmd);*/

                const bool redisresult = SendToRedis(rcontext, hw_features.hostname, labels, vals);
                if(redisresult != true) std::cerr << "Error sending data to Reids\n ";
            }

            // Old examples
            /*cout << "HSET monitor:" << hw_features.hostname << " cpu " << v[5] << " mem " << v[2] << " eno1Speed " << v[8] << " eno1bandwidth " << v[9] << 
                " eno2Speed " << v[10] << " eno2bandwidth " << v[11] << " ibs3Speed " << v[12] << " ibs3bandwidth " << v[13] << endl;*/
            /*std::string strcmd = "HSET monitor:" + hw_features.hostname + " cpu " + vals[5] + " mem " + vals[2] + " eno1Speed " + vals[8] + 
		    " eno1bandwidth " + vals[9] + " eno2Speed " + vals[10] + " eno2bandwidth " + vals[11] + " ibs3Speed " + vals[12] + " ibs3bandwidth " + 
		    vals[13] + " xmitdata " + std::to_string(xmitdata) + " xmitwait " + std::to_string(xmitwait) + " GUID " + hex_guid;*/
#endif

            //checkAlarm(ps);

            // CURRENTLY WE ARE NOT USING SERVER. JUST REDIS
            if (i == n_samples) {
                i = 0;
                // create packet and send
                if (net_reducer == 1) {
                    if (_last_ps.ip_addr_s.empty() || heartbit == 10) {
                        heartbit = 0;
                        _last_ps.ip_addr_s = ps->ip_addr_s;
                        memcpy(&_last_ps.packed_buffer, ps->packed_buffer, sizeof(ps->packed_buffer));
                        send_monitor_packet(*ps, tmrd);
                    } else if (checkRangePS(ps) != 0)
                        send_monitor_packet(*ps, tmrd);
                    else
                    heartbit++;
                } else {
                    send_monitor_packet(*ps, tmrd);
                    //To send generic packets uncomment this line and fill out the string
                    //send_monitor_generic(*ps, tmrd);
                }
            }
        }

        t2 = clk::now();
        difft = (t2 - t1);

        if (difft < tmilisleep) {
            std::chrono::milliseconds randomize(0);//rand() % 2000);
            clk::duration difference = (tmilisleep - difft - randomize);

            struct timeval tval{};
            tval.tv_sec = std::chrono::duration_cast<std::chrono::microseconds>(difference).count() / 1000000;
            tval.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(difference).count() % 1000000;

            struct itimerval tival{};
            tival.it_value = tval;
            tival.it_interval.tv_sec = 0;
            tival.it_interval.tv_usec = 0;

            setitimer(ITIMER_REAL, &tival, nullptr);
            pause();
        }

        if (i < 0) { i++; }
    }

#if ENABLE_REDIS
    FreeRedis(rcontext);
#endif

    return 0;
}


