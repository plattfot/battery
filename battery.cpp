#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cmath> // fabs
#include <array>
#include <vector>
#include <memory>
#include <regex>

#include <getopt.h>
#include <stdlib.h>
#include <dirent.h>

enum Status : size_t {
  ST_CHARGING = 0,
  ST_PLUG = 1,
  ST_DISCHARGING = 2
};

std::string STATUS_ICONS[] = {
  "",
  "",
  " "
};

struct BatteryData {
  Status status = ST_DISCHARGING;
  double full_design = -1.0;
  double remaining = -1.0;
  double present_rate= -1.0;
  double voltage = -1.0;
  bool watt_as_unit = false;
};

typedef std::vector<BatteryData> BatteryVectorT;
typedef std::array<std::string,5> ArrayT;

struct Parameters 
{
  std::string path = "/sys/class/power_supply";
  ArrayT icons = ArrayT{ "", "", "", "", "" };
  long int battery = -1; // -1 sum all.
};

size_t countBatteries(const std::string& path) 
{
  DIR* dir;
  struct dirent *ent;

  size_t count = 0;
  if( (dir = opendir( path.c_str() )) != NULL ) {
    std::string file;
    std::regex exp("BAT[0-9]+");
    
    while( ( ent = readdir(dir) ) != NULL ) {
      count += std::regex_match( ent->d_name, exp );
    }
    closedir(dir);
  }
  return count;
}

void parseBattery( const std::string& path, BatteryData& data )
{
  std::ifstream file(path);
  if( file.is_open() )
    {
      std::string line;
      auto to_double = [&line](const size_t delim){
        return static_cast<double>( atoi( line.substr(delim+1).c_str() ) );
      };

      auto begins_with = [&line]( const char* txt, const size_t end ){
        return line.compare(0,end,txt) == 0;
      };
      
      while( std::getline( file, line ) ) {
        // Based on the parser in i3status print_battery_info.c
        const size_t delim = line.find('=');
        if( begins_with("POWER_SUPPLY_ENERGY_NOW",delim) ) {
          data.watt_as_unit = true;
          data.remaining = to_double(delim);
        } else if( begins_with("POWER_SUPPLY_CHARGE_NOW", delim) ){
          data.watt_as_unit = false;
          data.remaining = to_double( delim );
        } else if( begins_with( "POWER_SUPPLY_CURRENT_NOW", delim) )
          data.present_rate = std::fabs( to_double( delim ) );
        else if( begins_with( "POWER_SUPPLY_VOLTAGE_NOW", delim) )
          data.voltage = std::fabs( to_double( delim ) );
        
        /* on some systems POWER_SUPPLY_POWER_NOW does not exist, but actually
         * it is the same as POWER_SUPPLY_CURRENT_NOW but with μWh as
         * unit instead of μAh. We will calculate it as we need it
         * later. */
        else if (begins_with("POWER_SUPPLY_POWER_NOW", delim ))
          data.present_rate = abs( to_double(delim) );
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
          data.full_design = to_double( delim );
        }
      }
      file.close();
    }
}

void convertTo_mWh( BatteryData& data )
{
  // From print_battery_info.c in i3status
  data.present_rate = (data.voltage / 1000.0) * (data.present_rate / 1000.0);

  if( data.voltage != -1) {
    data.remaining = (data.voltage / 1000.0) * (data.remaining / 1000.0);
    data.full_design = (data.voltage / 1000.0) * (data.full_design / 1000.0);
  }
}

double computePercentage( const BatteryVectorT& batteries, 
                          const Parameters& params)
{
  if( params.battery == -1 ){
    // Compute the total percentage
    double full_design = 0.0;
    double remaining = 0.0;
    for( const auto& data : batteries ){
      full_design += data.full_design; 
      remaining += data.remaining; 
    }
    return remaining / full_design * 100.0; 
  } else {
    const auto& data = batteries[params.battery];
    return data.remaining / data.full_design * 100.0;
  }
}

/// Computes the time, in seconds, until battery is depleted or fully charged.
size_t secondsRemaining( const BatteryData& data )
{
  double remaining_time;

  if( data.status == ST_CHARGING) {
    remaining_time = (data.full_design - data.remaining) / data.present_rate;
  } else if ( data.status == ST_DISCHARGING ) {
    remaining_time = data.remaining / data.present_rate;
  } else {
    remaining_time = 0.0;
  }
    
  return static_cast<size_t>( remaining_time * 3600.0 );
}

void computeTime( const BatteryVectorT& batteries,
                  const Parameters& params,
                  std::stringstream& ss )
{
  size_t seconds_remaining = 0;
  if( params.battery == -1 ){
    // Compute the total percentage
    for( const auto& data : batteries ){
      seconds_remaining += secondsRemaining( data );
    }
  } else {
    seconds_remaining += secondsRemaining( batteries[params.battery] );
  }

  if(seconds_remaining > 0.0 ) {
    const size_t hours = seconds_remaining / 3600;
    const size_t seconds = seconds_remaining - (hours * 3600);
    const size_t minutes = seconds / 60;
    ss<<(hours<10?"0":"")<<hours<<":"<<(minutes<10?"0":"")<<minutes;
  } else {
    ss<<"Full";
  }
}

void batteryStatus( const BatteryVectorT& batteries,
                    const Parameters& params,
                    std::stringstream& ss )
{
  if( params.battery == -1 ){
    Status status = ST_DISCHARGING;
    // Compute the total percentage
    for( const auto& data : batteries ){
      status = std::min(status, data.status);
    }
    ss<<STATUS_ICONS[ status ];
  } else {
    ss<<STATUS_ICONS[ batteries[params.battery].status ];
  }
}


bool parseArgs( const int argc, char* const* argv, Parameters& params )
{
  struct option long_options[] =
    {
      {"type",  required_argument, 0, 't'},
      {"custom",  required_argument, 0, 'c'},
      {"help",  no_argument, 0, 'h'},
      {"battery",  no_argument, 0, 'b'},
      {0, 0, 0, 0}
    };
  static const std::string help_text = R"(
Options:
  -t, --type [TYPE]         Specify what icons to use to indicate 
                            the battery status. Select between battery or
                            heart. Battery is the default.
  -c, --custom [FULL,EMPTY] Use custom battery indicator, using a 
                            combination  of characters FULL and EMPTY. 
  -b, --battery [-1,0,...]  Specify what battery to monitor. -1 combines
                            all the batteries and is the default.
  -h, --help                Print this message and then exit.
Author:
  Fredrik "PlaTFooT" Salomonsson
)";
  int option_index = 0;
  int c;
  while( true ) {
    c = getopt_long (argc, argv, "t:c:b:h", long_options, &option_index);
    
    /* Detect the end of the options. */
    if (c == -1)
      break;
      
    switch(c) {
    case 't':
      {
        std::string arg(optarg);
        if( arg == "heart") {
          params.icons = 
            {"", "", "", "", ""};
        } 
      }
      break;
    case 'c':
      {
        const std::string arg = optarg;
        const size_t delim = arg.find(',');

        if( delim == std::string::npos ) {
          std::cerr<<"[ERROR] Need to delimit FULL and EMPTY with a ','"<<std::endl;
          return false;
        }
        const std::string full = arg.substr(0,delim);
        const std::string empty = arg.substr(delim+1);
        params.icons = 
          { full  + full  + full  + full,
            full  + full  + full  + empty,
            full  + full  + empty + empty,
            full  + empty + empty + empty,
            empty + empty + empty + empty };
      }
      break;
    case 'b':
      // Using strol for more robust conversion than atoi
      params.battery = strtol(optarg,NULL,10);
      break;
    case 'h':
      std::cout<<"Usage: "<<argv[0]<<" [OPTIONS]..."<<help_text;
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

  const long int num_batts = countBatteries( params.path );

  // Make sure battery index is within bounds
  params.battery = std::min( params.battery, num_batts - 1 );

  std::vector<BatteryData> batteries(num_batts);
  
  for( size_t i = 0; i < num_batts; ++i ) {
    BatteryData& data = batteries[i];

    std::stringstream batt_path;
    batt_path<<params.path<<"/BAT"<<i<<"/uevent";
    parseBattery( batt_path.str(), data );
  
    /* The difference between POWER_SUPPLY_ENERGY_NOW and
     * POWER_SUPPLY_CHARGE_NOW is the unit of measurement. The energy is
     * given in mWh, the charge in mAh. So calculate every value given
     * in ampere  to watt */
    if( !data.watt_as_unit )
      convertTo_mWh(data);
  
    if( (data.full_design < 0.0) || (data.remaining < 0.0 ) ){
      std::cout<<"  "<<std::endl;
      return 1;
    }
  }

  const double percentage = computePercentage( batteries, params );

  std::stringstream ss;
  auto gen_battery_icon = [&ss,percentage]( const ArrayT& icons ){
    ss<<" ";
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
    ss<<"  ";
  };
  
  std::string block_button;
  {
    char* block_buffer = getenv("BLOCK_BUTTON");
    block_button = block_buffer != NULL ? block_buffer : "";
  }
  
  batteryStatus( batteries, params, ss );

  if( block_button.empty() ) {
    gen_battery_icon( params.icons );
  } else {
    ss<<static_cast<size_t>(percentage)<<"% ";
  }
  
  // Compute time of discharge/charge
  computeTime( batteries, params, ss );

  // Output the format i3blocks want
  std::cout<<ss.str()<<std::endl;
  std::cout<<ss.str()<<std::endl;

  if( percentage <= threshold )
    std::cout<<"#FF0000"<<std::endl;
  
  return 0;
}
