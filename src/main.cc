#include <iostream>
#include <unordered_map>

#include "config/config.hh"
#include "legislator/legislator-factory.hh"
#include "events/register.hh"
#include "misc/logger.hh"

paxos::EventWatcherRegistry paxos::event_register;
std::unordered_map<std::string, paxos::shared_legislator> paxos::legislators;
paxos::shared_legislator paxos::self;

ev_signal sigint_watcher;
static void sigint_cb(struct ev_loop* loop, ev_signal*, int)
{
    ev_break(loop, EVBREAK_ALL);
}

ev_signal sigstop_watcher;
static void sigstop_cb(struct ev_loop*, ev_signal*, int)
{
    log("SIGSTOP detected trying to initiate ballot", paxos::red);
    paxos::self->initiate_ballot();
}


int main(int argc, char **argv)
{
    if (argc != 3)
        return 1;
    paxos::ServerConfig server_config = paxos::ServerConfig::parse(argv);

    ev_signal_init(&sigint_watcher, sigint_cb, SIGINT);
    paxos::event_register.get_event_loop().register_sigint_watcher(&sigint_watcher);

    ev_signal_init(&sigstop_watcher, sigstop_cb, SIGTSTP);
    paxos::event_register.get_event_loop().register_sigint_watcher(&sigstop_watcher);

    for (auto config : server_config.legislators_)
    {
        paxos::shared_legislator legislator = paxos::LegislatorFactory::Create(config);
        paxos::legislators.insert(std::pair(config.name, legislator));
        if (config.is_self)
            paxos::self = legislator;
    }
    
    paxos::event_register.get_event_loop()();
    return 0;
}
