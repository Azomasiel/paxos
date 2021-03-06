#include <arpa/inet.h>
#include <iostream>
#include <fstream>

#include "config.hh"
#include "parse.hh"

namespace paxos
{

    ServerConfig::ServerConfig(const std::vector<LegislatorConfig>& legislators)
        : legislators_(legislators)
    {
    }

    static std::string format_ip(std::string ip)
    {
        int family = 0;

        /* Check if it is an IPv4 or Ipv6 address */
        if (ip.find(":") != std::string::npos)
            family = AF_INET6;
        else
            family = AF_INET;

        /* Allocated static buffer to make conversion */
        unsigned char buf[sizeof(struct in6_addr)];
        char formatted_ip[INET6_ADDRSTRLEN];

        /* Convert ip string to network address structure */
        int success = inet_pton(family, ip.c_str(), buf);

        if (success <= 0)
            error_and_exit(1, "parse_configuration: " + ip + " is an invalid ip");

        /* Convert back the network address to an string */
        inet_ntop(family, buf, formatted_ip, INET6_ADDRSTRLEN);
        return formatted_ip;
    }


    static void parse_mandatory_keys(const json& j, LegislatorConfig& config)
    {
        int port_int;

        safe_get_key(j, "ip", config.ip, true);
        config.ip = format_ip(config.ip);

        safe_get_key(j, "port", port_int, true);
        config.port = std::to_string(port_int);

        safe_get_key(j, "name", config.name, true);
    }


    static void from_json(const json& j, LegislatorConfig& config)
    {
        parse_mandatory_keys(j, config);
    }


    static std::vector<LegislatorConfig> parse_legislators(const json& j,
            const std::string& name)
    {
        /* Get 'vhosts' value */
        std::vector<json> legislators;
        safe_get_key(j, "legislators", legislators, true);

        /* Create VHostConfig vector and fill with VHostConfig */
        std::vector<LegislatorConfig> legislator_configs;
        int i = 0;

        for (auto it : legislators)
        {
            /* Differenciable vhost checking */
            auto legislator = it.get<paxos::LegislatorConfig>();
            legislator.ballot_partition_id = i;
            legislator.is_self = name == legislator.name;
            legislator_configs.push_back(legislator);
            ++i;
        }

        return legislator_configs;
    }


    ServerConfig ServerConfig::parse(char **argv)
    {
        std::string path = argv[1];
        std::string name = argv[2];

        json json_dict;
        std::ifstream json_file(path);

        /* Check if file is corretly opened */
        if (!json_file.is_open())
        {
            error_and_exit(1, "parse_configuration: file " + path
                    + " doesn't exist.");
        }


        /* Check if file respect the json syntax */
        try
        {
            json_file >> json_dict;
        }
        catch (const nlohmann::detail::parse_error& e)
        {
            error_and_exit(1, e.what());
        }

        auto legislators = parse_legislators(json_dict, name);

        return ServerConfig(legislators);
    }
}
