#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <chrono>
#include "cliente_monitor.hpp"
#include <memory>
#include <string>
#include <array>

//Inlfuxdb link
#include "influxdb.hpp"

#define BUFFER_SIZE 512

using namespace std;
/*Socket  from which the client will send its information*/
int sd=0, sd1=0, sd2=0;
struct sockaddr_in server_addr;
struct sockaddr_in server_bk_1;
struct sockaddr_in server_bk_2;
int fail_counter = 0;

int influxport = 8086;
std::string token;
std::string org;
std::string bucket;

/**
 * Send generic packet trough the socket.
 * @param ps Structure containing the information
 * @param tmr if TMR is enabled
 * @return 0 if no error occurred, -1 otherwise.
 */
int send_monitor_generic(Packed_sample &ps, int tmr){
  int error=0;
  unsigned char * buffer = nullptr;
  buffer = (unsigned char *) calloc(MAX_PACKET_SIZE, sizeof(char));
  if(buffer==nullptr){
    //std::cerr << "Error creating monitor packet." << endl;
    error = -1;
  }else{
	  int bsent = 0;
	  ps.sample_size = ps.packed_ptr;
    /*Creating the packet and sending it*/
    //int packet_size = create_generic_packet(ps, buffer);

    // 3 tries to connect on each server
    for (int i = 0; i < 9; i++) {
      struct sockaddr_in socketsend;
      if (i<3)
        socketsend = server_addr;
      else if (i >= 3 && i<6)
        socketsend = server_bk_1;
      else
        socketsend = server_bk_2;
      //bsent = sendn(sd, buffer, packet_size, &socketsend);
      bsent = sendn(sd, ps.packed_buffer, ps.sample_size, &socketsend);
      if (bsent != ps.sample_size) {
        error = -1;
      } else {
        error = 0;
        if (tmr != 1)
          break;
      }
    }
  }

  free(buffer);
  return error;
}

/**
 * Send monitoring packet trough the socket.
 * @param ps Structure containing the information
 * @param tmr if TMR is enabled
 * @return 0 if no error occurred, -1 otherwise.
 */
int send_monitor_packet(Packed_sample &ps, int tmr){
	// Allocate memory for the packet using std::unique_ptr
	auto buffer = std::make_unique<unsigned char[]>(MAX_PACKET_SIZE);

	if (!buffer) {
		std::cerr << "Error creating monitor packet." << std::endl;
		return -1;
	}

	int error = 0;

	// Calculate sample size and create the packet
	ps.calculate_sample_size();
	int packet_size = create_sample_packet(ps, buffer.get());
	if (packet_size <= 0) {
		std::cerr << "Error creating sample packet." << std::endl;
		return -1;
	}

	// Array of servers for TMR (Triple Modular Redundancy)
	const std::array<std::pair<int, sockaddr_in*>, 3> servers = {{
		{sd, &server_addr},
		{sd, &server_bk_1},
		{sd, &server_bk_2}
	}};

	// Attempt to send the packet up to 3 tries per server (9 total attempts)
	for (int i = 0; i < 9; ++i) {
		const auto& [sock_fd, server] = servers[i / 3]; // Use integer division to determine the server
		int bsent = sendn(sock_fd, buffer.get(), packet_size, server);

		if (bsent != packet_size) {
			std::cerr << "Error sending monitoring packet." << std::endl;
			error = -1;
		} else {
			error = 0;
			if (tmr != 1) {
				break; // Exit early if not using TMR
			}
		}
	}

	return error;
}

/**
	 Send configuration packet trough the socket.
	 @param hw_conf	Structure containing the information about th hardware configuration of the system.
 	 @return 0 if no error occurred, -1 otherwise.
*/
int send_conf_packet(const Hw_conf* hw_conf) {

	// Calculate the total size of the packet
	const size_t packet_size = CONF_PACKET_SIZE + hw_conf->hostname.length() + 1;
	// Allocate memory for the configuration packet using std::unique_ptr
	auto conf_packet = std::make_unique<char[]>(packet_size);

    if (!conf_packet) {
        std::cerr << "Error creating configuration packet.\n" ;
        return -1;
    }

	// Create the configuration packet
	int size = create_conf_packet(*hw_conf, conf_packet.get());
	if (size <= 0) {
		std::cerr << "Error while creating the configuration packet.\n";
		return -1;
	}

	// Send the packet to the main server
	int bsent = sendn(sd, conf_packet.get(), packet_size, &server_addr);
	if (bsent != static_cast<int>(packet_size)) {
		std::cerr << "Error sending configuration packet to the main server." << std::endl;
		return -1;
	}

	// Send the packet to the backup servers (TMR - Triple Modular Redundancy)
	for (const auto& [sd_backup, server_backup] : {std::pair{sd1, &server_bk_1},
																		std::pair{sd2, &server_bk_2}}) {
		bsent = sendn(sd_backup, conf_packet.get(), packet_size, server_backup);
		if (bsent != static_cast<int>(packet_size)) {
			std::cerr << "Error sending configuration packet to a backup server." << std::endl;
			return -1;
		}
	}

    return 0;
}

/**
	Initialize socket for communication.
	@param server String containing the Ip address to teh server towars the client will 
	communicate. 
	@param port String with the number of the port in which the server will be listening.
	@return Erroe in case the socket was not correctly created. 
*/
int init_socket(const std::string& server, int port){
	struct hostent *hp=nullptr;
	int error =0;

	if((sd = socket(AF_INET, SOCK_DGRAM, 0))< 0){
		std::cerr << " Could not connect to master. " << std::endl;
		exit(0);
	}
	hp = gethostbyname (server.c_str());
     
    //setting up sockaddr_in to be able to connect using it
    bzero((char *)&server_addr, sizeof(server_addr));
    if(hp != nullptr){
        memcpy(&(server_addr.sin_addr), hp->h_addr, sizeof(hp->h_length));
    }else{
        std::cerr << "Can not determine the address." << std::endl;
        error = -1;
		return error;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port  = htons(port);

    //connecting
    if(connect(sd,(struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
        std::cerr << "Error connecting to the server...\n";
        error = -1;
    }   
	return error;
}

/**

	Initialize socket for communication when master server fails.
	@param server String containing the Ip address to teh server towars the client will
	communicate.
	@param port String with the number of the port in which the server will be listening.
	@return Erroe in case the socket was not correctly created.
*/
int init_socket_backup1(const std::string& server, int port){
    struct hostent *hp=nullptr;
    int error =0;

    if((sd1 = socket(AF_INET, SOCK_DGRAM, 0))< 0){
        std::cerr << " Could not connect to master. " << std::endl;
        exit(0);
    }
    hp = gethostbyname (server.c_str());

    //setting up sockaddr_in to be able to connect using it
    bzero((char *)&server_bk_1, sizeof(server_bk_1));
    if(hp != nullptr){
        memcpy(&(server_bk_1.sin_addr), hp->h_addr, sizeof(hp->h_length));
    }else{
        std::cerr << "Can not determine the address." << std::endl;
        error = -1;
        return error;
    }
    server_bk_1.sin_family = AF_INET;
    server_bk_1.sin_port  = htons(port);

    //connecting
    if(connect(sd,(struct sockaddr *) &server_bk_1, sizeof(server_bk_1)) == -1){
        std::cerr << "Error connecting to the server...\n";
        error = -1;
    }
    return error;
}

/**
	Initialize socket for communication when master server and backup server fails. .
	@param server String containing the Ip address to teh server towards the client will
	communicate.
	@param port String with the number of the port in which the server will be listening.
	@return error in case the socket was not correctly created.
*/
int init_socket_backup2(const std::string& server, int port){
    struct hostent *hp=nullptr;
    int error =0;

    if((sd2 = socket(AF_INET, SOCK_DGRAM, 0))< 0){
        std::cerr << " Could not connect to master. " << std::endl;
        exit(0);
    }
    hp = gethostbyname (server.c_str());

    //setting up sockaddr_in to be able to connect using it
    bzero((char *)&server_bk_2, sizeof(server_bk_2));
    if(hp != nullptr){
        memcpy(&(server_bk_2.sin_addr), hp->h_addr, sizeof(hp->h_length));
    }else{
        std::cerr << "Can not determine the address." << std::endl;
        error = -1;
        return error;
    }
    server_bk_2.sin_family = AF_INET;
    server_bk_2.sin_port  = htons(port);

    //connecting
    if(connect(sd,(struct sockaddr *) &server_bk_2, sizeof(server_bk_2)) == -1){
        std::cerr << "Error connecting to the server...\n";
        error = -1;
    }
    return error;
}

/**
 * This function executes a command in linux returning the output.
 * @param cmd
 * @return
 */
std::string execCommand(const char * cmd){
	std::string res;
	std::array<char,128> buffer{};

	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
	if (!pipe){
		throw std::runtime_error("popen() failed");
	}

	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr){
		res += buffer.data();
	}

	return res;
}

int close_socket(){
	int error=0;
	error=close(sd);
	if (error ==-1){
		cerr << "Error happened when closing socket."<< endl;
	}
	return error;	
}


void SendDataToMetricAIInflux(const std::string& hostname, int xmitdata, int xmitwait,
    const std::string& db_addr)
{
    const int influxport = 8086;
    const std::string org = "metric_ai";
    const std::string bucket = "metrics";
    const std::string measurement_name = "metrics";

    const char* token_env = std::getenv("METRIC_INFLUX_TOKEN");
    if (token_env == nullptr || std::string(token_env).empty()) {
        std::cerr << "ERROR: falta METRIC_INFLUX_TOKEN\n";
        return;
    }

    const std::string token(token_env);

    auto split_ws = [](const std::string& value) {
        std::vector<std::string> result;
        std::istringstream stream(value);
        std::string item;

        while (stream >> item)
            result.push_back(item);

        return result;
    };

    auto normalize_name = [](std::string name) {
        std::string result;

        for (char c : name) {
            if (c == '%') {
                result += "pct";
            } else if (
                c == '(' || c == ')' || c == '/' || c == '\\' ||
                c == '-' || c == '.' || c == ':' || c == '=' || c == ',') {
                if (!result.empty() && result.back() != '_')
                    result += '_';
            } else if (
                std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                result += c;
            }
        }

        while (!result.empty() && result.front() == '_')
            result.erase(result.begin());

        while (!result.empty() && result.back() == '_')
            result.pop_back();

        return result;
    };

    auto ignore_header_field = [](const std::string& field) {
        return field == "CUDAcomp";
    };

    const std::string header = get_header_line();
    const std::string log = get_log_line();

    std::vector<std::string> fields;
    size_t pos = 0;

    while (pos < header.size()) {
        while (
            pos < header.size() &&
            std::isspace(static_cast<unsigned char>(header[pos]))) {
            ++pos;
        }

        if (pos >= header.size())
            break;

        const size_t start = pos;

        while (
            pos < header.size() &&
            !std::isspace(static_cast<unsigned char>(header[pos])) &&
            header[pos] != '{') {
            ++pos;
        }

        const std::string main_name =
            header.substr(start, pos - start);

        if (pos >= header.size() || header[pos] != '{') {
            if (!main_name.empty())
                fields.push_back(normalize_name(main_name));

            continue;
        }

        ++pos;

        const size_t close = header.find('}', pos);
        if (close == std::string::npos) {
            std::cerr << "ERROR: header invalido. Falta '}'\n";
            return;
        }

        const std::string inside = header.substr(pos, close - pos);
        const std::vector<std::string> subfields = split_ws(inside);

        for (const auto& subfield : subfields) {
            if (ignore_header_field(subfield))
                continue;

            const std::string main = normalize_name(main_name);
            const std::string sub = normalize_name(subfield);

            if (!main.empty() && !sub.empty())
                fields.push_back(main + "_" + sub);
        }

        pos = close + 1;
    }

    const std::vector<std::string> values = split_ws(log);

    if (fields.size() != values.size()) {
        std::cerr << "ERROR INFLUX: numero de campos distinto "
                  << "del numero de valores. Fields=" << fields.size()
                  << ", values=" << values.size() << '\n';
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    influxdb_cpp::server_info server(
        db_addr,
        influxport,
        org,
        token,
        bucket
    );

    influxdb_cpp::builder builder;

    /* METRIC-AI espera measurement=metrics y tag entity=<nodo>.*/
    auto& measurement = builder.meas(measurement_name);
    measurement.tag("entity", hostname);

    size_t first_numeric_field = 0;

    if (!fields.empty() && fields[0] == "IP_Addr") {
        measurement.tag("IP_Addr", values[0]);
        first_numeric_field = 1;
    }

    if (first_numeric_field >= fields.size()) {
        std::cerr << "ERROR: no hay datos numericos para Influx\n";
        return;
    }

    double first_value;

    try {
        first_value = std::stod(values[first_numeric_field]);
    } catch (const std::exception&) {
        std::cerr << "ERROR convirtiendo "
                  << fields[first_numeric_field]
                  << " = "
                  << values[first_numeric_field] << '\n';
        return;
    }

    auto& influx_fields = measurement.field(
        fields[first_numeric_field],
        first_value
    );

    for (size_t i = first_numeric_field + 1; i < fields.size(); ++i) {
        try {
            const double value = std::stod(values[i]);
            influx_fields.field(fields[i], value, 6);
        } catch (const std::exception&) {
            std::cerr << "WARNING: valor no numerico: "
                      << fields[i] << " = " << values[i] << '\n';
        }
    }

    influx_fields
        .field("xmitdata", xmitdata)
        .field("xmitwait", xmitwait)
        .timestamp(timestamp)
        .post_http(server);
}

void SendDataToInflux_v2(
    std::string hostname, int xmitdata, int xmitwait, std::string db_addr){
    // int influxport = 8086;
    // std::string token =
    //     "ZpJf7k3DPgVeWlK3acd3GTSM8YE75JLpelxhS_J-YKqoNoHrtAhH3WMsux438vulM_XZ7BIPjH9OfqMU1eERAA==";
    // std::string org = "uc3m";
    // std::string bucket = "test";

    // Split por espacios
    auto split_ws = [](const std::string& str){
        std::vector<std::string> result;
        std::istringstream iss(str);
        std::string token;

        while (iss >> token)
            result.push_back(token);

        return result;
    };

	// =====================================================
    // Lambda para normalizar nombre para InfluxDB
    //
    // Ejemplos:
    //
    // MemUsage(%)       -> MemUsage_pct
    // 0PWUsage(Joules)  -> 0PWUsage_Joules
    // Gb/s              -> Gb_s
    // temp(Cº)          -> temp_C
    // =====================================================
    auto normalize_name = [](std::string name){
        std::string result;
        for (size_t i = 0; i < name.size(); ++i){
            char c = name[i];
            if (c == '%')
            {
                result += "pct";
            }
            // Separadores/puntuación -> _
            else if (
                c == '(' ||
                c == ')' ||
                c == '/' ||
                c == '\\' ||
                c == '-' ||
                c == '.' ||
                c == ':' ||
                c == '=' ||
                c == ','){
                if (!result.empty() && result.back() != '_')
                    result += '_';
            }
            // ASCII normal
            else if (
                std::isalnum(static_cast<unsigned char>(c)) ||
                c == '_'){
                result += c;
            }
            // Caracteres especiales UTF-8 como º se ignoran.
        }

        // Eliminar _ inicial/final
        while (!result.empty() && result.front() == '_')
            result.erase(result.begin());

        while (!result.empty() && result.back() == '_')
            result.pop_back();

        return result;
    };

    // Campos que no tienen valor en header y hay que ignorar
    auto ignore_header_field = [](const std::string& field)
    {
        return field == "CUDAcomp";
    };

    std::string header = get_header_line();
    std::string log = get_log_line();

    std::vector<std::string> fields;
    size_t pos = 0;
    while (pos < header.size()){
        // Saltar whitespace
        while (pos < header.size() && std::isspace(static_cast<unsigned char>(header[pos]))){
            ++pos;
        }

        if (pos >= header.size())
            break;

        size_t start = pos;

        while (
            pos < header.size() &&
            !std::isspace(
                static_cast<unsigned char>(header[pos])) &&
            header[pos] != '{')
        {
            ++pos;
        }

        std::string main_name =
            header.substr(start, pos - start);

        if (
            pos >= header.size() ||
            header[pos] != '{')
        {
            if (!main_name.empty())
            {
                fields.push_back(
                    normalize_name(main_name)
                );
            }

            continue;
        }

        ++pos; // saltar {

        size_t close = header.find('}', pos);
        if (close == std::string::npos){
            std::cerr << "ERROR: header invalido. Falta }\n";
            return;
        }

        std::string inside = header.substr(pos, close - pos);
        std::vector<std::string> subfields = split_ws(inside);

        for (const auto& subfield : subfields){
            // Campos que aparecen en header pero
            // no tienen posición en el log.
            if (ignore_header_field(subfield))
                continue;

            std::string complete_name = normalize_name(main_name) + "_" + normalize_name(subfield);
            fields.push_back(complete_name);
        }

        pos = close + 1;
    }

    // Parsear log de datos
    std::vector<std::string> values = split_ws(log);
    if (fields.size() != values.size()){
        std::cerr
            << "ERROR INFLUX: numero de campos distinto "
            << "del numero de valores\n";

        std::cerr
            << "Header fields: "
            << fields.size()
            << "\n";

        std::cerr
            << "Log values:    "
            << values.size()
            << "\n";


        // Debug detallado
        size_t max_size = std::max(fields.size(), values.size());

        for (size_t i = 0; i < max_size; ++i){
            std::cerr << i << " : ";

            if (i < fields.size())
                std::cerr << fields[i];
            else
                std::cerr << "<NO FIELD>";

            std::cerr << " = ";

            if (i < values.size())
                std::cerr << values[i];
            else
                std::cerr << "<NO VALUE>";

            std::cerr << "\n";
        }

        return;
    }

    // DEBUG 
   /* 
    std::cout << "---- INFLUX DATA ----" << std::endl;
    for (size_t i = 0; i < fields.size(); ++i){
        std::cout
            << i
            << " : "
            << fields[i]
            << " = "
            << values[i]
            << std::endl;
    }*/
    

    // Timestamp
    auto currtime = std::chrono::high_resolution_clock::now();
    auto dur = currtime.time_since_epoch();
    unsigned long long nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
    
    influxdb_cpp::server_info si(
        db_addr,
        influxport,
        org,
        token,
        bucket
    );

    influxdb_cpp::builder builder;
    auto& measurement = builder.meas(hostname);
    size_t first_numeric_field = 0;

    if (!fields.empty() && fields[0] == "IP_Addr"){
        measurement.tag( "IP_Addr", values[0]);
        first_numeric_field = 1;
    }

    if (first_numeric_field >= fields.size()){
        std::cerr << "ERROR: no hay datos numericos para Influx\n";

        return;
    }


    // =====================================================
    // Primer field
    // =====================================================

    double first_value;

    try
    {
        first_value =
            std::stod(
                values[first_numeric_field]
            );
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ERROR convirtiendo "
            << fields[first_numeric_field]
            << " = "
            << values[first_numeric_field]
            << std::endl;

        return;
    }


    auto& influx_fields =
        measurement.field(
            fields[first_numeric_field],
            first_value
        );

    // Resto de campos
    for (size_t i = first_numeric_field + 1; i < fields.size(); ++i){
        try{
            double value = std::stod(values[i]);
            influx_fields.field(fields[i], value, 6);
        }
        catch (const std::exception& e){
            std::cerr << "WARNING: valor no numerico: " << fields[i] << " = " << values[i] << "\n";
        }
    }

	// Para IBA
    influx_fields
        .field("xmitdata", xmitdata)
        .field("xmitwait", xmitwait);

    influx_fields
        .timestamp(nanoseconds)
        .post_http(si);

    std::string resp;
    std::string query(
        "from(bucket: \"" + bucket + "\")"
        "|> range(start: -1h)"
        "|>filter(fn: (r)=>r[\"_measurement\"] == \""
        + hostname +
        "\")"
        "|>filter(fn: (r) => r[\"_field\"] == \"xmitwait\")"
    );

    influxdb_cpp::flux_query(
        resp,
        query,
        si
    );
}

void SendDataToInflux(string hostname, int xmitdata, int xmitwait, std::string db_addr) {
	// ************* INFLUX INIT *******
	//std::string url = "127.0.0.1"; // URL --> changed to db_addr from conf_file
	// int influxport = 8086;
	// std::string token = "ZpJf7k3DPgVeWlK3acd3GTSM8YE75JLpelxhS_J-YKqoNoHrtAhH3WMsux438vulM_XZ7BIPjH9OfqMU1eERAA==";
	// std::string org = "uc3m"; // Org
	// std::string bucket = "test"; // bucket

	// Data to influx
	auto currtime = std::chrono::high_resolution_clock::now();
	auto dur = currtime.time_since_epoch();
	unsigned long long nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
	influxdb_cpp::server_info si(db_addr, influxport, org, token, bucket);
	// tokenize log and get data --> indexes 5 and 2
	int cpu = std::stoi(split(get_log_line(), ' ')[5]);
	int mem = std::stoi(split(get_log_line(), ' ')[2]);
	int temp0 = std::stoi(split(get_log_line(), ' ')[6]);
	int temp1 = std::stoi(split(get_log_line(), ' ')[7]);
	
	int gpu0_mem_usage = std::stoi(split(get_log_line(), ' ')[8]);
	int gpu0_cpu_usage = std::stoi(split(get_log_line(), ' ')[9]);
	int gpu0_temp = std::stoi(split(get_log_line(), ' ')[10]);
	int gpu0_watts = std::stoi(split(get_log_line(), ' ')[11]);

	int gpu1_mem_usage = std::stoi(split(get_log_line(), ' ')[12]);
	int gpu1_cpu_usage = std::stoi(split(get_log_line(), ' ')[13]);
	int gpu1_temp = std::stoi(split(get_log_line(), ' ')[13]);
	int gpu1_watts = std::stoi(split(get_log_line(), ' ')[15]);

	int gpu2_mem_usage = std::stoi(split(get_log_line(), ' ')[16]);
	int gpu2_cpu_usage = std::stoi(split(get_log_line(), ' ')[17]);
	int gpu2_temp = std::stoi(split(get_log_line(), ' ')[18]);
	int gpu2_watts = std::stoi(split(get_log_line(), ' ')[19]);

	int gpu3_mem_usage = std::stoi(split(get_log_line(), ' ')[20]);
	int gpu3_cpu_usage = std::stoi(split(get_log_line(), ' ')[21]);
	int gpu3_temp = std::stoi(split(get_log_line(), ' ')[22]);
	int gpu3_watts = std::stoi(split(get_log_line(), ' ')[23]);

	int gpu4_mem_usage = std::stoi(split(get_log_line(), ' ')[24]);
	int gpu4_cpu_usage = std::stoi(split(get_log_line(), ' ')[25]);
	int gpu4_temp = std::stoi(split(get_log_line(), ' ')[26]);
	int gpu4_watts = std::stoi(split(get_log_line(), ' ')[27]);

	int gpu5_mem_usage = std::stoi(split(get_log_line(), ' ')[28]);
	int gpu5_cpu_usage = std::stoi(split(get_log_line(), ' ')[29]);
	int gpu5_temp = std::stoi(split(get_log_line(), ' ')[30]);
	int gpu5_watts = std::stoi(split(get_log_line(), ' ')[31]);

	int gpu6_mem_usage = std::stoi(split(get_log_line(), ' ')[32]);
	int gpu6_cpu_usage = std::stoi(split(get_log_line(), ' ')[33]);
	int gpu6_temp = std::stoi(split(get_log_line(), ' ')[34]);
	int gpu6_watts = std::stoi(split(get_log_line(), ' ')[35]);

	int gpu7_mem_usage = std::stoi(split(get_log_line(), ' ')[36]);
	int gpu7_cpu_usage = std::stoi(split(get_log_line(), ' ')[37]);
	int gpu7_temp = std::stoi(split(get_log_line(), ' ')[38]);
	int gpu7_watts = std::stoi(split(get_log_line(), ' ')[39]);

	int gpu8_mem_usage = std::stoi(split(get_log_line(), ' ')[40]);
	int gpu8_cpu_usage = std::stoi(split(get_log_line(), ' ')[41]);
	int gpu8_temp = std::stoi(split(get_log_line(), ' ')[42]);
	int gpu8_watts = std::stoi(split(get_log_line(), ' ')[43]);

	influxdb_cpp::builder()
		.meas(hostname)
		.field("cpu", cpu)
		.field("mem", mem)
		//.field("xmitdata", xmitdata)
		//.field("xmitwait", xmitwait)
		.field("temp0", temp0)
		.field("temp1", temp1)
		.field("gpu0_mem_usage", gpu0_mem_usage)
		.field("gpu0_cpu_usage", gpu0_cpu_usage)
		.field("gpu0_temp", gpu0_temp)
		.field("gpu0_watts", gpu0_watts)
		.field("gpu1_mem_usage", gpu1_mem_usage)
		.field("gpu1_cpu_usage", gpu1_cpu_usage)
		.field("gpu1_temp", gpu1_temp)
		.field("gpu1_watts", gpu1_watts)
		.field("gpu2_mem_usage", gpu2_mem_usage)
		.field("gpu2_cpu_usage", gpu2_cpu_usage)
		.field("gpu2_temp", gpu2_temp)
		.field("gpu2_watts", gpu2_watts)
		.field("gpu3_mem_usage", gpu3_mem_usage)
		.field("gpu3_cpu_usage", gpu3_cpu_usage)
		.field("gpu3_temp", gpu3_temp)
		.field("gpu3_watts", gpu3_watts)
		.field("gpu4_mem_usage", gpu4_mem_usage)
		.field("gpu4_cpu_usage", gpu4_cpu_usage)
		.field("gpu4_temp", gpu4_temp)
		.field("gpu4_watts", gpu4_watts)
		.field("gpu5_mem_usage", gpu5_mem_usage)
		.field("gpu5_cpu_usage", gpu5_cpu_usage)
		.field("gpu5_temp", gpu5_temp)
		.field("gpu5_watts", gpu5_watts)
		.field("gpu6_mem_usage", gpu6_mem_usage)
		.field("gpu6_cpu_usage", gpu6_cpu_usage)
		.field("gpu6_temp", gpu6_temp)
		.field("gpu6_watts", gpu6_watts)
		.field("gpu7_mem_usage", gpu7_mem_usage)
		.field("gpu7_cpu_usage", gpu7_cpu_usage)
		.field("gpu7_temp", gpu7_temp)
		.field("gpu7_watts", gpu7_watts)
		.field("gpu8_mem_usage", gpu8_mem_usage)
		.field("gpu8_cpu_usage", gpu8_cpu_usage)
		.field("gpu8_temp", gpu8_temp)
		.field("gpu8_watts", gpu8_watts)
		.timestamp(nanoseconds)
		.post_http(si);

	string resp;
	string query(
		"from(bucket: \\\"" + bucket + "\\\")|> range(start: -1h)|>filter(fn: (r)=>r[\\\"_measurement\\\"] == \\\""
		+ hostname + "\\\")|>filter(fn: (r) => r[\\\"_field\\\"] == \\\"xmitwait\\\")");
	influxdb_cpp::flux_query(resp, query, si);
	//** Data sent
}
