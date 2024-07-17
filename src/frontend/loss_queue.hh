/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef LOSS_QUEUE_HH
#define LOSS_QUEUE_HH

#include <queue>
#include <cstdint>
#include <string>
#include <random>

#include "file_descriptor.hh"

class LossQueue
{
private:
    std::queue<std::string> packet_queue_ {};

    virtual bool drop_packet( const std::string & packet ) = 0;

protected:
    std::default_random_engine prng_;

public:
    LossQueue();
    virtual ~LossQueue() {}

    void read_packet( const std::string & contents );

    void write_packets( FileDescriptor & fd );

    unsigned int wait_time( void );

    bool pending_output( void ) const { return not packet_queue_.empty(); }

    static bool finished( void ) { return false; }
};

class IIDLoss : public LossQueue
{
private:
    std::bernoulli_distribution drop_dist_;

    bool drop_packet( const std::string & packet ) override;

public:
    IIDLoss( const double loss_rate ) : drop_dist_( loss_rate ) {}
};

class TraceLoss : public LossQueue
{
private:
    std::vector<uint64_t> schedule_;
    std::vector<int> loss_rate_;
    std::vector<std::bernoulli_distribution> drop_dist_;
    bool drop_direction_;
    uint64_t base_timestamp_;
    unsigned int next_delivery_;
    std::string loss_type_= "None";
    uint64_t pkt_cnt_=0;

    bool drop_packet( const std::string & packet ) override;
    bool gen_random_pkt( int schedule );

public:
    TraceLoss(const bool drop_direction, const std::string & filename, const std::string & loss_type="None");

}; 

class StochasticSwitchingLink : public LossQueue
{
private:
    bool link_is_on_;
    std::exponential_distribution<> on_process_;
    std::exponential_distribution<> off_process_;

    uint64_t next_switch_time_;

    bool drop_packet( const std::string & packet ) override;

public:
    StochasticSwitchingLink( const double mean_on_time_, const double mean_off_time );

    unsigned int wait_time( void );
};

class PeriodicSwitchingLink : public LossQueue
{
private:
    bool link_is_on_;
    uint64_t on_time_, off_time_, next_switch_time_;

    bool drop_packet( const std::string & packet ) override;

public:
    PeriodicSwitchingLink( const double on_time, const double off_time );

    unsigned int wait_time( void );
};

#endif /* LOSS_QUEUE_HH */
