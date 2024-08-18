/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>
#include <iostream>
#include <fstream>

#include "delay_queue.hh"
#include "timestamp.hh"

using namespace std;

void DelayQueue::read_packet( const string & contents )
{
    packet_queue_.emplace( timestamp() + delay_ms_, contents );
}

void DelayQueue::write_packets( FileDescriptor & fd )
{
    while ( (!packet_queue_.empty())
            && (packet_queue_.front().first <= timestamp()) ) {
        fd.write( packet_queue_.front().second );
        packet_queue_.pop();
    }
}

unsigned int DelayQueue::wait_time( void ) const
{
    if ( packet_queue_.empty() ) {
        return numeric_limits<uint16_t>::max();
    }

    const auto now = timestamp();

    if ( packet_queue_.front().first <= now ) {
        return 0;
    } else {
        return packet_queue_.front().first - now;
    }
}

DelayTraceQueue::DelayTraceQueue( const string & filename )
    : schedule_(),
      delay_(),
      packet_queue_(),
      base_timestamp_( timestamp() ),
      next_delivery_( 0 )
{
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
        const uint64_t delay = stoull( line.substr( space + 1 ) );

        schedule_.emplace_back( ms );
        delay_.emplace_back( delay );
    }

    if ( schedule_.empty() ) {
        throw runtime_error( filename + ": no valid trace data found" );
    }
}

void DelayTraceQueue::read_packet( const string & contents )
{
    const uint64_t now = timestamp();
    const uint64_t elapsed = now - base_timestamp_;
    uint64_t trace_timestamp = schedule_.at(next_delivery_);
    uint64_t delay_ms_;

    if ( elapsed <= trace_timestamp ) {
        delay_ms_ = delay_.at(next_delivery_);
    } else {
        next_delivery_ = (next_delivery_ + 1) % schedule_.size();
        if ( next_delivery_ == 0 ) {
            base_timestamp_ = now;
        }
        delay_ms_ = delay_.at(next_delivery_);
    }

    packet_queue_.emplace_back( now + delay_ms_, contents );
}

void DelayTraceQueue::write_packets( FileDescriptor & fd )
{
    uint64_t now = timestamp();

    for ( auto it = packet_queue_.begin(); it != packet_queue_.end(); )
    {
        if ( it->first <= now ) {
            fd.write( it->second );
            it = packet_queue_.erase( it );
        }
        else {
            ++it;
        }
    }
}

unsigned int DelayTraceQueue::wait_time( void ) const
{
    if ( packet_queue_.empty() ) {
        return numeric_limits<uint16_t>::max();
    }

    const auto now = timestamp();

    if ( packet_queue_.front().first <= now ) {
        return 0;
    } else {
        return packet_queue_.front().first - now;
    }
}