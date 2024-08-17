/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <vector>
#include <string>
#include <iostream>

#include <getopt.h>

#include "loss_queue.hh"
#include "util.hh"
#include "ezio.hh"
#include "packetshell.cc"

using namespace std;

void usage_error( const string & program_name )
{
    cerr << "Usage: " + program_name + " downlink|uplink [OPTION]" << endl;
    cerr << endl;
    cerr << "Options = --trace-file=FILENAME (required)" << endl;
    cerr << "Options = --configure-file=FILENAME (optional)" << endl;
    cerr << "Trace format: timestamp,delay(half rtt)" << endl;
    cerr << "Configure file: the webrtc server will save the ip to the file" << endl;

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

        if ( argc < 3 ) {
            usage_error( argv[ 0 ] );
        }

        const option command_line_options[] = {
            { "trace-file",           required_argument, nullptr, 't' },
            { "configure-file",       optional_argument, nullptr, 'c' },
            { 0,                                      0, nullptr, 0 }
        };

        string link = argv[ 1 ];
        string trace_file;
        string configure_file;
        bool drop_uplink = false, drop_downlink = false;

        while ( true ) {
            const int opt = getopt_long( argc, argv, "t:c:", command_line_options, nullptr );
            if ( opt == -1 ) { /* end of options */
                break;
            }

            switch ( opt ) {
            case 't':
                trace_file = optarg;
                break;
            case 'c':
                configure_file = optarg;
                break;
            case '?':
                usage_error( argv[ 0 ] );
                break;
            default:
                throw runtime_error( "getopt_long: unexpected return value " + to_string( opt ) );
            }
        }

        if ( link != "uplink" and link != "downlink" ) {
            throw runtime_error( "unexpected link value: " +  link );
        }

        if ( link == "uplink" ) {
            drop_uplink = true;
        } else {
            drop_downlink = true;
        }

        vector<string> command;

        if ( argc <= 4 ) {
            command.push_back( shell_path() );
        } else {
            for ( int i = 4; i < argc; i++ ) {
                command.push_back( argv[ i ] );
            }
        }

        PacketShell<TraceLoss> loss_app( "loss", user_environment, passthrough_until_signal );

        if ( argc == 4) {
            FILE * file = fopen( configure_file.c_str(), "w" );
            fprintf( file, "%s\n", loss_app.egress_addr_pub().ip().c_str() );
            fclose( file );
        }

        string shell_prefix = "[loss ";
        if ( link == "uplink" ) {
            shell_prefix += "up=";
        } else {
            shell_prefix += "down=";
        }

        shell_prefix += "trace-file=" + trace_file + "] ";

        loss_app.start_uplink( shell_prefix,
                               command,
                               drop_uplink, trace_file);
        loss_app.start_downlink(drop_downlink, trace_file);
        return loss_app.wait_for_exit();
    } catch ( const exception & e ) {
        print_exception( e );
        return EXIT_FAILURE;
    }
}
