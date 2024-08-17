/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <vector>
#include <string>
#include <iostream>

#include <getopt.h>

#include "delay_queue.hh"
#include "util.hh"
#include "ezio.hh"
#include "packetshell.cc"

using namespace std;

void usage_error( const string & program_name )
{
    cerr << "Usage: " + program_name + " [OPTION]" << endl;
    cerr << endl;
    cerr << "Options = --trace-file=FILENAME" << endl;
    cerr << "Trace format: timestamp,delay(half rtt)" << endl;

    throw runtime_error( "invalid arguments" );
}

int main( int argc, char *argv[] )
{
    try {
        const bool passthrough_until_signal = getenv( "MAHIMAHI_PASSTHROUGH_UNTIL_SIGNAL" );

        /* clear environment while running as root */
        char ** const user_environment = environ;
        environ = nullptr;

        check_requirements( argc, argv );

        if ( argc < 2 ) {
            usage_error( argv[ 0 ] );
        }

        const option command_line_options[] = {
            { "trace-file",           required_argument, nullptr, 't' },
            { 0,                                      0, nullptr, 0 }
        };

        string trace_file;

        while ( true ) {
            const int opt = getopt_long( argc, argv, "t:", command_line_options, nullptr );
            if ( opt == -1 ) { /* end of options */
                break;
            }

            switch ( opt ) {
            case 't':
                trace_file = optarg;
                break;
            case '?':
            default:
                throw runtime_error( "getopt_long: unexpected return value " + to_string( opt ) );
            }
        }

        vector< string > command;

        if ( argc == 2 ) {
            command.push_back( shell_path() );
        } else {
            for ( int i = 2; i < argc; i++ ) {
                command.push_back( argv[ i ] );
            }
        }

        PacketShell<DelayTraceQueue> delay_shell_app( "delay", user_environment, passthrough_until_signal );

        delay_shell_app.start_uplink( "[delay trace-file=" + trace_file +"] ",
                                      command,
                                      trace_file );
        delay_shell_app.start_downlink( trace_file );
        return delay_shell_app.wait_for_exit();
    } catch ( const exception & e ) {
        print_exception( e );
        return EXIT_FAILURE;
    }
}
