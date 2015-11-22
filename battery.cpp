#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <array>

#include <getopt.h>
#include <stdlib.h>

#define ST_CHARGING ""
#define ST_DISCHARGING " "
#define ST_PLUG ""

struct BatteryData {
  std::string status = " ";
  int full_design = -1;
  int remaining = -1;
  int present_rate= -1;
  int voltage = -1;
  bool watt_as_unit = false;
};

void parseBattery( const std::string& path, BatteryData& data )
{
  std::ifstream file(path);
  if( file.is_open() )
    {
      std::string line;
      auto to_int = [&line](const size_t delim){
        return atoi( line.substr(delim+1).c_str() );
      };

      auto begins_with = [&line]( const char* txt, const size_t end ){
        return line.compare(0,end,txt) == 0;
      };
      
      while( std::getline( file, line ) )
        {
          // Based on the parser in i3status print_battery_info.c
          const size_t delim = line.find('=');
          if( begins_with("POWER_SUPPLY_ENERGY_NOW",delim) ) {
            data.watt_as_unit = true;
            data.remaining = to_int(delim);
          } else if( begins_with("POWER_SUPPLY_CHARGE_NOW", delim) ){
            data.watt_as_unit = false;
            data.remaining = to_int( delim );
          } else if( begins_with( "POWER_SUPPLY_CURRENT_NOW", delim) )
            data.present_rate = std::abs( to_int( delim ) );
          else if( begins_with( "POWER_SUPPLY_VOLTAGE_NOW", delim) )
            data.voltage = std::abs( to_int( delim ) );
        
          /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
           * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
           * unit instead of μAh. We will calculate it as we need it
           * later. */
          else if (begins_with("POWER_SUPPLY_POWER_NOW", delim ))
            data.present_rate = abs( to_int(delim) );
          else if (begins_with("POWER_SUPPLY_STATUS", delim )) {
            const std::string& status = line.substr(delim+1);
            if( status == "Charging" ) {
              data.status = ST_CHARGING;
            } else if( status == "Discharging" ) {
              data.status = ST_DISCHARGING;
            } else {
              data.status = ST_PLUG;
            }
          } else if (begins_with("POWER_SUPPLY_ENERGY_FULL_DESIGN", delim)){
            data.full_design = to_int( delim );
          }
        }
      file.close();
    }
}

void convertToMilliWattHour( BatteryData& data )
{
  // From print_battery_info.c in i3status
  data.present_rate = ((static_cast<double>(data.voltage) / 1000.0) *
                       (static_cast<double>(data.present_rate) / 1000.0));

  if( data.voltage != -1) {

    data.remaining = ((static_cast<double>(data.voltage) / 1000.0) *
                      (static_cast<double>(data.remaining) / 1000.0));

    data.full_design = ((static_cast<double>(data.voltage) / 1000.0) *
                        (static_cast<double>(data.full_design) / 1000.0));
  }
}


void computeTime( const BatteryData& data, std::stringstream& ss )
{
  double remaining_time;

  if( data.status == ST_CHARGING) {
    remaining_time = static_cast<double>(data.full_design) -
      static_cast<double>(data.remaining)/ static_cast<double>(data.present_rate);
  } else if ( data.status == ST_DISCHARGING ) {
    remaining_time = static_cast<double>(data.remaining) /
      static_cast<double>(data.present_rate);
  } else {
    remaining_time = 0.0;
  }
    
  const int seconds_remaining = static_cast<int>( remaining_time * 3600.0 );
  const int hours = seconds_remaining / 3600;
  const int seconds = seconds_remaining - (hours * 3600);
  const int minutes = seconds / 60;

  ss<<(hours<10?"0":"")<<hours<<":"<<(minutes<10?"0":"")<<minutes;
}

struct Parameters 
{
  int type = 0;
  std::string path = "/sys/class/power_supply/BAT0";
};

bool parseArgs( const int argc, char* const* argv, Parameters& params )
{
  struct option long_options[] =
    {
      {"type",  required_argument, 0, 't'},
      {"help",  no_argument, 0, 'h'},
      {0, 0, 0, 0}
    };

  int option_index = 0;
  int c;
  while( true ) {
    c = getopt_long (argc, argv, "t:h", long_options, &option_index);
    
    /* Detect the end of the options. */
    if (c == -1)
      break;
      
    switch(c) {
    case 't':
      {
        std::string arg(optarg);
        if( arg == "heart") {
          params.type=1;
        } else {
          params.type=0;
        }
      }
      break;
    case 'h':
      std::cout<<"Usage: "<<argv[0]<<" [OPTIONS]...\n"
               <<R"(Options:
  -t, --type [heart/battery]  Specify what icons to use to indicate 
                              the battery status.
  -h, --help                  Print this message and then exit.
Author:
  Fredrik "PlaTFooT" Salomonsson
)";
      return false;
      break;
    }
  }
  return true;
}

int main( int argc, char** argv )
{
  const double threshold = 10.0;
  Parameters params;

  if( !parseArgs(argc,argv,params) )
    return 0;
    
  BatteryData data;
  parseBattery( params.path+"/uevent", data );
  
  /* The difference between POWER_SUPPLY_ENERGY_NOW and
   * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
   * given in mWh, the charge in mAh. So calculate every value given in
   * ampere to watt */
  if( !data.watt_as_unit )
    convertToMilliWattHour(data);
  if( (data.full_design == -1) || (data.remaining == -1 ) ){
    std::cout<<"  "<<std::endl;
    return 1;
  }
  
  const double percentage = static_cast<double>(data.remaining)/
    static_cast<double>(data.full_design)*100.0;

  std::stringstream ss;


  auto gen_battery_icon = [&ss,percentage](const std::array<std::string,5>& icons ){
    if( percentage >= 95.0 ) {
      ss<<icons[0];
    } else if( percentage >= 75.0 ) {
      ss<<icons[1];
    } else if( percentage >= 50.0 ) {
      ss<<icons[2];
    } else if( percentage >= 25.0 ) {
      ss<<icons[3];
    } else {
      ss<<icons[4];
    }
  };
  std::string block_button;
  {
    char* block_buffer = getenv("BLOCK_BUTTON");
    block_button = block_buffer != NULL ? block_buffer : "";
  }
  if( block_button.empty() ) {
    ss<<data.status<<" ";
    if( params.type == 1 ) {
      gen_battery_icon( std::array<std::string,5>{ "", "", "",
            "", "" } );
    } else {
      gen_battery_icon( std::array<std::string,5>{ "", "", "", "", ""} );
    }
    ss<<"  ";
  } else {
    ss<<static_cast<size_t>(percentage)<<"% ";
  }

  // Compute time of discharge/charge
  if( data.present_rate > 0 ){
    computeTime( data, ss );
  }

  // Output the format i3blocks want
  std::cout<<ss.str()<<std::endl;
  std::cout<<ss.str()<<std::endl;

  if( percentage <= threshold )
    std::cout<<"#FF0000"<<std::endl;
  
  return 0;
}
