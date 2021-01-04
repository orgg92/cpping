#include <iostream>
#include <cmath>
#include <math.h>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <net/if.h>
#include <stdio.h>
#include <tins/tins.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <vector>
#include <string>

using namespace Tins;

struct interface {
  std::string name;
  std::string address;
  std::string MAC;
  std::string mask;
};

struct Host {
    std::string ip;
    std::string mac;
    std::string hostname;
};

void printInts(std::vector<interface> interfaces)
{
     std::cout << "Available interfaces... \n" << std::endl;
     std::cout << std::left << std::setw(30) << "Iface" << "Address" <<  std::endl;

    for (int j = 0; j < interfaces.size(); j++)
    {
        std::cout << std::left << std::setw(30) << interfaces[j].name << interfaces[j].address << "/" << interfaces[j].mask << std::endl;
    }

    std::cout << "" << std::endl;
}

void progressBar(int inc) {

    float progress = ((float)inc/(float)254)*100;
    int pg = std::round((int)progress);

    std::string pbar((pg/4)+1, '*');

    std::stringstream progss;
//    progss << inc;
//    progss << "/254";
    progss << std::round(progress);
    progss << "%";
    std::string prog=progss.str();
    if (inc < 254) {
        std::cout << "[" << pbar << "] " << prog << "\r" << std::flush;
    } else {
    // Empty string needs to clear progress bar completely
        std::cout << "                      " << "\r" << std::flush;
    }


}

std::string selectInt(std::vector<interface> interfaces)
{
    std::string selection;

    while (selection.length() < 1) {

        // std in to ask user which interface to use
        std::cout << "Select interface to sweep on: " << std::flush;
        getline(std::cin, selection);

        auto pred = [selection](const interface & item) {
                    return item.name == selection;
        };

        if (selection == "") {
                std::cout << "Invalid selection" << std::endl;
        }

        else if (std::find_if(std::begin(interfaces), std::end(interfaces), pred) != std::end(interfaces))
        {
            break;
        }

        else  {
            std::cout << "Invalid selection: " << std::endl;
            selection = "";
        }
    }
    return selection;
}

std::string getIP(std::vector<interface> interfaces, std::string selection)
{
    for (int i=0; i < interfaces.size(); i++)
    {
        if (interfaces[i].name == selection) {
            return interfaces[i].address;
        }
    }
}

std::vector<interface> checkInterfaces() {

    std::vector<interface> interfaces;

    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    // i want to get IPv4 addrress
    ifr.ifr_addr.sa_family = AF_INET;

    // I want IP attached to <int>
    struct if_nameindex *if_nidxs, *intf;
    if_nidxs = if_nameindex();

    if ( if_nidxs != NULL )
    {
        int i = 0;
        for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++)
        {
            // get IP of named interface
            strncpy(ifr.ifr_name, intf->if_name, IFNAMSIZ-1);
            ioctl(fd,SIOCGIFADDR, &ifr);
            std::string address = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

            // get mask of interface
            strncpy(ifr.ifr_name, intf->if_name, IFNAMSIZ-1);
            ioctl(fd,SIOCGIFNETMASK, &ifr);
            std::string mask = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

            // add details to interfaces vector
            interfaces.push_back(interface());
            interfaces[i].name = intf->if_name;
            interfaces[i].address = address;
            interfaces[i].mask = mask;

            // compiler doesn't like accessing each element through intf so manual increment
            i++;
        }
        if_freenameindex(if_nidxs);
    }
        return interfaces;
}

std::vector<int> ipBreak(std::string myIP)
{
    size_t pos = 0;
    int i = 0;
    std::string token;
    std::vector<int> ip;

    std::istringstream ss(myIP);

    while(std::getline(ss, token, '.'))
    {
        ip.push_back(std::stoi(token));
    }
    return ip;
}

std::string callSys(std::string cmd)
{
        char buffer[128];
        std::string result = "";

        // sys call

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cout << "Popen failed" << std::endl;
        }

        while (!feof(pipe)) {

            if (fgets(buffer, 128, pipe) != NULL)
            {
                result += buffer;
            }

        }

        pclose(pipe);
        return result;
}

int main(int argc, char* argv[])
{

    std::string lineBreak = "********************************************************************************";

// create vector to store hosts in
    std::vector<Host> hosts;
    std::vector<interface> interfaces;

    interfaces = checkInterfaces();

    // print interface status from vector
    printInts(interfaces);

// ask user which interface to sweep on
// function - input, return string (i.e. selection)

    std::string selection = selectInt(interfaces);
    std::cout << "Starting sweep on [" << selection << "]" << std::endl;

// take IP of named interface (selection), and split @ full stops.

   std::string myIP = getIP(interfaces, selection);
   std::cout << "" << std::endl;
   std::vector<int> octets = ipBreak(myIP);

   // looping through a /24 - start at .1 because .0 is broadcast

    octets[3] = 1;
// (later) check subnet mask, convert each octet to binary

    for (int i =0; i < 254; i++)
    {

    // create command string

         // build host IP
        std::stringstream hs;
        hs << octets[0];
        hs << ".";
        hs << octets[1];
        hs << ".";
        hs << octets[2];
        hs << ".";
        hs << octets[3];

        std::string host = hs.str();

    // turn this into a function (EASY)

        std::stringstream ss;
        ss << "ping -c 1 -t 32 -W 1 ";
        ss << host;

        std::string cmd = ss.str();

        // Popen and return stdout (ping)
        std::string result = callSys(cmd);

        // if packet is 100% loss...
        // else add to "Hosts" vector, we can then check the systems arp cache for the MAC

        if (result.find("100% packet loss") != std::string::npos) {
//            std::cout << host << " [Host timed out]" << std::endl;
                // try arping for devices filtering ICMP

                std::string arp = callSys("arping -c 1 -I " + selection + " " + host);

                if (arp.find("Unicast reply from") != std::string::npos)
                {
                    std::cout << std::left << std::setw(20) << host << " [Host is alive - filtering ICMP]" << std::endl;
                    std::string mac = callSys("arp " + host + "| grep -w " + selection);

                    Host newHost;
                    newHost.ip = host;
                    newHost.mac = mac;
                    hosts.push_back(newHost);
                }

        } else {
            std::cout << std::left << std::setw(20) << host << " [Host is alive]" << std::endl;
            if (host != myIP) {
                std::string mac = callSys("arp " + host + "| grep -w " + selection);

                Host newHost;
                newHost.ip = host;
                newHost.mac = mac;
                hosts.push_back(newHost);
            }
        }


        progressBar(octets[3]);
        octets[3] ++;
    }

    std::cout << "" << std::endl;

    std::stringstream info;
    info << "Found ";
    info << hosts.size();
    info << " host(s) on the network";

    std::string _info = info.str();

    std::cout << _info << std::endl;
    std::cout << lineBreak << std::endl;

    // report on IP & MAC

    for (int i = 0; i < hosts.size(); i++)
    {
        std::cout << hosts[i].mac << std::flush;
    }

    std::cout << "" << std::endl;

    return 0;
}
