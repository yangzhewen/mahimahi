/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>
#include <stdexcept>
#include <fstream>
#include <string>
#include <iostream>
#include <unistd.h>

#include "loss_queue.hh"
#include "timestamp.hh"

using namespace std;

LossQueue::LossQueue()
    : prng_( random_device()() )
{}

bool LossQueue::check()
{
    if ( access( "/home/lw/code/sparkrtc/my_experiment/file/start_server", F_OK) == 0) {
        if ( !ready_ ) {
            ready_ = true;
            reset_time_stamp_ = true;
            reset_cnt_++;
        }
        return true;
    } else {
        if ( ready_ ) {
            ready_ = false;
        }
        return false;
    }
}

void LossQueue::read_packet( const string & contents )
{
    if ( not check()) {
        return;
    }

    if ( not drop_packet( contents ) ) {
        packet_queue_.emplace( contents );
    }
}

void LossQueue::write_packets( FileDescriptor & fd )
{
    while ( not packet_queue_.empty() ) {
        fd.write( packet_queue_.front() );
        packet_queue_.pop();
    }
}

unsigned int LossQueue::wait_time( void )
{
    return packet_queue_.empty() ? numeric_limits<uint16_t>::max() : 0;
}

bool IIDLoss::drop_packet( const string & packet __attribute((unused)) )
{
    return drop_dist_( prng_ );
}

static const double MS_PER_SECOND = 1000.0;

TraceLoss::TraceLoss( const bool drop_direction, const string & filename )
    : schedule_(),
      loss_rate_(),
      drop_dist_(),
      drop_direction_( drop_direction ),
      base_timestamp_( timestamp() ),
      next_delivery_( 0 )
      
{
    if ( !drop_direction_ ) {
        return ;
    }

    ifstream trace_file( filename );

    if ( not trace_file.good() ) {
        throw runtime_error( filename + ": error opening for reading" );
    }

    string line;

    while ( trace_file.good() and getline( trace_file, line ) ) {
        if ( line.empty() ) {
            throw runtime_error( filename + ": invalid empty line" );
        }

        const size_t space = line.find( ',' );

        if ( space == string::npos ) {
            throw runtime_error( filename + ": invalid line: " + line );
        }

        const uint64_t ms = stoull( line.substr( 0, space ) );
        const float loss_rate = stof( line.substr( space + 1 ) );

        schedule_.emplace_back( ms );
        loss_rate_.emplace_back( loss_rate * 100 );
        drop_dist_.emplace_back( std::bernoulli_distribution(loss_rate) );
    }

    if ( schedule_.empty() ) {
        throw runtime_error( filename + ": no valid trace data found" );
    }

    srand(0);
}

bool TraceLoss::gen_random_pkt( int schedule )
{
    int loss_rate = loss_rate_.at(schedule);
    int random = (rand() % 100) + 1;
    return random <= loss_rate;
}

bool TraceLoss::drop_packet( const string & packet __attribute((unused)) )
{
    if ( !drop_direction_ ) {
        return false;
    }

    if ( reset_time_stamp_ ) {
        base_timestamp_ = timestamp();
        next_delivery_ = 0;
        reset_time_stamp_ = false;
    }

    const uint64_t now = timestamp();
    const uint64_t elapsed = now - base_timestamp_;
    uint64_t trace_timestamp = schedule_.at(next_delivery_);
    bool res;

    if ( reset_cnt_ == 41 ) {
        srand(0);
        reset_cnt_ = 0;
    }

    if ( elapsed <= trace_timestamp) {
        res = gen_random_pkt(next_delivery_);
        return res;
    } else {
        next_delivery_ = (next_delivery_ + 1) % schedule_.size();
        if ( next_delivery_ == 0 ) {
            base_timestamp_ = now;
        }
        res = gen_random_pkt(next_delivery_);
        return res;
    }
}

StochasticSwitchingLink::StochasticSwitchingLink( const double mean_on_time, const double mean_off_time )
    : link_is_on_( false ),
      on_process_( 1.0 / (MS_PER_SECOND * mean_off_time) ),
      off_process_( 1.0 / (MS_PER_SECOND * mean_on_time) ),
      next_switch_time_( timestamp() )
{}

uint64_t bound( const double x )
{
    if ( x > (1 << 30) ) {
        return 1 << 30;
    }

    return x;
}

unsigned int StochasticSwitchingLink::wait_time( void )
{
    const uint64_t now = timestamp();

    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        /* worried about integer overflow when mean time = 0 */
        next_switch_time_ += bound( (link_is_on_ ? off_process_ : on_process_)( prng_ ) );
    }

    if ( LossQueue::wait_time() == 0 ) {
        return 0;
    }

    if ( next_switch_time_ - now > numeric_limits<uint16_t>::max() ) {
        return numeric_limits<uint16_t>::max();
    }

    return next_switch_time_ - now;
}

bool StochasticSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    return !link_is_on_;
}

PeriodicSwitchingLink::PeriodicSwitchingLink( const double on_time, const double off_time )
    : link_is_on_( false ),
      on_time_( bound( MS_PER_SECOND * on_time ) ),
      off_time_( bound( MS_PER_SECOND * off_time ) ),
      next_switch_time_( timestamp() )
{
  if ( on_time_ == 0 and off_time_ == 0 ) {
      throw runtime_error( "on_time and off_time cannot both be zero" );
  }
}

unsigned int PeriodicSwitchingLink::wait_time( void )
{
    const uint64_t now = timestamp();

    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        next_switch_time_ += link_is_on_ ? on_time_ : off_time_;
    }

    if ( LossQueue::wait_time() == 0 ) {
        return 0;
    }

    if ( next_switch_time_ - now > numeric_limits<uint16_t>::max() ) {
        return numeric_limits<uint16_t>::max();
    }

    return next_switch_time_ - now;
}

bool PeriodicSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    return !link_is_on_;
}
