#pragma once
#include "config/config.hh"
#include "ledger.hh"
#include "message/message.hh"
#include <unordered_map>

namespace paxos
{
    class Legislator
    {
    public:
        Legislator(const LegislatorConfig& config);
        LegislatorConfig config_;

        void initiate_ballot();

        void send_next_ballot(int ballot);
        void receive_next_ballot(Message message);
        void receive_next_ballot(int ballot, std::string sender);

        void send_last_vote(int ballot, int previous_vote, std::string sender);
        void receive_last_vote(Message message);
        void receive_enough_last_vote
            (std::unordered_map<std::string, int> quorum_last_votes);

        void receive_begin_ballot(int ballot, int decree);

        void handle_message(Message message);

        Ledger ledger;
    };
    using shared_legislator = std::shared_ptr<Legislator>;

    extern shared_legislator self;
    extern std::unordered_map<std::string, shared_legislator> legislators;
}
