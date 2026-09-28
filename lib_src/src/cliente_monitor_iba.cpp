#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include "cliente_monitor_iba.hpp"


/**************** Cellia integration ***************/
#define PORT     5005
#define MAXLINE 1024
#define CELLIA "10.3.2.3" //3.2.3"//"cellia.i3a.info"
int sd_ib=0;
struct sockaddr_in server_addr_ib;
/*************/

extern bool verbose;

using namespace std;

void DoIBAstuff(std::string hostname, int& xmitdata, int& xmitwait, std::string& hex_guid) {
	/*CC modified */
	std::string guid_s;
	std::string guid_ss;
	std::string node_s;
	std::string path_ib;
	IBA_dev prev;
	prev.stats[0].xmitData = -1;
	int reset_counters = 100;
	int iter = 0;
	/**************/

	const char * cmd = "perfquery -r";
	    std::string result = execCommand(cmd);

	    int getguid = 1;
	    unsigned long long guid;
	    if (guid_s.empty()){
		    getguid = 0;
        	//const char* node_type;
        	struct ibv_device **dev_list;
        	int num_devices;
        	dev_list = ibv_get_device_list(&num_devices);
        	if (!dev_list) {
        	    //cerr << "Failed to get IB devices list\n";
        	} else {
                //std::cout << "IB devices obtained" << std::endl;
        	    //printf("    %-16s\t   node GUID \t\t       Tipo de Nodo\n", "device");
        	    //printf("    %-16s\t----------------\t--------------------------\n", "------");
        	    for (int index = 0; index < num_devices; ++index) {
        	        /*printf("    %-16s\t%016llx\t%-16s\n",
        	               ibv_get_device_name(dev_list[i]),
        	               (unsigned long long) ntohll(ibv_get_device_guid(dev_list[i])),
        	               ibv_node_type_str(dev_list[i]->node_type));*/
				    if (verbose) std::cout << "Original GUID obtained: " << ibv_get_device_guid(dev_list[index]) << std::endl;
        	        guid = static_cast<unsigned long long>(ntohll(ibv_get_device_guid(dev_list[index])));
				    std::stringstream ss;
				    ss<< std::hex << guid+1; // (guid+1); // int decimal_value
				    hex_guid = ss.str();
        	        //node_type = ibv_node_type_str(dev_list[i]->node_type);
        	    }
        	    ibv_free_device_list(dev_list);
        	}
	    }

	    //split by \n and get the last value -> after last point
	    std::vector<std::string> strings;
	    istringstream f(result);
	    std::string s;

	    //printf("Device GUID: %s\n", std::to_string(guid).c_str());
	    if(getguid == 0){
	        std::string id_s;
	        std::string id_sn;
	        std::stringstream ss;

	        ss << std::hex << guid;
	        id_s = ss.str();  //representation in Hex
	        id_sn = id_s; // without final \n

	        guid_s = id_s;
	        guid_ss = id_sn;
	    }

        if (node_s.empty()) {
            node_s = hostname;
            path_ib = "/tmp/IB_";
            path_ib += hostname;
        }

        /* NEW METHOD: data collection */
        /*IBA_dev ibad;
        read_iba_stats(ibad);
        cout << "hostname: " << node_s << endl;
        cout << "guid: " << guid_s << endl;
        cout << "xData: " << ibad.stats[0].xmitData << endl;
        cout << "rData: " << ibad.stats[0].rcvData << endl;
        cout << "xPkts: " << ibad.stats[0].xmitPkts << endl;
        cout << "rPkts: " << ibad.stats[0].rcvPkts << endl;
        cout << "xWait: " << ibad.stats[0].xmitWait << endl;*/

        /* OLD METHOD */
        int countstr = 0;

        while (std::getline(f, s, '.')) {
            long number = 0;
            char* endptr = nullptr;
            const char* nptr = s.c_str();
            errno = 0;

            // Convertimos la cadena a un número
            number = strtol(nptr, &endptr, 10);

            // Validamos la conversión
            if (nptr == endptr) {
                // Error: no se encontraron dígitos
            /*} else if (errno == ERANGE && number == LONG_MIN) {
                // Error: underflow
            } else if (errno == ERANGE && number == LONG_MAX) {
                // Error: overflow
            } else if (errno == EINVAL) {
                // Error: base error value
            } else if (errno != 0 && number == 0) {*/
                // Error: unespecified error
            } else if (errno == 0 && nptr && !*endptr) {
                // good conversion without unprocessed chars
                strings.push_back(std::to_string(number));
                countstr++;

                // Asignamos xmitWait y xmitData en base a la posición en la cadena
                if (countstr == 21) {
                    xmitwait = number;
                    //std::cout << "xmitWait = " << number << std::endl;
                }
                if (countstr == 17) {
                    xmitdata = number;
                    //std::cout << "xmitData = " << number << std::endl;
                }
            } else if (errno == 0 && nptr && *endptr != 0) {
                // good conversion
                strings.push_back(std::to_string(number));
                countstr++;

                if (countstr == 21) {
                    xmitwait = number;
                    //std::cout << "xmitWait = " << number << std::endl;
                }
                if (countstr == 17) {
                    xmitdata = number;
                    //std::cout << "xmitData = " << number << std::endl;
                }
            }
        }

        // Build the line
        std::string buffer = node_s;
        for (int k = 0; k < 21; ++k) {
            buffer += ";";
            buffer += strings[k];
        }

        // add guid to the end
        buffer += ";";
        buffer += guid_s;

        // Log in file
        std::ofstream myfile;
        myfile.open(path_ib, std::ofstream::out | std::ofstream::app);
        myfile << xmitdata << ";" << xmitwait << std::endl;
        myfile.close();

        // Send the buffer
        int bsent = sendn_ib(sd_ib, (void*)buffer.c_str(), buffer.length() + 1, &server_addr_ib);
}

void MakeIBADecision(int ccti_increase, int xmitdata, int xmitwait) {
	/* Integration OPENSM */
    bool is_cc_modified = false;
    std::string hex_guid = "";

    /*int dif = 0;
    int dif2 = 0;
    if (prev.stats[0].xmitData != -1){ //first iteration
        dif = ibad.stats[0].xmitData - prev.stats[0].xmitData;
        dif2 = ibad.stats[0].xmitWait - prev.stats[0].xmitWait;
        //cout << "TRACE DIFF " << ibad.stats[0].xmitData << " --- " << prev.stats[0].xmitData << endl;
    }
    dif = (dif < 0) ? 0 : dif;
    dif2 = (dif2 < 0) ? 0 : dif2;*/
    //cout << "DIFF: " << dif << endl;

    if(ccti_increase != -1){
        char * ccti_increase_s = (char*)calloc(3, 1);
        if (!is_cc_modified && xmitwait > 1000000){//dif > 20000){ //ibad.stats[0].xmitWait > 1000000){
            //ibccconfig --Guid CACongestionSetting <guid> 1 0x1 150 <ccti_increase> 1 0
            // guid +1 to HEX
            sprintf(ccti_increase_s, "%i", ccti_increase);
            char * initline = (char*)malloc(100);
            sprintf(initline, "ibccconfig --Guid CACongestionSetting 0x");
            strcat(initline, hex_guid.c_str());//guid_ss);
            strcat(initline,  " 1 0x1 150 " );
            strcat(initline, ccti_increase_s);
            strcat(initline, " 1 0");

            cout << initline << endl;
            std::string result = execCommand(initline);
            free(initline);
            is_cc_modified = true;
        }

        if (is_cc_modified && xmitwait < 100000){ //dif < 2000){ //ibad.stats[0].xmitWait < 100000){
            char * initline = (char*)malloc(100);
            sprintf(initline, "ibccconfig --Guid CACongestionSetting 0x");
            strcat(initline, hex_guid.c_str()); //guid_ss);
            strcat(initline,  " 1 0x1 150 0 1 0 " );
            std::string result = execCommand(initline);
            cout << initline << endl;
            free(initline);
            is_cc_modified = false;
        }
    }
}


/**
* Functions to send the data to the light server
*/
int sendn_ib(int socket_descriptor, void *buf, int n, struct sockaddr_in *out_addr) {
	char *buf_ptr = (char *) buf;
	sendto(socket_descriptor, buf_ptr, n, 0, (struct sockaddr *) out_addr, sizeof(struct sockaddr_in));

	return n; //success
}

void initializeSocket(){
	struct hostent * hp = nullptr;

	if((sd_ib = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		cerr << " Could not connect to master. " << endl;
	}

	hp = gethostbyname (CELLIA);

	bzero((char *)&server_addr_ib, sizeof(server_addr_ib));
	if(hp != nullptr){
		memcpy(&(server_addr_ib.sin_addr), hp->h_addr, sizeof(hp->h_length));
	}else{
		cerr << "Can not determine the address." << endl;
	}

	server_addr_ib.sin_family = AF_INET;
	server_addr_ib.sin_port  = htons(PORT);

	if(connect(sd_ib,reinterpret_cast<struct sockaddr *>(&server_addr_ib), sizeof(server_addr_ib)) == -1){
		cerr << "Error connecting to the server..." << endl;
	}
}

