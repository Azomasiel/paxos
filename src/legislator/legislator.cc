#include <string>

#include "legislator.hh"
#include "misc/logger.hh"
#include "events/register.hh"
#include "events/send.hh"
#include "vote.hh"

namespace paxos
{
    Legislator::Legislator(const LegislatorConfig& config)
        : config_(config), ledger(config.name), has_started(false)
    {
        log("created legislator " + config.name, blue);
    }

    void Legislator::initiate_ballot()
    {
        has_started = false;
        quorum_previous_votes.clear();
        int new_ballot_number = get_next_ballot_id();
        ledger.set_last_tried(new_ballot_number);
        log(config_.name + " is initiating ballot " + std::to_string(new_ballot_number), green);
        send_next_ballot(new_ballot_number);
    }

    int Legislator::get_next_ballot_id()
    {
        int previous_ballot_id = ledger.last_tried();
        if (previous_ballot_id == -1)
            previous_ballot_id = config_.ballot_partition_id - legislators.size();
        int last_voted_ballot = ledger.next_bal();
        while (last_voted_ballot > (int)legislators.size() + previous_ballot_id)
            previous_ballot_id += legislators.size();
        return previous_ballot_id + legislators.size();
    }

    void Legislator::send_next_ballot(int ballot)
    {
        std::string ballot_string = std::to_string(ballot);
        Message message;
        message.set_method("NextBallot");
        message.add_header("ballot", ballot_string);
        message.add_header("sender", self->config_.name);

        for (auto legislator : legislators)
            message.send(legislator.second);
    }

    void Legislator::receive_next_ballot(Message message)
    {
        std::string ballot_str = *message.get_header("ballot");
        int ballot = std::stoi(ballot_str);
        receive_next_ballot(ballot, *message.get_header("sender"));
    }

    void Legislator::receive_next_ballot(int ballot, std::string sender)
    {
        log(config_.name + " has received a NextBallot("
                + std::to_string(ballot)
                + ") from " + sender, green);
        int next_ballot = ledger.next_bal();
        if (ballot <= next_ballot)
        {
            log("but it was discarded because ballot " + std::to_string(ballot)
                    + " is inferior or equal to nextBallot "
                    + std::to_string(next_ballot), red);
            send_higher_ballot(next_ballot, sender);
            return;
        }
        ledger.set_next_bal(ballot);
        Vote previous_vote = ledger.prev_vote();
        previous_vote = previous_vote;
        send_last_vote(ballot, previous_vote, sender);
    }

    void Legislator::send_higher_ballot(int ballot, std::string receiver)
    {
        Message message;
        message.set_method("HigherBallot");
        message.add_header("ballot", std::to_string(ballot));
        message.send(legislators[receiver]);
    }

    void Legislator::receive_higher_ballot(Message message)
    {
        std::string ballot_str = *message.get_header("ballot");
        log(config_.name + " has received a HigherBallot("
                + ballot_str + ")", red);

        receive_higher_ballot(std::stoi(ballot_str));
    }

    void Legislator::receive_higher_ballot(int ballot)
    {
        int last_tried = ledger.last_tried();
        if (last_tried > ballot)
        {
            log("But it was discarded because an higher ballot ("
                    + std::to_string(last_tried) + ") as been found before", red);

            return;
        }

        while (last_tried + (int)legislators.size() < ballot)
        {
            last_tried += legislators.size();
        }
        ledger.set_last_tried(last_tried);
        initiate_ballot();
    }

    void Legislator::send_last_vote(int ballot, Vote previous_vote,
            std::string sender)
    {
        std::string ballot_string = std::to_string(ballot);
        std::string vote_ballot_id_string
            = std::to_string(previous_vote.ballot_id);
        std::string decree_string = std::to_string(previous_vote.decree.decree);
        Message message;
        message.set_method("LastVote");
        message.add_header("ballot", ballot_string);
        message.add_header("vote_ballot_id", vote_ballot_id_string);
        message.add_header("decree", decree_string);
        message.add_header("sender", self->config_.name);
        message.send(legislators[sender]);

    }

    void Legislator::receive_last_vote(Message message)
    {
        std::string ballot_str = *message.get_header("ballot");
        std::string vote_ballot_id_str = *message.get_header("vote_ballot_id");
        std::string sender =  *message.get_header("sender");
        log(config_.name + " has received a LastVote("
                + ballot_str + ", " + vote_ballot_id_str
                + ") from " + sender, green);
        int ballot = std::stoi(ballot_str);
        int vote_ballot_id = std::stoi(vote_ballot_id_str);
        int vote_decree = std::stoi(*message.get_header("decree"));

        unsigned int nb_legislators = legislators.size();
        if (ballot != ledger.last_tried() || has_started)
        {
            log("but it was discarded because ballot " + std::to_string(ballot)
                    + " is outdated, it is either no longer in treatment or already started"
                    , red);
            return;
        }

        Decree decree;
        decree.decree = vote_decree;
        Vote vote;
        vote.decree = decree;
        vote.legislator = sender;
        vote.ballot_id = vote_ballot_id;

        quorum_previous_votes.insert(std::pair<std::string, Vote>
                (sender, vote));
        unsigned int quorum_size = quorum_previous_votes.size();
        if (quorum_size > nb_legislators / 2)
            receive_enough_last_vote();
    }

    void Legislator::receive_enough_last_vote()
    {
        has_started = true;
        int ballot = ledger.last_tried();
        std::string ballot_str = std::to_string(ballot);

        log("===============================================\n"
                + config_.name + " has received enough LastVote("
                + ballot_str + ", v"
                + ")\n"
            "===============================================", green);


        Vote max_vote;
        max_vote.ballot_id = -1;
        for (auto legislator_vote_pair : quorum_previous_votes)
        {
            Vote current_vote = legislator_vote_pair.second;
            if (current_vote.ballot_id > max_vote.ballot_id)
                max_vote = current_vote;
        }

        Decree decree;

        if (max_vote.ballot_id != -1)
            decree = max_vote.decree;
        else
            decree.decree = ballot;

        send_begin_ballot(ballot, decree);
    }

    void Legislator::send_begin_ballot(int ballot, Decree decree)
    {
        std::string ballot_string = std::to_string(ballot);
        std::string decree_string = std::to_string(decree.decree);

        Message message;
        message.set_method("BeginBallot");
        message.add_header("sender", self->config_.name);
        message.add_header("ballot", ballot_string);
        message.add_header("decree", decree_string);
        for (auto legislator_vote_pair : quorum_previous_votes)
        {
            std::string legislator = legislator_vote_pair.first;
            message.send(legislators[legislator]);
        }
    }

    void Legislator::receive_begin_ballot(Message message)
    {
        std::string ballot_string = *message.get_header("ballot");
        std::string decree_string = *message.get_header("decree");
        std::string sender = *message.get_header("sender");

        log(config_.name + " has received BeginBallot("
                + ballot_string + ", " + decree_string
                + ") from " + sender, green);

        int ballot = std::stoi(ballot_string);
        int decree = std::stoi(decree_string);
        receive_begin_ballot(ballot, decree, sender);
    }

    void Legislator::receive_begin_ballot(int ballot, int decree_id,
            std::string sender)
    {
        int next_ballot = ledger.next_bal();
        if (ballot != next_ballot)
        {
            if (ballot < next_ballot)
                send_higher_ballot(next_ballot, sender);
            return;
        }
        Vote vote;
        vote.ballot_id = ballot;
        Decree decree;
        decree.decree = decree_id;
        vote.decree = decree;
        ledger.set_prev_vote(vote);

        send_voted(ballot, decree, sender);
    }

    void Legislator::send_voted(int ballot, Decree decree, std::string receiver)
    {
        Message message;
        message.set_method("Voted");
        message.add_header("ballot", std::to_string(ballot));
        message.add_header("sender", self->config_.name);
        message.add_header("decree", std::to_string(decree.decree));
        message.send(legislators[receiver]);
    }

    void Legislator::receive_voted(Message message)
    {
        std::string ballot_str = *message.get_header("ballot");
        std::string sender = *message.get_header("sender");
        std::string decree_str = *message.get_header("decree");
        Decree decree;
        decree.decree = std::stoi(decree_str);

        log(config_.name + " has received Voted("
                + ballot_str
                + ") from " + sender, green);

        receive_voted(std::stoi(ballot_str), decree, sender);
    }

    void Legislator::receive_voted(int ballot, Decree decree, std::string voter)
    {
        if (ballot != ledger.last_tried())
            return;
        quorum_previous_votes.erase(voter);
        if (quorum_previous_votes.size() == 0)
            receive_enough_voted(ballot, decree);
    }

    void Legislator::receive_enough_voted(int ballot, Decree decree)
    {
        log(config_.name + " has received enough Voted("
                + std::to_string(ballot)
                + "), saving decree: " + std::to_string(decree.decree), green);
        ledger.set_decree(decree);
        send_success(decree);
    }

    void Legislator::send_success(Decree decree)
    {
        Message message;
        message.set_method("Success");
        message.add_header("decree", std::to_string(decree.decree));

        for (auto legislator : legislators)
            message.send(legislator.second);
    }

    void Legislator::receive_success(Message message)
    {
        std::string decree_str = *message.get_header("decree");
        Decree decree;
        decree.decree = std::stoi(decree_str);
        receive_success(decree);
    }

    void Legislator::receive_success(Decree decree)
    {
        ledger.set_decree(decree);
        std::string new_decree= std::to_string(ledger.get_decree().decree);
        log("=======================================\n"
            "Received success, the new decree is now: " + new_decree +
            "\n=======================================", green);
    }

    void Legislator::handle_message(Message message)
    {
        std::string method = message.get_method();
        if (method == "NextBallot")
            receive_next_ballot(message);
        else if (method == "LastVote")
            receive_last_vote(message);
        else if (method == "BeginBallot")
            receive_begin_ballot(message);
        else if (method == "Voted")
            receive_voted(message);
        else if (method == "Success")
            receive_success(message);
        else if (method == "HigherBallot")
            receive_higher_ballot(message);
    }
}
