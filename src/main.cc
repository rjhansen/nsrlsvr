#include "main.h"
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <algorithm>
#include <limits.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <regex>

#ifdef __FreeBSD__
#include <sys/socket.h>
#include <netinet/in.h>
#endif

using std::string;
using std::transform;
using std::ifstream;
using std::cerr;
using std::cout;
using std::vector;
using std::remove_if;
using std::sort;
using std::pair;
using std::regex;
using std::stoi;
using std::to_string;
using std::getline;

namespace
{
vector<pair64> hash_set;
string hashes_location { PKGDATADIR "/hashes.txt" };
uint16_t port { 9120 };

void load_hashes()
{
    const regex md5_re {
        "^[A-Fa-f0-9]{32}$"
    };
    vector<char> buf(1024, 0);
    uint32_t hash_count { 0 };
    ifstream infile { hashes_location.c_str() };

    try
    {
        hash_set.reserve(40000000);
    }
    catch (std::bad_alloc& ba)
    {
        log(LogLevel::ALERT, "couldn't reserve enough memory");
        exit(EXIT_FAILURE);
    }

    if (not infile)
    {
        log(LogLevel::ALERT, "couldn't open hashes file " +
            hashes_location);
        exit(EXIT_FAILURE);
    }

    while (infile)
    {
        string line;
        getline(infile, line);
        transform(line.begin(), line.end(), line.begin(), ::toupper);
        if (0 == line.size()) continue;

        if (! regex_match(line.cbegin(), line.cend(), md5_re))
        {
            log(LogLevel::ALERT,
                "hash file appears corrupt!  Loading no hashes.");
            log(LogLevel::ALERT,
                "offending line is: " + line);
            log(LogLevel::ALERT,
                "shutting down!");
            exit(EXIT_FAILURE);
            return;
        }
        
        try
        {
            hash_set.push_back(to_pair64(line));
            hash_count += 1;
            if (0 == hash_count % 1000000)
            {
                string howmany { to_string(hash_count / 1000000) };
                log(LogLevel::ALERT,
                    "loaded " + howmany + " million hashes");
            }
        }
        catch (std::bad_alloc& ba)
        {
            log(LogLevel::ALERT,
                "couldn't allocate enough memory");
            exit(EXIT_FAILURE);
            return;
        }
    }
    string howmany { to_string(hash_count) };
    log(LogLevel::INFO,
        "read in " + howmany + " unique hashes");

    infile.close();

    sort(hash_set.begin(), hash_set.end());
    if (hash_set.size() > 1)
    {
        log(LogLevel::INFO,
            "ensuring no duplicates");
        pair64& foo { hash_set.at(0) };
        for (auto iter = (hash_set.cbegin() + 1) ;
                iter != hash_set.cend();
                ++iter)
        {
            if (foo == *iter)
            {
                log(LogLevel::ALERT,
                    "hash file contains duplicates -- "
                    "shutting down!");
                exit(EXIT_FAILURE);
                return;
            }
            foo = *iter;
        }
    }
}


void daemonize()
{
    const auto pid = fork();
    if (pid < 0)
    {
        log(LogLevel::WARN, "couldn't fork!");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }
    log(LogLevel::INFO, "daemon started");

    umask(0);

    if (setsid() < 0)
    {
        log(LogLevel::WARN, "couldn't set sid");
        exit(EXIT_FAILURE);
    }

    if (0 > chdir("/"))
    {
        log(LogLevel::WARN, "couldn't chdir to root");
        exit(EXIT_FAILURE);
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

}


auto make_socket()
{
    sockaddr_in server;
    memset(static_cast<void*>(&server), 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    const auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        log(LogLevel::WARN, "couldn't create a server socket");
        exit(EXIT_FAILURE);
    }
    if (0 > bind(sock, reinterpret_cast<sockaddr*>(&server),
                 sizeof(server)))
    {
        log(LogLevel::WARN, "couldn't bind to port 9120");
        exit(EXIT_FAILURE);
    }
    if (0 > listen(sock, 20))
    {
        log(LogLevel::WARN, "couldn't listen for clients");
        exit(EXIT_FAILURE);
    }
    log(LogLevel::INFO, "ready for clients");

    return sock;
}

void show_usage(string program_name)
{
    cout <<
         "Usage: "
         << program_name
         << " [-vbhs -f FILE -p PORT]\n\n"
         "-v : print version information\n"
         "-b : get information on reporting bugs\n"
         "-f : specify an alternate hash set (default: "
         << PKGDATADIR <<
         "/hashes.txt)\n"
         "-h : show this help message\n"
         "-p : listen on PORT, between 1024 and 65535 (default: 9120)\n\n";
}


void parse_options(int argc, char* argv[])
{
    int32_t opt { 0 };

    while (-1 != (opt = getopt(argc, argv, "bsvof:hp:t:S")))
    {
        switch (opt)
        {
        case 'v':
            cout << argv[0]
                 << " "
                 << PACKAGE_VERSION
                 << "\n\n";
            exit(EXIT_SUCCESS);
            break;
            
        case 'b':
            cout << argv[0]
                 << " "
                 << PACKAGE_VERSION
                 << "\n"
                 << PACKAGE_URL
                 << "\n"
                 "Praise, blame and bug reports to " << PACKAGE_BUGREPORT << ".\n\n"
                 "Please be sure to include your operating system, version of your\n"
                 "operating system, and a detailed description of how to recreate\n"
                 "your bug.\n\n";
            exit(EXIT_SUCCESS);
            break;
            
        case 'f':
        {
            hashes_location = string((const char*) optarg);
            ifstream infile { hashes_location.c_str() };
            if (not infile)
            {
                cerr <<
                     "Error: the specified dataset file could not be found.\n\n";
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'h':
            show_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
            
        case 'p':
            try
            {
                auto port_num = stoi(optarg);
            }
            catch (...)
            {
                cerr << "Error: invalid value for port\n\n";
                exit(EXIT_FAILURE);
            }
            break;
        default:
            show_usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
}

}

const vector<pair64>& hashes { hash_set };

void log(const LogLevel level, const string msg)
{
    syslog(LOG_MAKEPRI(LOG_USER, static_cast<int>(level)), msg.c_str());
}

int main(int argc, char* argv[])
{
    parse_options(argc, argv);
    daemonize();
    load_hashes();
    signal(SIGCHLD, SIG_IGN);

    int32_t client_sock {0};
    int32_t svr_sock { make_socket() };
    sockaddr_in client;
    socklen_t client_length { sizeof(client) };

SERVER_LOOP:
    if (0 > (client_sock = accept(svr_sock,
                                  reinterpret_cast<sockaddr*>(&client),
                                  &client_length)))
    {
        log(LogLevel::WARN, "dropped a connection");
        goto SERVER_LOOP;
    }
    string ipaddr { inet_ntoa(client.sin_addr) };
    log(LogLevel::ALERT, string("accepted a client: ") + ipaddr);
        
    if (0 == fork()) {
        log(LogLevel::ALERT, "calling handle_client");
        handle_client(client_sock);
        return 0;
    }
    goto SERVER_LOOP;

    return EXIT_SUCCESS;
}


